#!/usr/bin/env python3
"""Producer-side Linux CLI contract replay; never a CLI runtime dependency.

Linux counterpart of replay_cli_contract_macos.py: the same inputs, the same steps and the same pass criteria,
with Bubblewrap and an execve audit in place of macOS Seatbelt.

  provider export -> boundary probes -> hosted C++ seed (compiled through the exported SDK vectors)
  -> configure -> seed build -> self-build (cold cache) -> unchanged rebuild (warm cache)
  -> the CLI's own native tests -> bare help/version/doctor with SDK, source and compilers hidden
  -> single-executable audit -> SDK inventory unchanged

Inside every tested run the network namespace is empty, the CRT producer checkout and the CLI repository are
replaced by empty directories, PATH is /nonexistent and the environment is cleared. Shells, Python, CMake, CTest,
Ninja and make are masked by an empty file, and every execve is logged by strace and checked afterwards: any
executed file outside the declared toolchain, the CLI binaries and the build's own outputs fails the replay.
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

import crt_elf
from crt_dev_profile import digest
from test_dev_sdk_external import inventory, inventory_digest

ROOT = Path(__file__).resolve().parent.parent
MASKED_NAMES = {'bash', 'dash', 'sh', 'zsh', 'fish', 'mksh', 'ksh', 'cmake', 'ctest', 'ninja', 'make', 'gmake',
                'perl', 'ruby'}


def masked_executables(sdk):
    """Installed interpreters and build drivers (resolved)."""
    found = set()
    for directory in ('/usr/bin', '/usr/local/bin', '/bin', '/usr/sbin', '/sbin'):
        base = Path(directory)
        if not base.is_dir():
            continue
        for path in base.iterdir():
            if (path.name in MASKED_NAMES or path.name.startswith(('python', 'pypy'))) and path.is_file():
                found.add(path.resolve())
    # The SDK's own wrappers are not masked: the CLI reads and hashes them as part of the SDK identity. That they are
    # never executed is proven by the execve audit instead.
    return sorted(found)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--cli-source', type=Path, required=True)
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--clang', type=Path, required=True)
    parser.add_argument('--clangxx', type=Path, required=True)
    parser.add_argument('--linker', type=Path, required=True)
    parser.add_argument('--ar', type=Path, required=True)
    parser.add_argument('--work-parent', type=Path, default=ROOT.parent / 'crt-cli-replay',
                        help='directory (outside the producer checkout) that receives the per-run work tree')
    parser.add_argument('--hide', type=Path, action='append', default=[],
                        help='extra checkout/handoff directory to replace by an empty one inside every tested run')
    args = parser.parse_args()
    if not sys.platform.startswith('linux'):
        parser.error('this acceptance driver requires native Linux Bubblewrap')
    bwrap, strace = shutil.which('bwrap'), shutil.which('strace')
    if not bwrap or not strace:
        parser.error('bwrap and strace are required')
    cli, original = args.cli_source.resolve(), args.sdk_root.resolve()
    evidence = args.output.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    (evidence / 'result.json').unlink(missing_ok=True)
    args.work_parent.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='crt-cli-contract-', dir=args.work_parent)).resolve()
    source = work / 'source'
    sdk = work / 'SDK space 한글/dev'
    source.mkdir()
    sdk.parent.mkdir()
    before = inventory(original)
    shutil.copytree(original, sdk, symlinks=True)
    names = subprocess.check_output(['git', '-C', cli, 'ls-files', '--cached', '--others', '--exclude-standard', '-z']).decode().split('\0')
    for name in sorted(set(filter(None, names))):
        file = cli / name
        if not file.is_file():
            continue
        target = source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(file, target)
    code = [p for directory in ('src', 'include') for p in (source / directory).rglob('*') if p.is_file()]
    if any(re.search(r'\b(?:__linux__|__APPLE__|_WIN32|CRT_TARGET_OS_[A-Z]+)\b', p.read_text()) for p in code):
        raise ValueError('CLI source must use CRT PAL rather than OS macros')
    if list((source / 'src/main').glob('*.h')) or not (source / 'include/crtcli/core.h').is_file():
        raise ValueError('CLI headers must live under include/')
    input_inventory = inventory(source)
    commands = []

    def save(name, data):
        (evidence / name).write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n')

    save('inputs.json', {'cli_base': subprocess.check_output(['git', '-C', cli, 'rev-parse', 'HEAD'], text=True).strip(),
                        'source_inventory': input_inventory, 'source_sha256': inventory_digest(input_inventory),
                        'sdk_sha256': inventory_digest(before), 'work': str(work)})
    for name in ('home', 'tmp', 'seed', 'bare', 'logs', 'execve'):
        (work / name).mkdir()
    tools = {key: getattr(args, key).absolute() for key in ('clang', 'clangxx', 'linker', 'ar')}
    ranlib = tools['ar'].with_name('ranlib')
    provider = work / 'native-toolchain-v1.json'
    masked = masked_executables(sdk)
    hidden = [ROOT, cli, *[p.resolve() for p in args.hide]]
    execve_logs = []

    def sandbox(bare, label):
        """Bubblewrap argv prefix: no network, empty producer/CLI trees, masked interpreters, cleared environment."""
        argv = [bwrap, '--unshare-all', '--die-with-parent', '--new-session', '--ro-bind', '/', '/',
                '--proc', '/proc', '--dev', '/dev', '--tmpfs', '/tmp', '--clearenv',
                '--setenv', 'HOME', str(work / 'home'), '--setenv', 'PATH', '/nonexistent', '--setenv', 'LC_ALL', 'C',
                '--setenv', 'TMPDIR', '/tmp']
        # Hide first, bind the work tree afterwards: a tmpfs over a parent would cover a mount made earlier below it.
        for path in hidden:
            argv += ['--tmpfs', str(path)]
        argv += ['--bind', str(work), str(work), '--chdir', str(work)]
        if bare:
            argv += ['--tmpfs', str(source), '--tmpfs', str(sdk.parent), '--ro-bind', '/dev/null', str(provider)]
            for path in tools.values():
                argv += ['--ro-bind', '/dev/null', str(path.resolve())]
        for path in masked:
            argv += ['--ro-bind', '/dev/null', str(path)]
        log = work / 'execve' / (label + '.log')
        execve_logs.append(log)
        return argv + [strace, '--seccomp-bpf', '-f', '-qq', '-e', 'trace=execve', '-o', str(log), '--']

    def run(label, argv, *, boxed=False, bare=False, timeout=900, expected=0):
        argv = list(map(str, argv))
        if boxed:
            argv = sandbox(bare, label) + argv
        commands.append({'label': label, 'argv': argv, 'cwd': str(work)})
        save('commands.json', commands)
        print('START', label, flush=True)
        with (work / 'logs' / (label + '.stdout')).open('w') as out, (work / 'logs' / (label + '.stderr')).open('w') as err:
            result = subprocess.run(argv, cwd=work, env=None if boxed else {'PATH': os.environ['PATH'], 'LC_ALL': 'C'},
                                    stdout=out, stderr=err, stdin=subprocess.DEVNULL, timeout=timeout)
        for suffix in ('.stdout', '.stderr'):
            shutil.copy2(work / 'logs' / (label + suffix), evidence / (label + suffix))
        commands[-1]['exit'] = result.returncode
        save('commands.json', commands)
        if result.returncode != expected:
            raise ValueError(f'{label}: exit {result.returncode}; ' + (evidence / (label + '.stderr')).read_text()[-6000:])
        print('PASS', label, flush=True)

    run('provider', [sys.executable, ROOT / 'tools/export_cli_toolchain.py', '--sdk-root', sdk,
                     '--output', provider, '--clang', tools['clang'], '--clangxx', tools['clangxx']])
    contract = json.loads(provider.read_text())
    probe_source = work / 'probe.c'
    probe_source.write_text(r'''#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
int main(int argc, char** argv) {
  int i;
  for (i = 1; i + 1 < argc; i += 2) {
    if (!strcmp(argv[i], "--empty")) {
      /* a hidden tree is an empty directory: opening it works, and it must hold nothing */
      int fd = open(argv[i+1], O_RDONLY | O_DIRECTORY);
      if (fd < 0) return 10;
      close(fd);
      char marker[4096];
      snprintf(marker, sizeof marker, "%s/.git", argv[i+1]);
      if (access(marker, F_OK) == 0) return 14;
      snprintf(marker, sizeof marker, "%s/tools", argv[i+1]);
      if (access(marker, F_OK) == 0) return 15;
    } else if (!strcmp(argv[i], "--masked")) {
      /* a masked executable is an empty file: it exists but cannot run */
      struct stat st;
      if (stat(argv[i+1], &st) != 0 || st.st_size != 0) return 16;
      if (access(argv[i+1], X_OK) == 0) { /* executable bit survives the bind; running it must still do nothing */
        pid_t pid = fork(); int status;
        if (pid < 0) return 11;
        if (pid == 0) { char* args[] = {argv[i+1], 0}; execv(args[0], args); _exit(errno == ENOEXEC || errno == EACCES || errno == EPERM ? 0 : 12); }
        if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status)) return 13;
      }
    }
  }
  puts("boundary probe: denied"); return 0;
}
''')
    run('probe-build', [tools['clang'], probe_source, '-o', work / 'probe'])

    def probe(bare):
        command = [work / 'probe']
        for path in hidden:
            command.extend(['--empty', path])
        if bare:
            command.extend(['--empty', source, '--empty', sdk.parent])
        forbidden = list(masked)
        for path in forbidden:
            command.extend(['--masked', path])
        run('bare-visibility' if bare else 'build-visibility', command, boxed=True, bare=bare)

    probe(False)
    resource = subprocess.check_output([tools['clangxx'], '-print-resource-dir'], text=True).strip()
    generated = work / 'generated'
    generated.mkdir()
    profile = json.loads((sdk / 'share/crt/contracts/development.json').read_text())
    values = {'VERSION': '0.1.0-dev', 'TARGET_OS': contract['target']['os'], 'TARGET_ARCH': contract['target']['arch'],
              'SDK_VERSION': profile['version'], 'SDK_COMMIT': contract['producer_source_commit'], 'SDK_CONTENT': contract['sdk_content_sha256']}
    (generated / 'bootstrap_config.h').write_text('#pragma once\n' + ''.join('#define CRT_CLI_' + key + ' ' + json.dumps(value) + '\n' for key, value in values.items()))

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

    declaration = json.loads((source / 'crt.json').read_text())
    application = declaration['application']
    objects = []
    for i, name in enumerate(application['sources']):
        obj = work / 'seed' / (str(i) + '.o')
        flags = ['-std=c++17', '-O1', '-g', '-I' + str(source / 'include'), '-I' + str(generated), *profile['consumer_compile_options']]
        run(f'seed-compile-{i}', [tools['clangxx'], *expand(contract['modes']['executable-static']['compile_cxx'],
              inputs=[source / name], output=obj, extra=flags)], boxed=True)
        objects.append(obj)
    seed = work / 'seed/crt-cli'
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
    run('self-build', [source / 'out/native/crt-cli', 'build', '--project', source, '--json'], boxed=True)
    cold = json.loads((evidence / 'self-build.stdout').read_text())['data']['cache']
    if cold['executed'] < 2 or cold['reused'] != 0:
        raise ValueError(f'self-build did not recompile from source: {cold}')
    run('cached-build', [source / 'out/native/crt-cli', 'build', '--project', source, '--json'], boxed=True)
    warm = json.loads((evidence / 'cached-build.stdout').read_text())['data']['cache']
    if warm['executed'] != 0 or warm['reused'] != cold['executed']:
        raise ValueError(f'unchanged rebuild was not served from the cache: {cold} {warm}')
    program = work / 'bare/crt-cli'
    shutil.copy2(source / 'out/native/crt-cli', program)
    run('native-tests', [program, 'test', '--project', source, '--json'], boxed=True, timeout=2400)
    probe(True)
    for label, argv in [('help', ['--help']), ('version', ['--version']), ('doctor', ['doctor', '--bootstrap'])]:
        run('bare-' + label, [program, *argv, '--json'], boxed=True, bare=True)

    # Single-executable audit: no DT_NEEDED, no RPATH/RUNPATH, the SDK's declared loader as interpreter.
    needed = crt_elf.needed_libraries(program)
    runtime_paths = crt_elf.runtime_paths(program)
    issues = []
    if needed:
        issues.append(f'DT_NEEDED is not empty: {needed}')
    if any(value for value in runtime_paths):
        issues.append(f'RPATH/RUNPATH present: {runtime_paths}')
    interpreter = subprocess.check_output(['readelf', '-l', str(program)], text=True)
    found = re.search(r'Requesting program interpreter: ([^\]]+)\]', interpreter)
    interpreter_path = found.group(1) if found else None
    if interpreter_path not in ('/lib64/ld-linux-x86-64.so.2', '/lib/ld-linux-aarch64.so.1'):
        issues.append(f'unexpected program interpreter: {interpreter_path}')

    # Every executed file, from the strace logs of all tested runs.
    allowed = {p.resolve() for p in (*tools.values(), ranlib) if p.exists()}
    allowed |= {seed.resolve(), program.resolve(), (work / 'probe').resolve(), (source / 'out/native/crt-cli').resolve()}
    forbidden_names = MASKED_NAMES | {'crt-cc', 'crt-c++'}
    executed, outside = {}, {}
    for log in execve_logs:
        for line in log.read_text(errors='replace').splitlines():
            found = re.search(r'execve\("([^"]+)".*\)\s+=\s+(-?\d+)', line)
            if not found or int(found.group(2)) != 0:
                continue
            path = Path(found.group(1))
            try:
                resolved = path.resolve()
            except OSError:
                resolved = path
            executed[str(resolved)] = executed.get(str(resolved), 0) + 1
            name = resolved.name
            if name in forbidden_names or name.startswith(('python', 'pypy')):
                outside[str(resolved)] = 'forbidden tool executed'
            elif resolved not in allowed and work not in resolved.parents and not str(resolved).startswith('/tmp/'):
                # clang re-executes itself; the dynamic loader and files the build produced live under work or /tmp
                if resolved.parent not in {p.parent for p in allowed} and str(resolved) != interpreter_path:
                    outside[str(resolved)] = 'not a declared tool or build output'
    if outside:
        issues.append(f'unexpected executed files: {outside}')
    save('execve-summary.json', {'distinct': len(executed), 'total': sum(executed.values()), 'executed': executed, 'unexpected': outside})
    if issues:
        raise ValueError('single executable / execve audit failed: ' + '; '.join(issues))
    if inventory(original) != before or inventory(sdk) != before:
        raise ValueError('SDK inputs were modified')
    doctor = json.loads((evidence / 'bare-doctor.stdout').read_text())
    save('result.json', {'passed': True, 'work': str(work), 'sdk_inventory_unchanged': True,
                         'sdk_sha256': inventory_digest(before), 'provider_sha256': digest(provider),
                         'source_sha256': inventory_digest(input_inventory), 'binary_sha256': digest(program),
                         'binary_size': program.stat().st_size, 'needed': needed, 'interpreter': interpreter_path,
                         'execve_distinct': len(executed), 'doctor': doctor, 'cache': {'cold': cold, 'warm': warm},
                         'tests': json.loads((evidence / 'native-tests.stdout').read_text())})
    print('CLI contract replay passed:', evidence / 'result.json')


if __name__ == '__main__':
    main()
