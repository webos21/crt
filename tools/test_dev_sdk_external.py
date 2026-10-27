#!/usr/bin/env python3
"""Linux Dev SDK acceptance with the original SDK and producer trees hidden.

The host compiler/Python/OpenSSL remain available. Target headers, libraries
and wrappers come from a relocated SDK; the only source supplied separately is
the reference consumer and its build-host validation scripts.
"""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile

from crt_dev_profile import digest, runtime_digests

ROOT = Path(__file__).resolve().parent.parent


def validation_scripts():
    """Only the fixture's top-level local imports, not the producer source tree."""
    pending, seen = ['test_dev_sdk_consumer'], {}
    while pending:
        name = pending.pop()
        path = ROOT / 'tools' / (name + '.py')
        if name in seen or not path.is_file():
            continue
        seen[name] = path
        for node in ast.parse(path.read_text(encoding='utf-8')).body:
            if isinstance(node, ast.Import):
                pending.extend(alias.name.split('.')[0] for alias in node.names)
            elif isinstance(node, ast.ImportFrom) and node.module and not node.level:
                pending.append(node.module.split('.')[0])
    return sorted(seen.values())


def inventory(root):
    return {p.relative_to(root).as_posix():
            'symlink:' + os.readlink(p) if p.is_symlink() else 'file:' + digest(p)
            for p in sorted(root.rglob('*')) if p.is_symlink() or p.is_file()}


def inventory_digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def native_linux(manifest):
    arch = {'amd64': 'x86_64', 'arm64': 'aarch64'}.get(platform.machine().lower(), platform.machine().lower())
    if sys.platform != 'linux' or manifest.get('target') != {'os': 'linux', 'arch': arch}:
        raise ValueError('external execution requires a native Linux SDK; cross targets are compile-only')


def hidden_roots(sdk, extra=()):
    roots = sorted({ROOT.resolve(), sdk.resolve(), *(p.resolve() for p in extra)}, key=lambda p: len(p.parts))
    result = []
    for path in roots:
        if path == Path('/') or not path.is_dir():
            raise ValueError(f'hidden root must be an existing non-root directory: {path}')
        if not any(path.is_relative_to(parent) for parent in result):
            result.append(path)
    return result


