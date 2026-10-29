#!/usr/bin/env python3
"""Export experimental native target policies from installed SDK wrappers.

Producer migration tool, not a CLI runtime dependency or SDK Build Contract v1.
The immutable SDK is read only; the separately hashed output binds its full
inventory and captures wrapper argv rather than duplicating their ABI policy.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile


def digest(path):
    return hashlib.file_digest(path.open('rb'), 'sha256').hexdigest()


def native_target(system=None, machine=None):
    system = system or platform.system()
    machine = (machine or platform.machine()).lower()
    arch = {'amd64': 'x86_64', 'arm64': 'aarch64'}.get(machine, machine)
    target_os = {'Linux': 'linux', 'Darwin': 'macos'}.get(system)
    if (target_os, arch) not in (('linux', 'x86_64'), ('macos', 'aarch64')):
        raise ValueError('experimental export supports native Linux/x86_64 and macOS/arm64 only')
    return {'os': target_os, 'arch': arch}


def normalize_argv(vector, sdk, resource, suffix):
    return [value.replace(str(sdk), '@SDK@').replace(resource, '@RESOURCE@')
            .replace(f'@SOURCE@.{suffix}', '@SOURCE@') for value in vector]


TEMPORARY_PREFIXES = ('/tmp/', '/var/tmp/', '/private/tmp/', '/var/folders/', '/private/var/folders/')


def unreplayable_paths(vector, sdk):
    """Absolute temporary-directory paths in a recorded argv that do not belong to the SDK.

    Placeholders (@SDK@, @RESOURCE@, @OBJECTS@ ...) and the SDK itself are fine; a path under a temporary directory
    is a file some wrapper made for that one call."""
    found = []
    for value in vector:
        for token in str(value).split(','):
            token = token.split('=', 1)[-1] if token.startswith('-') else token
            token = token.lstrip('-L').lstrip('I') if token[:2] in ('-L', '-I') else token
            if token.startswith(TEMPORARY_PREFIXES) and not token.startswith(str(sdk)):
                found.append(token)
    return found


def project_local_shared_rpath(vector, mode, operation, target_os):
    """Let a consumer executable find a shared library of its own project beside it.

    An ELF executable that links a shared library records only the library's soname (DT_NEEDED) and finds it through
    RUNPATH. The recorded wrapper argv carries the SDK directory (C++ only) but not the executable's own directory, so
    a project-local shared library next to the executable was not found at run time (2026-10-29 Linux replay,
    variant-c-shared-library). macOS resolves the dylib by its recorded path and needs nothing here; every other
    mode and operation is returned unchanged."""
    if not (mode == 'executable-shared' and operation == 'link' and target_os == 'linux'):
        return list(vector)
    result = [value for value in vector if value != '-Wl,-rpath,$ORIGIN']
    result.insert(result.index('@OBJECTS@') if '@OBJECTS@' in result else 0, '-Wl,-rpath,$ORIGIN')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--clang', default='clang')
    parser.add_argument('--clangxx', default='clang++')
    args = parser.parse_args()
    sdk = args.sdk_root.resolve(strict=True)
    manifest = json.loads((sdk / 'manifest.json').read_text())
    profile = json.loads((sdk / 'share/crt/contracts/development.json').read_text())
    try:
        target = native_target()
    except ValueError as error:
        parser.error(str(error))
    if manifest['target'] != target:
        parser.error('experimental export requires an SDK for the native host target')
    if profile.get('format') != 1 or profile.get('profile') != 'dev':
        parser.error('expected development packaging profile format 1')
    if not {'crt-platform-queries-1', 'json-sha2-1'} <= set(profile.get('capabilities', [])):
        parser.error('SDK lacks platform-query/JSON/SHA-2 capabilities; produce a new candidate')
    inventory = []
    for path in sorted(sdk.rglob('*')):
        if path.is_symlink():
            path.resolve(strict=True).relative_to(sdk)
            inventory.append([path.relative_to(sdk).as_posix(), 'symlink', os.readlink(path)])
        elif path.is_file():
            inventory.append([path.relative_to(sdk).as_posix(), 'file', digest(path)])
    compact = json.dumps(inventory, ensure_ascii=False, separators=(',', ':'))
    result = dict(schema='crt.experimental.native-toolchain', format=1,
                  target=target, policy='native-targets-exceptions-rtti',
                  sdk_inventory=inventory, sdk_content_sha256=hashlib.sha256(compact.encode()).hexdigest(),
                  producer_source_commit=profile['producer_source_commit'], commands={}, modes={})
    result['provenance'] = {key: profile[key] for key in
                            ('producer_working_tree_dirty', 'base_source_commit', 'compile_provenance')}
    result['artifacts'] = dict(executable_suffix='', static_library_prefix='lib', static_library_suffix='.a',
                               shared_library_prefix='lib', shared_library_suffix='.dylib' if target['os'] == 'macos' else '.so')
    result['libraries'] = {}
    for archive in sorted((sdk / 'lib').glob('lib*.a')):
        if archive.name.endswith('.dll.a'):
            continue
        name = archive.name[3:-2]
        choices = {'static': archive.relative_to(sdk).as_posix()}
        shared = sdk / 'lib' / ('lib' + name + result['artifacts']['shared_library_suffix'])
        if shared.is_file():
            choices['shared'] = shared.relative_to(sdk).as_posix()
        result['libraries'][name] = choices
    os_sdk_flags = []
    if target['os'] == 'macos':
        # Explicit vendor SDK for libSystem/linker lookup; -nostdinc and the
        # installed Bionic include paths still own target C/C++ headers.
        os_sdk = subprocess.check_output(['xcrun', '--sdk', 'macosx', '--show-sdk-path'], text=True).strip()
        os_sdk = str(Path(os_sdk).resolve(strict=True))
        result['external_os_sdk'] = os_sdk
        os_sdk_flags = ['-isysroot', os_sdk]
    with tempfile.TemporaryDirectory(prefix='crt-cli-policy-') as tmp:
        work = Path(tmp)
        recorder = work / 'record'
        recorder.write_text('''#!/usr/bin/python3
import json, os, subprocess, sys
if sys.argv[1:] == ['-print-resource-dir']:
    sys.exit(subprocess.call([os.environ['REAL_CLANG'], '-print-resource-dir']))
with open(os.environ['CAPTURE_ARGV'], 'w') as stream:
    json.dump(sys.argv[1:], stream)
''')
        recorder.chmod(0o700)
        for language, compiler, wrapper, suffix in [('c', args.clang, 'crt-cc', 'c'), ('cxx', args.clangxx, 'crt-c++', 'cpp')]:
            real = str(Path(shutil.which(compiler) or compiler).resolve(strict=True))
            resource = subprocess.check_output([real, '-print-resource-dir'], text=True).strip()
            env = {'PATH': '/usr/bin:/bin', 'CRT_SYSROOT': str(sdk), 'CRT_ROOTFS': str(sdk),
                   'CRT_HOST_CC': str(recorder), 'CRT_HOST_CXX': str(recorder),
                   'CRT_TARGET_OS': target['os'], 'CRT_TARGET_ARCH': target['arch'],
                   'CRT_CXX_RUNTIME_LINKAGE': 'static', 'CRT_RUNTIME_LINKAGE': 'static',
                   'CRT_CXX_ENABLE_EXCEPTIONS': '1', 'CRT_CXX_ENABLE_RTTI': '1', 'CRT_CXX_HOSTED': '1',
                   'REAL_CLANG': real, 'CAPTURE_ARGV': str(work / 'argv.json')}
            for mode, linkage, shared, pic in (
                    ('executable-static', 'static', False, False),
                    ('executable-shared', 'shared', False, True),
                    ('static-library', 'static', False, True),
                    ('shared-library', 'shared', True, True)):
                selected_env = dict(env, CRT_CXX_RUNTIME_LINKAGE=linkage, CRT_RUNTIME_LINKAGE=linkage,
                                    CRT_CXX_STL_LINKAGE=linkage)
                commands = result['modes'].setdefault(mode, {})
                compile_flags = (['-fPIC'] if pic else []) + ['@EXTRA@', '-c', f'@SOURCE@.{suffix}', '-o', '@OUTPUT@']
                naming = (['-dynamiclib', '-Wl,-install_name,@OUTPUT@'] if target['os'] == 'macos'
                          else ['-shared', '-Wl,-soname,@OUTPUT_NAME@']) if shared else []
                for operation, flags in [('compile', compile_flags),
                                         ('link', naming + ['@OBJECTS@', '@LIBRARIES@', '-o', '@OUTPUT@'])]:
                    if mode == 'static-library' and operation == 'link':
                        commands['archive'] = ['rcs', '@OUTPUT@', '@OBJECTS@']
                        continue
                    subprocess.run([str(sdk / 'tools' / wrapper), *os_sdk_flags, *flags], env=selected_env, check=True)
                    vector = normalize_argv(json.loads((work / 'argv.json').read_text()), sdk, resource, suffix)
                    if any(str(work) in v for v in vector):
                        raise RuntimeError('unresolved producer scratch path')
                    # A wrapper-made temporary file (the loader import stub used to be one) is gone once the wrapper
                    # returns, so a vector that names one cannot be replayed by a consumer.
                    leaked = unreplayable_paths(vector, sdk)
                    if leaked:
                        raise RuntimeError('recorded argv names temporary files outside the SDK: ' + ', '.join(leaked))
                    vector = project_local_shared_rpath(vector, mode, operation, target['os'])
                    commands[operation + '_' + language] = vector
                if mode == 'executable-static':
                    result['commands'].update(commands)
    result['exporter_sha256'] = digest(Path(__file__))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    # No overwrite: a candidate is a new input identity, never an in-place update.
    with args.output.open('x') as stream:
        json.dump(result, stream, indent=2, ensure_ascii=False)
        stream.write('\n')
    print(json.dumps(dict(path=str(args.output), sha256=digest(args.output),
                         sdk_content_sha256=result['sdk_content_sha256'], entries=len(inventory))))


if __name__ == '__main__':
    main()
