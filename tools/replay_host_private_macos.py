#!/usr/bin/env python3
"""Native private-dylib host-tools replay. Python/Xcode tools are test-host only.

Build the existing AWK import against a pinned public SDK's shared C policy,
assemble a labeled fixture bundle, then consume relocated copies with SDK and
producer access denied. No upstream source or original SDK is edited.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import audit_macos_bindings as audit
import create_host_tools as host_tools
import crt_macho
import crt_sdk_install_metadata as metadata

ROOT = Path(__file__).resolve().parents[1]
AWK_SOURCES = ('awkgram.tab.c', 'b.c', 'lex.c', 'lib.c', 'main.c', 'parse.c', 'proctab.c', 'run.c', 'tran.c')


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def expand(recipe, values, lists):
    result = []
    for item in recipe:
        if item in lists:
            result.extend(map(str, lists[item]))
        else:
            for key, value in values.items():
                item = item.replace(key, str(value))
            result.append(item)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('sdk-root', 'contract', 'cli', 'clang', 'linker', 'output', 'work-parent'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--contract-sha256', required=True)
    parser.add_argument('--hide', type=Path, action='append', default=[])
    args = parser.parse_args()
    assert sys.platform == 'darwin', 'native macOS required'
    sdk = args.sdk_root.resolve()
    assert digest(args.contract) == args.contract_sha256
    contract = json.loads(args.contract.read_text())
    original = metadata.inventory(sdk)
    assert contract['sdk_inventory'] == original and contract['sdk_content_sha256'] == metadata.content_sha256(original)
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=False)
    args.work_parent.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='private dylib 한글 ', dir=args.work_parent.resolve()))
    producer = work / 'producer'; producer.mkdir()
    source = producer / 'awk'; shutil.copytree(ROOT / 'shell/awk/src', source)
    input_hashes = {p.name: digest(p) for p in source.iterdir() if p.is_file()}
    cli = work / 'crt-cli'; shutil.copy2(args.cli, cli)
    home = work / 'home'; home.mkdir()
    poison = work / 'hostile cwd'; poison.mkdir()
    marker = work / 'unexpected-private-library'
    results, commands = [], []
    bundle = None
    completed = False

    def save():
        (out / 'commands.json').write_text(json.dumps(commands, ensure_ascii=False, indent=2) + '\n')
        (out / 'result.json').write_text(json.dumps({'passed': completed and bool(results) and all(r['passed'] for r in results), 'completed': completed,
            'cases': results, 'work': str(work), 'cli_sha256': digest(cli), 'contract_sha256': digest(args.contract),
            'sdk_content_sha256': contract['sdk_content_sha256'], 'source_hashes': input_hashes,
            'bundle': str(bundle) if bundle else None}, ensure_ascii=False, indent=2) + '\n')

    def record(name, passed, detail=''):
        results.append({'case': name, 'passed': bool(passed), 'detail': str(detail)[:1500]})
        print(('PASS ' if passed else 'FAIL ') + name, flush=True)
        save()

    def run(name, argv, **options):
        print('RUN', name, flush=True)
        result = subprocess.run(list(map(str, argv)), capture_output=True, stdin=subprocess.DEVNULL, timeout=120, **options)
        index = len(commands)
        (out / f'{index:03}.stdout').write_bytes(result.stdout)
        (out / f'{index:03}.stderr').write_bytes(result.stderr)
        commands.append({'case': name, 'argv': list(map(str, argv)), 'exit': result.returncode,
                         'stdout': f'{index:03}.stdout', 'stderr': f'{index:03}.stderr'})
        save()
        return result

    def checked(name, argv, **options):
        proc = run(name, argv, **options)
        assert proc.returncode == 0, (name, proc.stderr.decode(errors='replace')[-3000:])
        return proc

    try:
        resource = checked('clang resource directory', [args.clang, '-print-resource-dir']).stdout.decode().strip()
        objects = []
        recipe = contract['modes']['executable-shared']
        values = {'@SDK@': sdk, '@RESOURCE@': resource}
        for name in AWK_SOURCES:
            obj = producer / (name + '.o')
            argv = [args.clang, *expand(recipe['compile_c'], {**values, '@SOURCE@': source/name, '@OUTPUT@': obj},
                {'@EXTRA@': ['-std=c99', '-O1', '-I'+str(source)]})]
            checked('compile AWK '+name, argv); objects.append(obj)
        linked = producer / 'awk-linked'
        checked('link actual AWK with shared CRT', [args.clang, *expand(recipe['link_c'], {**values, '@OUTPUT@': linked},
            {'@OBJECTS@': objects, '@LIBRARIES@': []}), '--ld-path='+str(args.linker)])
        record('actual AWK imports private CRT dylibs', '@rpath/libc.dylib' in crt_macho.dependencies(linked))
        view = producer / 'sdk-fixture'; shutil.copytree(sdk, view, symlinks=True)
        view_manifest = json.loads((view/'manifest.json').read_text())
        view_manifest['host_tools_test_fixture'] = {'kind': 'actual-shared-AWK', 'parent_manifest_sha256': digest(sdk/'manifest.json'),
            'parent_content_sha256': contract['sdk_content_sha256'], 'awk_source_hashes': input_hashes}
        (view/'manifest.json').write_text(json.dumps(view_manifest, indent=2)+'\n')
        (view/'VERSION').write_text('private-dylib-fixture.20261106\n')
        awk = view/'system/bin/awk'; awk.unlink(); shutil.copy2(linked, awk)
        def sync_aliases():
            for relative in ('bin/awk', 'usr/bin/awk'):
                path = view/relative
                if not path.is_symlink():
                    shutil.copy2(awk, path)
        sync_aliases()
        rejected = producer/'rejected-absolute-rpath'
        try:
            host_tools.assemble(view, rejected, 'rejected-test')
        except ValueError as error:
            record('assembler refuses producer SDK RPATH before publication', 'runtime search path' in str(error) and not rejected.exists(), error)
        else:
            record('assembler refuses producer SDK RPATH before publication', False)
        checked('relocate AWK runtime path', ['/usr/bin/install_name_tool', '-rpath', str(sdk/'lib'), '@loader_path', awk])
        checked('ad-hoc sign relocated AWK', ['/usr/bin/codesign', '--force', '--sign', '-', awk])
        safe_awk = producer/'awk-relative'; shutil.copy2(awk, safe_awk)
        checked('inject absolute dependency negative fixture', ['/usr/bin/install_name_tool', '-change', '@rpath/libdl.dylib', str(sdk/'lib/libdl.dylib'), awk])
        checked('sign absolute-load negative fixture', ['/usr/bin/codesign', '--force', '--sign', '-', awk])
        sync_aliases()
        rejected = producer/'rejected-absolute-load'
        try:
            host_tools.assemble(view, rejected, 'rejected-test')
        except ValueError as error:
            record('assembler refuses absolute private load names', 'non-relocatable' in str(error) and not rejected.exists(), error)
        else:
            record('assembler refuses absolute private load names', False)
        awk.unlink(); shutil.copy2(safe_awk, awk)
        # Some SDK aliases are copies, not links; all AWK aliases use the new bytes.
        sync_aliases()
        produced = producer/'bundle'
        manifest = host_tools.assemble(view, produced, 'private-dylib-fixture.20261106')
        private = [p for p in manifest['dependencies'] if p.endswith('.dylib')]
        record('producer copies the transitive private closure', {'system/bin/libc.dylib','system/bin/libm.dylib','system/bin/libdl.dylib'} <= set(private))
        first = work/'relocation one 한글'; first.mkdir()
        bundle = first/'host tools'; shutil.copytree(produced, bundle, symlinks=True)
        pin = digest(bundle/'manifest.json')
        baseline = host_tools.payload_inventory(bundle)
        hidden = [ROOT, producer, sdk, args.contract.resolve(), *[p.resolve() for p in args.hide], args.clang.resolve().parent]
        assert all(not bundle.is_relative_to(p) and not cli.is_relative_to(p) for p in hidden)
        # A host-only poisoned dylib is an executable positive control, never a product dependency.
        trap = producer/'trap.c'
        trap.write_text('#include <fcntl.h>\n#include <unistd.h>\n__attribute__((constructor)) static void trap(void) { int f=open('+json.dumps(str(marker),ensure_ascii=False)+',O_CREAT|O_WRONLY,0600); if(f>=0){write(f,"trap",4);close(f);} _exit(97); }\n')
        vendor = contract.get('external_os_sdk') or checked('host test SDK path', ['/usr/bin/xcrun','--sdk','macosx','--show-sdk-path']).stdout.decode().strip()
        checked('build poisoned native library control', [args.clang, '-isysroot', vendor, '-dynamiclib', trap,
            '-Wl,-install_name,@rpath/libdl.dylib', '-o', poison/'libdl.dylib'])
        probe_source = producer/'probe.c'
        probe_source.write_text('''#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
int main(int c,char**v){if(c>2&&!strcmp(v[1],"environment")){int i=2;while(i+1<c&&strcmp(v[i],"--")){if(strncmp(v[i],"DYLD_",5)||setenv(v[i],v[i+1],1))return 6;i+=2;}if(i+1>=c)return 7;execv(v[i+1],v+i+1);return 8;}for(int i=1;i+1<c;i+=2){if(!strcmp(v[i],"read")){int f=open(v[i+1],O_RDONLY);if(f>=0 || (errno!=EPERM && errno!=EACCES))return 2;}else{int s;pid_t p=fork();if(p<0)return 3;if(!p){execl(v[i+1],v[i+1],(char*)0);_exit(errno==EPERM||errno==EACCES?0:4);}if(waitpid(p,&s,0)!=p||!WIFEXITED(s)||WEXITSTATUS(s))return 5;}}return 0;}
''')
        probe = work/'boundary-probe'
        checked('build native boundary probe', [args.clang, '-isysroot', vendor, probe_source, '-o', probe])
        env = {'HOME': str(home), 'PATH': '/nonexistent', 'LC_ALL': 'C', 'DYLD_LIBRARY_PATH': str(poison),
               'DYLD_FALLBACK_LIBRARY_PATH': str(poison)}

        def boxed(name, argv, environment=env):
            quote = lambda p: json.dumps(str(p), ensure_ascii=False)
            allowed = [cli, probe, *{(bundle/p).resolve() for p in manifest['executable_files']}]
            text = '(version 1)\n(allow default)\n(deny network*)\n'
            for path in hidden:
                text += '(deny file-read* file-write* (subpath '+quote(path)+'))\n'
            text += '(deny file-write* (subpath '+quote(bundle)+'))\n'
            text += '(deny process-exec (require-not (require-any '+' '.join('(literal '+quote(p)+')' for p in sorted(set(allowed)))+')))\n'
            policy = work/(name+'.sb'); policy.write_text(text)
            # Apple's sandbox-exec strips DYLD_* at its own startup. Set only
            # the test's DYLD keys after that boundary, in a native host probe.
            injected = [item for key,value in environment.items() if key.startswith('DYLD_') for item in (key,value)]
            return run(name, ['/usr/bin/sandbox-exec','-f',policy,probe,'environment',*injected,'--',*argv], cwd=poison, env=environment)

        boundary = [probe]
        for path in hidden:
            boundary += ['read', path]
        for path in ['/bin/sh','/usr/bin/awk','/usr/bin/make',str(Path(sys.executable).resolve()),str(args.clang)]:
            boundary += ['exec', path]
        record('source SDK compiler and host fallbacks are denied', boxed('boundary', boundary).returncode == 0)
        awk = bundle/'system/bin/awk'
        control = boxed('unfiltered-poison-positive-control', [awk,'BEGIN {print 42}'])
        record('poisoned library is a working positive control', control.returncode == 97 and marker.read_bytes() == b'trap')
        marker.unlink(missing_ok=True)
        clean = {'HOME': str(home), 'PATH': '/nonexistent', 'LC_ALL': 'C', 'DYLD_PRINT_LIBRARIES': '1'}
        proc = boxed('direct-shared-awk-loader-evidence', [awk,'BEGIN {if (3^4 != 81) exit 12; print 42}'], clean)
        trace = proc.stderr.decode(errors='replace')
        record('actual private dylibs load from the relocated bundle', proc.returncode == 0 and proc.stdout == b'42\n' and
            all(str(bundle/p) in trace for p in private), trace)

        def cli_call(name, tail, okay=True):
            proc = boxed(name, [cli,'--json',*map(str,tail)])
            try:
                reply = json.loads(proc.stdout)
            except ValueError:
                reply = {}
            record(name, (proc.returncode == 0 if okay else proc.returncode > 0) and reply.get('ok') is okay, reply)
            return reply

        def selection():
            return ['--host-tools',str(bundle),'--host-tools-sha256',pin]

        cli_call('private-closure-environment-doctor', ['doctor','--scope','environment',*selection()])
        script = work/'private workflow.sh'
        script.write_text('set -eu\ntest "${DYLD_LIBRARY_PATH-unset}" = unset\ntest "${DYLD_FALLBACK_LIBRARY_PATH-unset}" = unset\nawk \'BEGIN {if (3^4 != 81) exit 12; print "shared-awk: 42"}\'\n"$PATH/../../usr/bin/awk" \'BEGIN {print "alias-awk: 42"}\'\n')
        job = work/'job one'
        reply = cli_call('CLI shared-AWK hostile-path workflow', ['shell',*selection(),'--work-root',job,'--script',script,'--directory',poison])
        record('no poisoned library was loaded by CLI children', not marker.exists() and 'shared-awk: 42' in reply.get('data',{}).get('stdout',''))
        second = work/'relocation two 한글'; second.mkdir()
        bundle.rename(second/'host tools'); bundle = second/'host tools'
        record('second relocation preserves the pinned inventory', digest(bundle/'manifest.json') == pin and host_tools.payload_inventory(bundle) == baseline)
        cli_call('second-relocation-doctor', ['doctor','--scope','environment',*selection()])
        cli_call('second-relocation-workflow', ['shell',*selection(),'--work-root',work/'job two','--script',script,'--directory',poison])
        alias_links = {p:os.readlink(p) for p in (bundle/'usr/bin').glob('*.dylib') if p.is_symlink()}
        assert alias_links, 'a copied alias needs explicit private-runtime links'
        try:
            for path in alias_links:
                path.unlink()
            proc = boxed('copied-alias-without-runtime-links', [bundle/'usr/bin/awk','BEGIN {print 42}'], clean)
            record('copied alias requires the producer private-runtime links', proc.returncode != 0 and b'Library not loaded' in proc.stderr)
        finally:
            for path,target in alias_links.items():
                path.symlink_to(target)
        proc = boxed('copied-alias-with-restored-runtime-links', [bundle/'usr/bin/awk','BEGIN {print 42}'], clean)
        record('copied alias works with the canonical closure restored', proc.returncode == 0 and proc.stdout == b'42\n')
        victim = bundle/'system/bin/libdl.dylib'; saved = work/'original-libdl'; victim.rename(saved)
        try:
            cli_call('missing-private-library-is-refused', ['doctor','--scope','environment',*selection()], False)
            failed_job = work/'missing-job'
            cli_call('missing-private-library-prevents-job', ['shell',*selection(),'--work-root',failed_job,'--script',script], False)
            record('missing library leaves no job root', not failed_job.exists())
            proc = boxed('dyld-missing-library-diagnostic', [bundle/'system/bin/awk','BEGIN {print 42}'], clean)
            record('dyld rejects missing private library without SDK fallback', proc.returncode != 0 and b'Library not loaded' in proc.stderr)
            victim.write_bytes(b'not a Mach-O library\n')
            cli_call('modified-private-library-is-refused', ['doctor','--scope','environment',*selection()], False)
            failed_job = work/'modified-job'
            cli_call('modified-private-library-prevents-job', ['shell',*selection(),'--work-root',failed_job,'--script',script], False)
            record('modified library leaves no job root', not failed_job.exists())
        finally:
            victim.unlink(missing_ok=True); saved.rename(victim)
        cli_call('restored-private-closure-verifies', ['doctor','--scope','environment',*selection()])
        cli_call('restored-private-closure-runs', ['shell',*selection(),'--work-root',work/'job restored','--script',script,'--directory',poison])
        record('bundle and original SDK bytes are unchanged', baseline == host_tools.payload_inventory(bundle) and original == metadata.inventory(sdk))
        owned = audit.owned_symbols(sdk)
        images = [audit.collect_image(bundle/manifest['tools']['awk'],'system/bin/awk',owned)]
        images += [audit.collect_image(bundle/p,'lib/'+Path(p).name,owned) for p in private]
        failures = audit.problems(images)
        (out/'bindings.json').write_text(json.dumps([{**i,'bound':{k:sorted(v) for k,v in i['bound'].items()},'math_only':sorted(i['math_only'])} for i in images],indent=2)+'\n')
        record('shared AWK and private closure binding audit', not failures, failures)
        record('AWK import sources are unmodified', input_hashes == {p.name:digest(p) for p in source.iterdir() if p.is_file()})
        (out/'fixture.json').write_text(json.dumps({'manifest_sha256':pin,'manifest':manifest,'private_libraries':private,
            'linked_awk_sha256':digest(linked),'bundled_awk_sha256':digest(awk if awk.exists() else bundle/'system/bin/awk'),
            'clang':{'path':str(args.clang),'sha256':digest(args.clang)},'linker':{'path':str(args.linker),'sha256':digest(args.linker)},
            'sdk_manifest_sha256':digest(sdk/'manifest.json'),'contract_sha256':digest(args.contract),'poison_sha256':digest(poison/'libdl.dylib')},ensure_ascii=False,indent=2)+'\n')
    except Exception as error:
        record('replay completed without a harness exception', False, repr(error))
    finally:
        completed = True
        save()
    print(f"{sum(r['passed'] for r in results)}/{len(results)} passed", flush=True)
    return 0 if results and all(r['passed'] for r in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
