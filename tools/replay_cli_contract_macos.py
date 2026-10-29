#!/usr/bin/env python3
"""Producer-side macOS CLI contract replay; never a CLI runtime dependency."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

import audit_macos_bindings as audit
import crt_macho
from crt_dev_profile import digest
from test_dev_sdk_external import inventory, inventory_digest

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cli-source', type=Path, required=True)
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--clang', type=Path, required=True)
    parser.add_argument('--clangxx', type=Path, required=True)
    parser.add_argument('--linker', type=Path, required=True)
    parser.add_argument('--ar', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('this acceptance driver requires native macOS Seatbelt')
    cli, original = args.cli_source.resolve(), args.sdk_root.resolve()
    evidence = args.output.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    (evidence / 'result.json').unlink(missing_ok=True)
    work = Path(tempfile.mkdtemp(prefix='crt-cli-contract-')).resolve()
    source = work / 'source'
    sdk = work / 'SDK space \uD55C\uAE00/dev'
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
    for name in ('home', 'tmp', 'seed', 'bare', 'logs'):
        (work / name).mkdir()
    tools = {key: getattr(args, key).absolute() for key in ('clang', 'clangxx', 'linker', 'ar')}
    environment = {'PATH': '/nonexistent', 'HOME': str(work / 'home'), 'TMPDIR': str(work / 'tmp'), 'LC_ALL': 'C'}
    provider = work / 'native-toolchain-v1.json'
    def run(label, argv, *, boxed=False, bare=False, timeout=900, expected=0):
        argv = list(map(str, argv))
        if boxed:
            argv = ['/usr/bin/sandbox-exec', '-f', str(policy(bare)), *argv]
        commands.append({'label': label, 'argv': argv, 'cwd': str(work)})
        save('commands.json', commands)
        print('START', label, flush=True)
        with (work / 'logs' / (label + '.stdout')).open('w') as out, (work / 'logs' / (label + '.stderr')).open('w') as err:
            result = subprocess.run(argv, cwd=work, env=environment if boxed else None,
                                    stdout=out, stderr=err, stdin=subprocess.DEVNULL, timeout=timeout)
        for suffix in ('.stdout', '.stderr'):
            shutil.copy2(work / 'logs' / (label + suffix), evidence / (label + suffix))
        commands[-1]['exit'] = result.returncode
        save('commands.json', commands)
        if result.returncode != expected:
            raise ValueError(f'{label}: exit {result.returncode}; ' + (evidence / (label + '.stderr')).read_text()[-6000:])
        print('PASS', label, flush=True)
    def policy(bare):
        denied = [ROOT, cli] + ([source, sdk] if bare else [])
        allowed = [work / 'bare/crt-cli', work / 'probe']
        if not bare:
            # ar runs ranlib itself to build the archive index
            ranlib = tools['ar'].with_name('ranlib')
            allowed += list(tools.values()) + [p.resolve() for p in tools.values()] + [ranlib, ranlib.resolve(), work / 'seed/crt-cli']
        terms = ' '.join('(literal ' + json.dumps(str(p), ensure_ascii=False) + ')' for p in allowed)
        if not bare:
            terms += ' (subpath ' + json.dumps(str(source / 'out/native'), ensure_ascii=False) + ')'
            terms += ' (regex "^/private/tmp/crt-cli-native-[^/]+/.*")'
        text = '(version 1)\n(allow default)\n(deny network*)\n'
        text += ''.join('(deny file-read* file-write* (subpath ' + json.dumps(str(p), ensure_ascii=False) + '))\n' for p in denied)
        if bare:
            text += '(deny file-read* file-write* (literal ' + json.dumps(str(provider), ensure_ascii=False) + '))\n'
        text += '(deny process-exec (require-not (require-any ' + terms + ')))\n'
        path = work / ('bare.sb' if bare else 'build.sb')
        path.write_text(text)
        return path
    run('provider', [sys.executable, ROOT / 'tools/export_cli_toolchain.py', '--sdk-root', sdk,
                     '--output', provider, '--clang', tools['clang'], '--clangxx', tools['clangxx']])
    contract = json.loads(provider.read_text())
    probe_source = work / 'probe.c'
    probe_source.write_text(r'''#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
int main(int argc, char** argv) {
  int i;
  for (i = 1; i + 1 < argc; i += 2) {
    if (!strcmp(argv[i], "--read")) {
      int fd = open(argv[i+1], O_RDONLY);
      if (fd >= 0 || (errno != EPERM && errno != EACCES)) return 10;
    } else {
      pid_t pid = fork(); int status;
      if (pid < 0) return 11;
      if (pid == 0) {
        char* args[] = {argv[i+1], 0};
        execv(args[0], args);
        _exit(errno == EPERM || errno == EACCES ? 0 : 12);
      }
      if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status)) return 13;
    }
  }
  puts("boundary probe: denied"); return 0;
}
''')
    run('probe-build', [tools['clang'], '-isysroot', contract['external_os_sdk'], probe_source, '-o', work / 'probe'])
    def probe(bare):
        command = [work / 'probe']
        for path in [ROOT, cli, *([source, sdk, provider] if bare else [])]:
            command.extend(['--read', path])
        forbidden = ['/bin/sh', '/bin/bash', '/bin/zsh', sys.executable, '/usr/bin/python3',
                     '/opt/homebrew/bin/cmake', '/opt/homebrew/bin/ninja', '/usr/bin/make',
                     str(sdk / 'tools/crt-cc'), str(sdk / 'tools/crt-c++')]
        if bare:
            forbidden.extend(map(str, tools.values()))
        for path in forbidden:
            if not Path(path).is_file():
                raise ValueError('probe prerequisite missing: ' + path)
            command.extend(['--exec', Path(path).resolve()])
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
    run('self-build', [source / 'out/native/crt-cli', 'build', '--project', source, '--json'], boxed=True)
    program = work / 'bare/crt-cli'
    shutil.copy2(source / 'out/native/crt-cli', program)
    run('native-tests', [program, 'test', '--project', source, '--json'], boxed=True, timeout=2400)
    probe(True)
    for label, argv in [('help', ['--help']), ('version', ['--version']), ('doctor', ['doctor', '--bootstrap'])]:
        run('bare-' + label, [program, *argv, '--json'], boxed=True, bare=True)
    image = audit.collect_image(program, 'crt-cli', audit.owned_symbols(sdk))
    issues = audit.problems([image])
    if issues or crt_macho.dependencies(program) != ['/usr/lib/libSystem.B.dylib'] or crt_macho.rpaths(program):
        raise ValueError('single executable import audit failed: ' + str(issues))
    if inventory(original) != before or inventory(sdk) != before:
        raise ValueError('SDK inputs were modified')
    doctor = json.loads((evidence / 'bare-doctor.stdout').read_text())
    save('result.json', {'passed': True, 'work': str(work), 'sdk_inventory_unchanged': True,
                         'sdk_sha256': inventory_digest(before), 'provider_sha256': digest(provider),
                         'source_sha256': inventory_digest(input_inventory), 'binary_sha256': digest(program),
                         'binary_size': program.stat().st_size, 'imports': image['dependencies'],
                         'owns_malloc': image['owns_malloc'], 'bindings': {key: sorted(value) for key, value in image['bound'].items()},
                         'doctor': doctor, 'tests': json.loads((evidence / 'native-tests.stdout').read_text())})
    print('CLI contract replay passed:', evidence / 'result.json')


if __name__ == '__main__':
    main()
