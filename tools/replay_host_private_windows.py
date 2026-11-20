#!/usr/bin/env python3
"""Producer/consumer private-DLL acceptance with real AWK and an immutable public SDK.

Python, LLVM, module snapshots and the deliberately hostile OS-only DLL are test-host tools.
The fixture is local-only; it does not change a public SDK, bundle, publisher pin or system policy.
"""
import argparse
import ctypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import create_host_tools as host_tools
import crt_pe
import crt_sdk_install_metadata as metadata
import crt_windows_external as sandbox
import crt_windows_trace as trace
from replay_host_environment_windows import readonly_fixture

ROOT = Path(__file__).resolve().parents[1]
AWK_SOURCES = ('awkgram.tab.c', 'b.c', 'lex.c', 'lib.c', 'main.c', 'parse.c', 'proctab.c', 'run.c', 'tran.c')
PROGRAM = 'BEGIN { print 6*7; print sqrt(81); for(i=0;i<10000000;i++) n+=i }'
EXPECTED = b'42\n9\n'


def digest(path):
    return host_tools.digest(path)


def expand(recipe, values, lists):
    result = []
    for item in recipe:
        if item in lists:
            result.extend(map(str, lists[item])); continue
        for key, value in values.items(): item = item.replace(key, str(value))
        if '@' in item: raise ValueError('unexpanded native-policy token: ' + item)
        result.append(item)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('sdk-root', 'contract', 'cli', 'clang', 'clangxx', 'linker', 'os-sdk', 'output', 'work-parent'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--contract-sha256', required=True)
    parser.add_argument('--hide', action='append', type=Path, default=[])
    args = parser.parse_args()
    if host_tools.native_host() != {'os': 'windows', 'arch': 'x86_64'}:
        parser.error('native Windows/x64 is required')
    sdk = args.sdk_root.resolve(strict=True)
    contract = json.loads(args.contract.read_text(encoding='utf-8'))
    if digest(args.contract) != args.contract_sha256 or contract['sdk_inventory'] != metadata.inventory(sdk):
        parser.error('SDK/portable contract differs from the supplied pin')
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=False)
    args.work_parent.mkdir(parents=True, exist_ok=True)
    producer, _ = sandbox.create_work(args.work_parent / 'producer')
    work, sid = sandbox.create_work(args.work_parent / 'consumer')
    (work / 'home').mkdir(); (work / 'tmp').mkdir()
    cli = work / 'crt-cli.exe'; shutil.copy2(args.cli, cli)
    source = producer / 'awk-source'; shutil.copytree(ROOT / 'shell/awk/src', source)
    source_hashes = {p.name: digest(p) for p in source.iterdir() if p.is_file()}
    original = metadata.inventory(sdk)
    results, commands, snapshots, bundle_rows = [], [], [], []
    completed = False
    baseline = {}
    (output / 'driver.py').write_bytes(Path(__file__).read_bytes())
    producer_identity = {'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                         'working_tree_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT)),
                         'driver_sha256': digest(Path(__file__)),
                         'helper_sha256': {name: digest(ROOT / 'tools' / name) for name in
                             ('create_host_tools.py', 'crt_host_tools_pe.py', 'crt_windows_trace.py', 'crt_windows_modules.py')}}
    external_tools = {name: {'path': str(path.resolve()), 'sha256': digest(path)} for name, path in
                      (('clang', args.clang), ('clangxx', args.clangxx), ('linker', args.linker))}
    kernel, _ = sandbox.api()
    kernel.SetErrorMode.argtypes, kernel.SetErrorMode.restype = [ctypes.c_uint], ctypes.c_uint
    previous_error_mode = kernel.SetErrorMode(0x8003)

    def save():
        receipt = {'passed': completed and bool(results) and all(r['passed'] for r in results), 'completed': completed,
                   'cases': results, 'consumer_work': str(work), 'producer_work': str(producer),
                   'cli_sha256': digest(cli), 'contract_sha256': args.contract_sha256,
                   'sdk_manifest_sha256': digest(sdk / 'manifest.json'), 'sdk_content_sha256': contract['sdk_content_sha256'],
                   'source_hashes': source_hashes, 'bundles': bundle_rows,
                   'producer': producer_identity, 'external_tools': external_tools, 'os_sdk': str(args.os_sdk),
                   'isolation': 'restricted token; selected originals denied; job-object accounting, not network/exec denial'}
        (output / 'result.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
        (output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
        (output / 'modules.json').write_text(json.dumps(snapshots, indent=2) + '\n', encoding='utf-8')

    def record(name, passed, detail=''):
        results.append({'case': name, 'passed': bool(passed), 'detail': str(detail)[-1800:]})
        print(('PASS ' if passed else 'FAIL ') + name, flush=True); save()

    def build(name, argv):
        done = subprocess.run(list(map(str, argv)), capture_output=True, stdin=subprocess.DEVNULL,
                              timeout=180, creationflags=subprocess.CREATE_NO_WINDOW)
        (output / (name + '.stdout')).write_bytes(done.stdout); (output / (name + '.stderr')).write_bytes(done.stderr)
        commands.append({'case': name, 'scope': 'producer/test-host', 'argv': list(map(str, argv)), 'exit_code': done.returncode})
        save()
        if done.returncode: raise ValueError(name + ': ' + done.stderr.decode('utf-8', 'replace')[-2000:])
        return done.stdout

    env = {'HOME': str(work / 'home'), 'USERPROFILE': str(work / 'home'), 'PATH': '',
           'SystemRoot': os.environ['SystemRoot'], 'TEMP': str(work / 'tmp'), 'TMP': str(work / 'tmp'), 'LC_ALL': 'C'}
    permitted = {cli.resolve(), (work / 'probe.exe').resolve(), Path(os.environ['SystemRoot'], 'System32/conhost.exe').resolve()}

    def run(name, argv, *, cwd=None, environment=None, capture=None):
        status = trace.run(list(map(str, argv)), work=work, sid=sid, env=environment or env, cwd=cwd or work,
                           stdout=output / (name + '.stdout'), stderr=output / (name + '.stderr'), timeout=60,
                           module_images=capture)
        status['undeclared_images'] = [p for p in status['processes'] if not p['image'] or Path(p['image']).resolve() not in permitted]
        commands.append({'case': name, 'scope': 'restricted consumer', 'argv': list(map(str, argv)), **status})
        for row in status.get('module_snapshots', []): snapshots.append(dict(row, case=name))
        save()
        if status['timed_out'] or status['undeclared_images']:
            raise ValueError(name + ': timeout or undeclared process image: ' + str(status))
        return status, (output / (name + '.stdout')).read_bytes()

    try:
        resource = build('clang-resource', [args.clang, '-print-resource-dir']).decode().strip()
        values = {'@SDK@': sdk, '@RESOURCE@': resource, '@OS_SDK@': args.os_sdk}
        objects = []
        shared = contract['modes']['executable-shared']
        for name in AWK_SOURCES:
            obj = producer / (name + '.o')
            build('compile-' + name, [args.clang, *expand(shared['compile_c'],
                  dict(values, **{'@SOURCE@': source / name, '@OUTPUT@': obj}),
                  {'@EXTRA@': ['-std=c99', '-O1', '-I' + str(source)]})]); objects.append(obj)
        linked = producer / 'awk-shared.exe'
        # The public C link policy remains static. Use its separately qualified shared C++ link policy for these C objects.
        build('link-actual-shared-AWK', [args.clangxx, *expand(shared['link_cxx'], dict(values, **{'@OUTPUT@': linked}),
              {'@OBJECTS@': objects, '@LIBRARIES@': []}), '--ld-path=' + str(args.linker)])
        record('actual AWK imports private libc and libm', {'libc.dll', 'libm.dll'} <= set(crt_pe.imported_libraries(linked)))

        probe_source = producer / 'probe.c'
        probe_source.write_text('#include <stdio.h>\nint main(int c,char **v){for(int i=1;i<c;i++){FILE *f=fopen(v[i],"rb");if(f){fclose(f);return 10;}}return 0;}\n')
        probe_obj, probe = producer / 'probe.o', producer / 'probe.exe'
        static = contract['modes']['executable-static']
        build('compile-visibility-probe', [args.clang, *expand(static['compile_c'], dict(values, **{'@SOURCE@': probe_source, '@OUTPUT@': probe_obj}), {'@EXTRA@': ['-std=c99', '-O1']})])
        build('link-visibility-probe', [args.clang, *expand(static['link_c'], dict(values, **{'@OUTPUT@': probe}), {'@OBJECTS@': [probe_obj], '@LIBRARIES@': []}), '--ld-path=' + str(args.linker)])
        shutil.copy2(probe, work / 'probe.exe')
        hidden = [sdk / 'manifest.json', sdk / 'bin/libc.dll', sdk / 'bin/libm.dll', args.contract.resolve(),
                  ROOT / 'CMakeLists.txt', ROOT / 'shell/awk/src/main.c', args.cli.resolve(), linked]
        for path in args.hide:
            path = path.resolve(strict=True)
            hidden.append(next(p for p in path.rglob('*') if p.is_file()) if path.is_dir() else path)
        assert all(p.is_file() and not p.is_relative_to(work) for p in hidden)
        status, _ = run('original-inputs-hidden', [work / 'probe.exe', *hidden])
        record('consumer cannot read originals or producer build', status['exit_code'] == 0)

        view = producer / 'sdk-fixture'; shutil.copytree(sdk, view, symlinks=True)
        data = json.loads((view / 'manifest.json').read_text(encoding='utf-8'))
        data['host_tools_test_fixture'] = {'kind': 'actual-AWK-shared-CXX-link-policy', 'parent_manifest_sha256': digest(sdk / 'manifest.json'),
                                           'parent_content_sha256': contract['sdk_content_sha256'], 'source_hashes': source_hashes}
        (view / 'manifest.json').chmod(0o600); (view / 'manifest.json').write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
        (view / 'VERSION').chmod(0o600); (view / 'VERSION').write_text('private-dll-fixture.20261120.win-x64\n')
        for relative in ('system/bin/awk.exe', 'system/bin/awk', 'bin/awk.exe', 'bin/awk', 'usr/bin/awk.exe', 'usr/bin/awk'):
            path = view / relative; path.chmod(0o600); shutil.copyfile(linked, path)
        produced = producer / 'bundle'
        manifest = host_tools.assemble(view, produced, 'private-dll-fixture.20261120.win-x64')
        host_tools.verify_bundle(produced)
        private = [path for path in manifest['dependencies'] if path.lower().endswith('.dll')]
        record('producer closes actual native private DLL graph', {'system/bin/libc.dll', 'system/bin/libm.dll'} <= set(private))
        (output / 'fixture-manifest.json').write_bytes((produced / 'manifest.json').read_bytes())

        # A working hostile DLL exports the real libm names but never reaches the AWK program.
        poison_source = producer / 'poison.c'
        poison_source.write_text('''typedef unsigned long D; typedef unsigned short W; typedef void *H;
__declspec(dllimport) D __stdcall GetEnvironmentVariableW(const W*,W*,D);
__declspec(dllimport) H __stdcall CreateFileW(const W*,D,D,void*,D,D,H);
__declspec(dllimport) int __stdcall WriteFile(H,const void*,D,D*,void*);
__declspec(dllimport) int __stdcall CloseHandle(H);
__declspec(dllimport) void __stdcall ExitProcess(D);
void poison_stub(void){} static W path[32768];
int __stdcall poison_entry(void *m,D reason,void *r){if(reason==1){D n=0;GetEnvironmentVariableW((const W*)L"CRT_PRIVATE_DLL_POISON_MARKER",path,32768);H h=CreateFileW(path,0x40000000,3,0,2,0x80,0);WriteFile(h,"poison",6,&n,0);CloseHandle(h);ExitProcess(97);}return 1;}
''')
        exports = crt_pe.exported_symbols(sdk / 'bin/libm.dll')
        if any(name.startswith('ordinal:') for name in exports): raise ValueError('poison control requires named exports')
        definition = producer / 'poison.def'; definition.write_text('LIBRARY libm.dll\nEXPORTS\n' + ''.join('  ' + name + '=poison_stub\n' for name in exports))
        poison_dll = producer / 'poison.dll'
        build('build-working-OS-only-poison', [args.clang, '--target=x86_64-pc-windows-msvc', '-O1', '-ffreestanding', '-fno-builtin',
              '-fno-stack-protector', '-nostdlib', '-shared', poison_source, '-o', poison_dll, '-Wl,/entry:poison_entry',
              '-Wl,/def:' + str(definition), '-Wl,/libpath:' + str(args.os_sdk), '-lkernel32', '--ld-path=' + str(args.linker)])
        record('poison is an OS-only test control', set(crt_pe.imported_libraries(poison_dll)) == {'KERNEL32.dll'})
        poison = work / 'hostile cwd'; poison.mkdir(); shutil.copy2(poison_dll, poison / 'libm.dll')
        clean = work / 'clean cwd'; clean.mkdir()
        marker = work / 'poison-loaded'
        hostile = dict(env, PATH=str(poison), CRT_PRIVATE_DLL_POISON_MARKER=str(marker))

        for index in (1, 2):
            relocation = work / ('relocation %d 한글' % index); relocation.mkdir()
            bundle = relocation / 'host tools'; shutil.copytree(produced, bundle, symlinks=True)
            readonly_fixture(bundle, sid)
            pin = digest(bundle / 'manifest.json'); baseline[str(bundle)] = host_tools.payload_inventory(bundle)
            bundle_rows.append({'root': str(bundle), 'manifest_sha256': pin, 'content_sha256': manifest['content_sha256']})
            permitted.update((bundle / path).resolve() for path in manifest['executable_files'])
            awk_paths = [manifest['tools']['awk']] + [a['path'] for a in manifest['aliases'] if a['role'] == 'awk']
            captured = {(bundle / path).resolve(): [Path(p).name for p in private] for path in awk_paths}
            direct_env = dict(env, CRT_ROOTFS=str(bundle))
            status, stdout = run('initial-no-PATH-%d' % index, [bundle / manifest['tools']['awk'], PROGRAM], environment=direct_env, capture=captured)
            record('relocation %d initializes DLLs without PATH' % index, status['exit_code'] == 0 and stdout.replace(b'\r\n', b'\n') == EXPECTED)
            for relative in ('bin/awk.exe', 'usr/bin/awk'):
                label = 'alias-%d-%s' % (index, relative.replace('/', '-'))
                status, stdout = run(label, [bundle / relative, PROGRAM], cwd=poison,
                                      environment=dict(hostile, CRT_ROOTFS=str(bundle)), capture=captured)
                record(label + ' uses adjacent DLLs ahead of hostile cwd/PATH', status['exit_code'] == 0 and stdout.replace(b'\r\n', b'\n') == EXPECTED and not marker.exists())
            script = work / ('script-%d.sh' % index)
            script.write_text('"$AWK" \'' + PROGRAM + '\'\n', encoding='utf-8')
            selection = ['--host-tools', bundle, '--host-tools-sha256', pin]
            status, stdout = run('CLI-doctor-%d' % index, [cli, '--json', 'doctor', '--scope', 'environment', *selection], environment=hostile)
            record('CLI accepts relocation %d private closure' % index, status['exit_code'] == 0 and json.loads(stdout).get('ok') is True)
            status, stdout = run('CLI-shell-%d' % index, [cli, '--json', 'shell', *selection, '--work-root', work / ('job-%d' % index),
                                 '--script', script, '--directory', poison, '--timeout', '30'], environment=hostile, capture=captured)
            reply = json.loads(stdout)
            record('CLI shell %d runs shared AWK without poisoning' % index, status['exit_code'] == 0 and reply.get('ok') is True and not marker.exists())
            rows = [row for row in snapshots if row['case'] in {'initial-no-PATH-%d' % index, 'alias-%d-bin-awk.exe' % index,
                    'alias-%d-usr-bin-awk' % index, 'CLI-shell-%d' % index}]
            mapped = len(rows) == 4
            for row in rows:
                if row.get('error'): mapped = False; continue
                image = Path(row['image']).resolve()
                for library in private:
                    expected_path = (image.parent / Path(library).name).resolve()
                    loaded = [Path(p).resolve() for p in row['paths'] if Path(p).name.casefold() == Path(library).name.casefold()]
                    mapped = mapped and loaded == [expected_path] and digest(expected_path) == digest(produced / library)
            record('relocation %d mapped DLL paths and hashes are app-local' % index, mapped, rows)

            if index == 1:
                victim = bundle / 'bin/libm.dll'; original_dll = victim.read_bytes(); mode = victim.stat().st_mode
                victim.chmod(0o600); victim.unlink()
                try:
                    for context, cwd, path_value in [('cwd', poison, str(bundle / 'system/bin')),
                                                     ('PATH', clean, str(poison) + ';' + str(bundle / 'system/bin'))]:
                        marker.unlink(missing_ok=True)
                        status, _ = run('working-poison-' + context, [bundle / 'bin/awk.exe', PROGRAM], cwd=cwd,
                                        environment=dict(hostile, PATH=path_value, CRT_ROOTFS=str(bundle)))
                        record(context + ' poison control actually loads and exits 97', status['exit_code'] == 97 and marker.read_bytes() == b'poison')
                    marker.unlink()
                    refused = work / 'missing-job'
                    status, stdout = run('CLI-missing-alias-DLL', [cli, '--json', 'shell', *selection, '--work-root', refused, '--script', script], environment=hostile)
                    record('CLI refuses missing private alias DLL before job creation', status['exit_code'] != 0 and not refused.exists() and not marker.exists())
                    victim.write_bytes(b'changed private DLL')
                    status, _ = run('CLI-modified-DLL', [cli, '--json', 'doctor', '--scope', 'environment', *selection], environment=hostile)
                    record('CLI refuses a changed private DLL', status['exit_code'] != 0)
                finally:
                    victim.chmod(0o600); victim.write_bytes(original_dll); victim.chmod(mode)
                canonical = bundle / 'system/bin/libm.dll'; saved = canonical.read_bytes(); mode = canonical.stat().st_mode
                canonical.chmod(0o600); canonical.unlink()
                try:
                    status, _ = run('loader-missing-canonical-DLL', [bundle / 'system/bin/awk.exe', PROGRAM], cwd=clean, environment=direct_env)
                    record('native initial loader refuses missing DLL without SDK fallback', status['exit_code'] == 0xC0000135)
                    status, _ = run('CLI-missing-canonical-DLL', [cli, '--json', 'doctor', '--scope', 'environment', *selection], environment=hostile)
                    record('CLI refuses missing canonical DLL', status['exit_code'] != 0)
                finally:
                    canonical.write_bytes(saved); canonical.chmod(mode)
                status, stdout = run('restored-closure', [cli, '--json', 'shell', *selection, '--work-root', work / 'job-restored',
                                     '--script', script, '--directory', poison, '--timeout', '30'], environment=hostile)
                record('restored closure runs without changing the pin', status['exit_code'] == 0 and json.loads(stdout).get('ok') is True and not marker.exists())
            record('relocation %d payload remains unchanged' % index, baseline[str(bundle)] == host_tools.payload_inventory(bundle))

        static_bundle = producer / 'static-regression'
        static_manifest = host_tools.assemble(sdk, static_bundle, 'static-regression.20261120')
        host_tools.verify_bundle(static_bundle)
        record('previous public static payload is byte-identical', static_manifest['content_sha256'] == 'dc106db47ed8bcbb56bdd6da0415dce527653fcb42b0c54bdc1402dca74c35fe')
        record('original SDK and upstream AWK source are unchanged', original == metadata.inventory(sdk) and source_hashes ==
               {p.name: digest(p) for p in (ROOT / 'shell/awk/src').iterdir() if p.is_file()})
        completed = True
    except Exception as error:
        record('replay completed without a harness exception', False, repr(error))
    finally:
        kernel.SetErrorMode(previous_error_mode)
        save()
    print('%d/%d cases passed' % (sum(row['passed'] for row in results), len(results)), flush=True)
    return 0 if completed and results and all(row['passed'] for row in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