def sandbox_command(bwrap, work, hidden, program, args):
    if any(work == path or work.is_relative_to(path) or path.is_relative_to(work) for path in hidden):
        raise ValueError('external work directory must be disjoint from every hidden producer/SDK root')
    command = [bwrap, '--die-with-parent', '--unshare-user', '--unshare-pid',
               '--ro-bind', '/', '/', '--proc', '/proc', '--dev', '/dev',
               '--tmpfs', '/tmp', '--bind', str(work), str(work), '--chdir', str(work),
               '--clearenv', '--setenv', 'HOME', str(work / 'home'),
               '--setenv', 'TMPDIR', '/tmp', '--setenv', 'LC_ALL', 'C.UTF-8',
               '--setenv', 'PYTHONDONTWRITEBYTECODE', '1']
    search = ['/usr/local/bin', '/usr/bin', '/bin']
    for variable, default in [('CRT_CC', 'clang'), ('CRT_CXX', 'clang++'), ('CRT_AR', 'ar')]:
        tool = shutil.which(os.environ.get(variable, default))
        if not tool:
            raise ValueError(f'build-host tool missing: {default}')
        # Keep clang++'s invocation name; resolving its symlink to clang changes
        # driver behavior. Resolve only when checking hidden-tree membership.
        tool = str(Path(tool).absolute())
        if any(Path(tool).resolve().is_relative_to(path) for path in hidden):
            raise ValueError(f'build-host tool is inside a hidden producer tree: {tool}')
        command += ['--setenv', variable, tool]
        search.insert(0, str(Path(tool).parent))
    command += ['--setenv', 'PATH', os.pathsep.join(dict.fromkeys(search))]
    for path in hidden:
        command += ['--tmpfs', str(path)]
    return command + ['--', program, *args]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-root', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--work-root', type=Path, help='fresh external directory; retained with evidence')
    parser.add_argument('--hide-root', action='append', default=[], type=Path,
                        help='also hide a producer build root located outside the checkout')
    args = parser.parse_args(argv)
    sdk, output = args.sdk_root.resolve(), args.output.resolve()
    if sdk == output or sdk.is_relative_to(output) or output.is_relative_to(sdk):
        raise ValueError('SDK input and evidence output must be disjoint')
    output.mkdir(parents=True, exist_ok=True)
    (output / 'result.json').unlink(missing_ok=True)
    manifest = json.loads((sdk / 'manifest.json').read_text())
    native_linux(manifest)
    bwrap = shutil.which('bwrap')
    if not bwrap:
        raise ValueError('source-hidden Linux acceptance requires bubblewrap (bwrap); no unisolated fallback')
    hidden = hidden_roots(sdk, args.hide_root)
    work = args.work_root.resolve() if args.work_root else Path(tempfile.mkdtemp(prefix='crt dev external '))
    if args.work_root:
        work.mkdir(parents=True, exist_ok=False)
    # Validate disjoint paths before copying the SDK.
    sandbox_command(bwrap, work, hidden, sys.executable, [])
    (work / 'home').mkdir()
    relocated = work / 'relocated Development SDK'
    before = inventory(sdk)
    shutil.copytree(sdk, relocated, symlinks=True)
    if inventory(relocated) != before:
        raise ValueError('relocated SDK inventory differs from original')
    kit = work / 'fixture'
    (kit / 'tools').mkdir(parents=True)
    # Exact validation scripts, with no producer headers/runtime objects. Their
    # ROOT becomes this fixture directory; the original checkout is inaccessible.
    scripts = validation_scripts()
    for script in scripts:
        shutil.copy2(script, kit / 'tools' / script.name)
    source = kit / 'libstdc++/tests/development_sdk_consumer.cc'
    source.parent.mkdir(parents=True)
    shutil.copy2(ROOT / 'libstdc++/tests/development_sdk_consumer.cc', source)
    probe = kit / 'tools/external_probe.py'
    probe.write_text('''import json, os, pathlib, runpy, sys
import crt_dev_profile
hidden = json.loads(os.environ['CRT_TEST_HIDDEN_ROOTS'])
for path in hidden:
    if list(pathlib.Path(path).iterdir()):
        raise RuntimeError('producer/SDK root remains visible: ' + path)
pathlib.Path('visibility.json').write_text(json.dumps({'hidden_roots': hidden, 'all_empty': True}))
sdk = pathlib.Path(sys.argv[1])
problems = crt_dev_profile.check(sdk, json.loads((sdk / 'manifest.json').read_text()))
if problems:
    raise RuntimeError('relocated Dev profile rejected: ' + '; '.join(problems))
sys.argv = ['test_dev_sdk_consumer.py', '--sdk-root', sys.argv[1], '--output', sys.argv[2]]
runpy.run_path(str(pathlib.Path(__file__).with_name('test_dev_sdk_consumer.py')), run_name='__main__')
''')
    command = sandbox_command(bwrap, work, hidden, sys.executable,
                              [str(probe), str(relocated), str(work / 'consumer build with spaces')])
    command[command.index('--'):command.index('--')] = [
        '--setenv', 'CRT_TEST_HIDDEN_ROOTS', json.dumps([str(p) for p in hidden])]
    (output / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
    with (output / 'consumer.log').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=600)
    visibility = json.loads((work / 'visibility.json').read_text())
    consumer = json.loads((work / 'consumer build with spaces/result.json').read_text())
    if not visibility.get('all_empty') or not consumer.get('passed'):
        raise ValueError('hidden-tree consumer validation did not complete')
    if inventory(sdk) != before or inventory(relocated) != before:
        raise ValueError('consumer replay changed original or relocated SDK payload')
    result = {'passed': True, 'isolation': 'linux-bubblewrap', 'target': manifest['target'],
              'producer_source_and_build_hidden': True, 'original_sdk_hidden': True,
              'hidden_roots': visibility['hidden_roots'], 'work_root': str(work),
              'sdk_inventory_sha256': inventory_digest(before), 'sdk_inventory_entries': len(before),
              'sdk_payload_unchanged': True, 'runtime_sha256': runtime_digests(sdk),
              'fixture_scripts': {p.name: digest(p) for p in scripts},
              'consumer': consumer}
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print('External Dev SDK: hidden producer/SDK, relocated static/shared HTTPS+inflate and trust negatives pass')


if __name__ == '__main__':
    main()
