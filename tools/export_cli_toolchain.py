#!/usr/bin/env python3
"""Export a local experimental Linux static CLI policy from installed SDK wrappers.

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
    if platform.system() != 'Linux' or platform.machine() != 'x86_64' or manifest['target'] != {'os': 'linux', 'arch': 'x86_64'}:
        parser.error('experimental export currently supports native Linux/x86_64 only')
    if profile.get('format') != 1 or profile.get('profile') != 'dev':
        parser.error('expected development packaging profile format 1')
    inventory = []
    for path in sorted(sdk.rglob('*')):
        if path.is_symlink():
            path.resolve(strict=True).relative_to(sdk)
            inventory.append([path.relative_to(sdk).as_posix(), 'symlink', os.readlink(path)])
        elif path.is_file():
            inventory.append([path.relative_to(sdk).as_posix(), 'file', digest(path)])
    compact = json.dumps(inventory, ensure_ascii=False, separators=(',', ':'))
    result = dict(schema='crt.experimental.native-toolchain', format=0,
                  target=dict(os='linux', arch='x86_64'), policy='static-hosted-exceptions-rtti',
                  sdk_inventory=inventory, sdk_content_sha256=hashlib.sha256(compact.encode()).hexdigest(),
                  producer_source_commit=profile['producer_source_commit'], commands={})
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
                   'CRT_TARGET_OS': 'linux', 'CRT_TARGET_ARCH': 'x86_64',
                   'CRT_CXX_RUNTIME_LINKAGE': 'static', 'CRT_RUNTIME_LINKAGE': 'static',
                   'CRT_CXX_ENABLE_EXCEPTIONS': '1', 'CRT_CXX_ENABLE_RTTI': '1', 'CRT_CXX_HOSTED': '1',
                   'REAL_CLANG': real, 'CAPTURE_ARGV': str(work / 'argv.json')}
            for operation, flags in [('compile', ['@EXTRA@', '-c', f'@SOURCE@.{suffix}', '-o', '@OUTPUT@']),
                                     ('link', ['@OBJECTS@', '@LIBRARIES@', '-o', '@OUTPUT@'])]:
                subprocess.run([str(sdk / 'tools' / wrapper), *flags], env=env, check=True)
                vector = json.loads((work / 'argv.json').read_text())
                vector = [v.replace(str(sdk), '@SDK@').replace(resource, '@RESOURCE@').replace(f'@SOURCE@.{suffix}', '@SOURCE@') for v in vector]
                if any(str(work) in v for v in vector):
                    raise RuntimeError('unresolved producer scratch path')
                result['commands'][operation + '_' + language] = vector
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
