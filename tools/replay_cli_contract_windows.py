#!/usr/bin/env python3
"""Producer-side native Windows (x64 or arm64) CLI contract replay; never a CLI runtime dependency.

Windows counterpart of replay_cli_contract_macos.py. A disposable hosted C++ seed is compiled from the provider's
argv, builds the CRT-native CLI, which then self-builds and runs its native integration suite. Every consumer
process runs under the restricted token of crt_windows_external (the producer and CLI trees, which live under the
user profile, are not readable through the per-run SID) with PATH empty, stdio redirected to logs, and a job
object that records every process the run starts (crt_windows_trace). Windows offers no per-executable launch
denial without changing ACLs of installed tools, so the OS-recorded process inventory plays the part of the Linux
execve trace and the macOS exec policy: it must contain only the declared native tools, the CLI and the
executables it generated. The bare run additionally hides the exported source, the relocated SDK and the provider.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import uuid

import crt_pe
import crt_windows_external as sandbox
import crt_windows_trace as trace
from crt_dev_profile import digest
from test_dev_sdk_external import inventory, inventory_digest

ROOT = Path(__file__).resolve().parent.parent
FORBIDDEN = ('cmd.exe', 'powershell.exe', 'pwsh.exe', 'python.exe', 'python3.exe', 'py.exe', 'cmake.exe', 'ninja.exe',
             'make.exe', 'gmake.exe', 'mingw32-make.exe', 'sh.exe', 'bash.exe', 'mksh.exe', 'perl.exe', 'ruby.exe',
             'crt-cc.cmd', 'crt-c++.cmd', 'wsl.exe')
SYSTEM_IMPORTS = {'kernel32.dll', 'api-ms-win-core-synch-l1-2-0.dll'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cli-source', type=Path, required=True)
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--clang', type=Path, required=True)
    parser.add_argument('--clangxx', type=Path, required=True)
    parser.add_argument('--linker', type=Path, required=True)
    parser.add_argument('--ar', type=Path, required=True)
    parser.add_argument('--hidden', type=Path, action='append', default=[],
                        help='additional producer-side trees that must be unreadable (e.g. the producer worktree)')
    parser.add_argument('--work', type=Path, help='fresh private work directory (default: a new one under TEMP)')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('this acceptance driver requires native Windows')
    cli, original = args.cli_source.resolve(), args.sdk_root.resolve()
    evidence = args.output.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    (evidence / 'result.json').unlink(missing_ok=True)
    work, sid = sandbox.create_work(args.work)
    source = work / 'source'
    # Spaces yes, Hangul no: Windows narrow paths are ANSI code-page bytes and the imported libc++ rejects non-ASCII
    # narrow<->wide conversion ("locale not supported"); that gap is recorded in the acceptance document, not hidden.
    sdk = work / 'SDK space' / 'dev'
    source.mkdir()
    sdk.parent.mkdir()
    before = inventory(original)
    shutil.copytree(original, sdk, symlinks=True)
    names = subprocess.check_output(['git', '-C', str(cli), 'ls-files', '--cached', '--others', '--exclude-standard', '-z']).decode().split('\0')
    for name in sorted(set(filter(None, names))):
        file = cli / name
        if not file.is_file() or name.startswith('bin/'):
            continue  # committed host binaries are artifacts, not build inputs
        target = source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        # Windows checkouts convert to CRLF; the exported inputs are the repository's LF text, as on the other hosts.
        target.write_bytes(file.read_bytes().replace(b'\r\n', b'\n'))
    code = [p for directory in ('src', 'include') for p in (source / directory).rglob('*') if p.is_file()]
    if any(re.search(r'\b(?:__linux__|__APPLE__|_WIN32|CRT_TARGET_OS_[A-Z]+)\b', p.read_text(encoding='utf-8')) for p in code):
        raise ValueError('CLI source must use CRT PAL rather than OS macros')
    if list((source / 'src/main').glob('*.h')) or not (source / 'include/crtcli/core.h').is_file():
        raise ValueError('CLI headers must live under include/')
    input_inventory = inventory(source)
    commands = []

    def save(name, data):
        (evidence / name).write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

    save('inputs.json', {'cli_base': subprocess.check_output(['git', '-C', str(cli), 'rev-parse', 'HEAD'], text=True).strip(),
                        'source_inventory': input_inventory, 'source_sha256': inventory_digest(input_inventory),
                        'sdk_sha256': inventory_digest(before), 'work': str(work)})
    for name in ('home', 'tmp', 'seed', 'bare', 'logs'):
        (work / name).mkdir()
    tools = {key: getattr(args, key).absolute() for key in ('clang', 'clangxx', 'linker', 'ar')}
    system_root = os.environ['SystemRoot']
    environment = {'PATH': '', 'USERPROFILE': str(work / 'home'), 'HOME': str(work / 'home'),
                   'TEMP': str(work / 'tmp'), 'TMP': str(work / 'tmp'), 'TMPDIR': str(work / 'tmp'),
                   'SystemRoot': system_root, 'LC_ALL': 'C'}
    provider = work / 'native-toolchain-v1.json'
    processes = {}
    # The bare phase hides the exported source, the relocated SDK and the provider: it runs under a second, unregistered
    # SID that is granted only the bare/home/tmp directories this driver created (the work root grants just the build SID).
    bare_sid = 'S-1-5-21-' + '-'.join(str(int.from_bytes(uuid.uuid4().bytes[i:i + 4], 'little')) for i in (0, 4, 8, 12))
    for name in ('bare', 'home', 'tmp'):
        trace.private_directory(work / name, sid, bare_sid)

    def allowed(image, bare):
        if image is None:
            return False
        path = Path(image).resolve()
        name = path.name.lower()
        if name in FORBIDDEN:
            return False
        if path == Path(system_root, 'System32', 'conhost.exe').resolve():
            return True  # the OS console host the loader attaches to a console-subsystem child; not a build tool
        declared = {p.resolve() for p in tools.values()} | {p.with_suffix('.exe').resolve() for p in tools.values()}
        # clang drives the declared linker/archiver, which may be the same image under another name (lld, llvm-ar).
        tool_directory = tools['clang'].resolve().parent
        if path.parent == tool_directory and name in {'clang.exe', 'clang++.exe', 'clang-cl.exe', 'ld.lld.exe', 'lld.exe',
                                                       'lld-link.exe', 'llvm-ar.exe', 'llvm-ranlib.exe'}:
            return not bare
        if path in declared:
            return not bare
        roots = [work / 'bare'] if bare else [work / 'seed', work / 'bare', work / 'probe.exe', source / 'out']
        generated = [work / 'tmp'] + ([] if bare else [source / 'out'])
        return any(path == r.resolve() or r.resolve() in path.parents for r in roots + generated)

    def run(label, argv, *, boxed=False, bare=False, timeout=900, expected=0, env=None, cwd=None):
        identity = bare_sid if bare else sid
        cwd = cwd or (work / 'bare' if bare else None)
        argv = list(map(str, argv))
        commands.append({'label': label, 'argv': argv, 'cwd': str(cwd or work)})
        save('commands.json', commands)
        print('START', label, flush=True)
        out, err = work / 'logs' / (label + '.stdout'), work / 'logs' / (label + '.stderr')
        if boxed:
            result = trace.run(argv, work=work, sid=identity, env=env or environment, cwd=cwd or work, stdout=out, stderr=err, timeout=timeout)
            status = result['exit_code']
            commands[-1]['processes'] = result['processes']
            commands[-1]['timed_out'] = result['timed_out']
            for item in result['processes']:
                processes.setdefault(label, []).append(item)
        else:
            with out.open('w') as o, err.open('w') as e:
                status = subprocess.run(argv, cwd=cwd or work, env=env, stdout=o, stderr=e, stdin=subprocess.DEVNULL, timeout=timeout).returncode
        for suffix in ('.stdout', '.stderr'):
            shutil.copy2(work / 'logs' / (label + suffix), evidence / (label + suffix))
        commands[-1]['exit'] = status
        save('commands.json', commands)
        if boxed:
            stray = [p for p in commands[-1]['processes'] if not allowed(p['image'], bare)]
            if stray:
                raise ValueError(f'{label}: undeclared processes started: {stray[:5]}')
        if status != expected:
            raise ValueError(f'{label}: exit {status}; ' + (evidence / (label + '.stderr')).read_text(errors='replace')[-6000:])
        print('PASS', label, flush=True)

    producer_env = dict(os.environ)
    run('provider', [sys.executable, ROOT / 'tools/export_cli_toolchain.py', '--sdk-root', sdk, '--output', provider,
                     '--clang', tools['clang'], '--clangxx', tools['clangxx']], env=producer_env)
    contract = json.loads(provider.read_text(encoding='utf-8'))

    # CRT-native probe, compiled with the SDK under test; it must fail to read the denied trees.
    activation = dict(producer_env, CRT_SYSROOT=sdk.as_posix(), CRT_ROOTFS=sdk.as_posix(), CRT_TARGET_OS='windows',
                      CRT_TARGET_ARCH=contract['target']['arch'], CRT_MKSH_EXE=str(sdk / 'system/bin/mksh.exe'),
                      CRT_HOST_CC=tools['clang'].as_posix(), CRT_HOST_CXX=tools['clangxx'].as_posix(),
                      CRT_WINDOWS_SDK_LIBPATH=contract['external_os_sdk'])
    probe_source = work / 'probe.c'
    probe_source.write_text(r'''#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
int main(int argc, char** argv) {
  int i;
  for (i = 1; i + 1 < argc; i += 2) {
    if (strcmp(argv[i], "--read")) return 9;
    DIR* directory = opendir(argv[i + 1]);
    if (directory) { closedir(directory); return 10; }
    if (errno != EACCES && errno != EPERM) { fprintf(stderr, "unexpected errno %d for %s\n", errno, argv[i + 1]); return 11; }
    int fd = open(argv[i + 1], O_RDONLY);
    if (fd >= 0) return 12;
  }
  puts("boundary probe: denied");
  return 0;
}
''', encoding='utf-8')
    run('probe-build', [sdk / 'tools/crt-cc.cmd', probe_source, '-o', work / 'probe.exe'], env=activation)

    def probe(bare):
        if bare:
            shutil.copy2(work / 'probe.exe', work / 'bare' / 'probe.exe')
        command = [work / 'bare' / 'probe.exe' if bare else work / 'probe.exe']
        for path in [ROOT, cli, original, *args.hidden, *([source, sdk, provider] if bare else [])]:
            command.extend(['--read', path])
        run('bare-visibility' if bare else 'build-visibility', command, boxed=True, bare=bare)
    probe(False)

    resource = subprocess.check_output([tools['clangxx'], '-print-resource-dir'], text=True).strip()
    generated = work / 'generated'
    generated.mkdir()
    profile = json.loads((sdk / 'share/crt/contracts/development.json').read_text(encoding='utf-8'))
    values = {'VERSION': '0.1.0-dev', 'TARGET_OS': contract['target']['os'], 'TARGET_ARCH': contract['target']['arch'],
              'SDK_VERSION': profile['version'], 'SDK_COMMIT': contract['producer_source_commit'], 'SDK_CONTENT': contract['sdk_content_sha256']}
    (generated / 'bootstrap_config.h').write_text('#pragma once\n' + ''.join('#define CRT_CLI_' + key + ' ' + json.dumps(value) + '\n' for key, value in values.items()), encoding='utf-8')

    def expand(vector, *, inputs, output, extra=(), libraries=()):
        result = []
        for item in vector:
            if item in ('@OBJECTS@', '@EXTRA@', '@LIBRARIES@'):
                result.extend(map(str, {'@OBJECTS@': inputs, '@EXTRA@': extra, '@LIBRARIES@': libraries}[item]))
            else:
                for key, value in {'@SDK@': sdk, '@RESOURCE@': resource, '@SOURCE@': inputs[0], '@OUTPUT@': output}.items():
                    item = item.replace(key, str(value))
                result.append(item)
        return result

    declaration = json.loads((source / 'crt.json').read_text(encoding='utf-8'))
    application = declaration['application']
    objects = []
    for i, name in enumerate(application['sources']):
        obj = work / 'seed' / (str(i) + '.o')
        # Same rule as the CLI builder: the SDK consumer options reach only the sources that declare they need them.
        options = profile['consumer_compile_options'] if name in application.get('sdk_consumer_option_sources', []) else []
        flags = ['-std=c++17', '-O1', '-g', '-I' + str(source / 'include'), '-I' + str(generated), *options]
        run(f'seed-compile-{i}', [tools['clangxx'], *expand(contract['modes']['executable-static']['compile_cxx'],
              inputs=[source / name], output=obj, extra=flags)], boxed=True)
        objects.append(obj)
    seed = work / 'seed' / 'crt-cli.exe'
    libraries = [sdk / contract['libraries'][name]['static'] for name in application['sdk_libraries']]
    run('seed-link', [tools['clangxx'], *expand(contract['modes']['executable-static']['link_cxx'],
          inputs=objects, output=seed, libraries=libraries), '--ld-path=' + str(tools['linker'])], boxed=True)
    configure = ['configure', '--project', source, '--sdk-root', sdk, '--sdk-contract', provider,
                 '--contract-sha256', digest(provider), '--cc', tools['clang'], '--cxx', tools['clangxx'],
                 '--linker', tools['linker'], '--ar', tools['ar'], '--allow-dirty-sdk', 'true']
    run('configure', [seed, *configure, '--json'], boxed=True)
    run('build', [seed, 'build', '--project', source, '--json'], boxed=True)
    # The incremental cache would let the self-build reuse the seed's objects; drop them so it recompiles from source.
    for stale in (source / 'out/native').iterdir():
        if stale.suffix in ('.o', '.key', '.d'):
            stale.unlink()
    built = source / 'out/native/crt-cli.exe'
    run('self-build', [built, 'build', '--project', source, '--json'], boxed=True)
    cold = json.loads((evidence / 'self-build.stdout').read_text(encoding='utf-8'))['data']['cache']
    if cold['executed'] < 2 or cold['reused'] != 0:
        raise ValueError(f'self-build did not recompile from source: {cold}')
    run('cached-build', [built, 'build', '--project', source, '--json'], boxed=True)
    warm = json.loads((evidence / 'cached-build.stdout').read_text(encoding='utf-8'))['data']['cache']
    if warm['executed'] != 0 or warm['reused'] != cold['executed']:
        raise ValueError(f'unchanged rebuild was not served from the cache: {cold} {warm}')
    program = work / 'bare' / 'crt-cli.exe'
    shutil.copy2(built, program)
    run('native-tests', [program, 'test', '--project', source, '--json'], boxed=True, timeout=7200)
    probe(True)
    for label, argv in [('help', ['--help']), ('version', ['--version']), ('doctor', ['doctor', '--bootstrap'])]:
        run('bare-' + label, [program, *argv, '--json'], boxed=True, bare=True)
    imports = sorted(name.lower() for name in crt_pe.imported_libraries(program))
    if not set(imports) <= SYSTEM_IMPORTS:
        raise ValueError('single executable import audit failed: ' + str(imports))
    if inventory(original) != before or inventory(sdk) != before:
        raise ValueError('SDK inputs were modified')
    doctor = json.loads((evidence / 'bare-doctor.stdout').read_text(encoding='utf-8'))
    seen = sorted({Path(p['image']).name.lower() for items in processes.values() for p in items if p['image']})
    save('result.json', {'passed': True, 'work': str(work), 'sdk_inventory_unchanged': True,
                         'sdk_sha256': inventory_digest(before), 'provider_sha256': digest(provider),
                         'source_sha256': inventory_digest(input_inventory), 'binary_sha256': digest(program),
                         'binary_size': program.stat().st_size, 'imports': imports,
                         'process_images_seen': seen, 'process_count': sum(len(v) for v in processes.values()),
                         'doctor': doctor, 'cache': {'cold': cold, 'warm': warm},
                         'tests': json.loads((evidence / 'native-tests.stdout').read_text(encoding='utf-8'))})
    print('CLI contract replay passed:', evidence / 'result.json')


if __name__ == '__main__':
    main()
