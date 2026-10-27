#!/usr/bin/env python3
"""Native Dev SDK acceptance with the original SDK and producer trees inaccessible.

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


def native_target(manifest):
    arch = {'amd64': 'x86_64', 'arm64': 'aarch64'}.get(platform.machine().lower(), platform.machine().lower())
    target_os = {'linux': 'linux', 'darwin': 'macos', 'win32': 'windows'}.get(sys.platform)
    if target_os is None or manifest.get('target') != {'os': target_os, 'arch': arch}:
        raise ValueError('external execution requires a native Linux/macOS/Windows SDK; cross targets are compile-only')
    return target_os


def hidden_roots(sdk, extra=()):
    roots = sorted({ROOT.resolve(), sdk.resolve(), *(p.resolve() for p in extra)}, key=lambda p: len(p.parts))
    result = []
    for path in roots:
        if path == Path(path.anchor) or not path.is_dir():
            raise ValueError(f'hidden root must be an existing non-root directory: {path}')
        if not any(path.is_relative_to(parent) for parent in result):
            result.append(path)
    return result


def require_disjoint(work, hidden):
    if any(work == path or work.is_relative_to(path) or path.is_relative_to(work) for path in hidden):
        raise ValueError('external work directory must be disjoint from every hidden producer/SDK root')


def sandbox_command(bwrap, work, hidden, program, args):
    require_disjoint(work, hidden)
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


def macos_sandbox_command(sandbox, work, hidden, program, args):
    require_disjoint(work, hidden)
    # Use OS-enforced denial, not renaming/chmod of the shared checkout. Children
    # (compiler, linker, dyld and consumers) inherit the same file restrictions.
    profile = '(version 1)\n(allow default)\n' + ''.join(
        f'(deny file-read* file-write* (subpath {json.dumps(str(path))}))\n' for path in hidden)
    return [sandbox, '-p', profile, program, *args]


def macos_environment(work, hidden):
    env = {'HOME': str(work / 'home'), 'TMPDIR': str(work / 'tmp'), 'LC_ALL': 'C',
           'PYTHONDONTWRITEBYTECODE': '1', 'CRT_TEST_HIDDEN_ROOTS': json.dumps([str(p) for p in hidden]),
           'CRT_TEST_ISOLATION': 'macos-seatbelt'}
    search = ['/usr/local/bin', '/usr/bin', '/bin', '/usr/sbin', '/sbin', '/opt/homebrew/bin']
    for variable, default in [('CRT_CC', 'clang'), ('CRT_CXX', 'clang++'), ('CRT_AR', 'ar')]:
        tool = shutil.which(os.environ.get(variable, default))
        if not tool:
            raise ValueError(f'build-host tool missing: {default}')
        tool = str(Path(tool).absolute())
        if any(Path(tool).resolve().is_relative_to(path) for path in hidden):
            raise ValueError(f'build-host tool is inside a hidden producer tree: {tool}')
        env[variable] = tool
        search.insert(0, str(Path(tool).parent))
    env['PATH'] = os.pathsep.join(dict.fromkeys(search))
    return env


def windows_environment(work, hidden):
    """No producer paths or ambient Python, trust or library configuration."""
    env = {'SystemRoot': os.environ['SystemRoot'], 'WINDIR': os.environ['SystemRoot'],
           'COMSPEC': str(Path(os.environ['SystemRoot']) / 'System32/cmd.exe'),
           'TEMP': str(work / 'tmp'), 'TMP': str(work / 'tmp'),
           'USERPROFILE': str(work / 'home'), 'PYTHONDONTWRITEBYTECODE': '1',
           'PYTHONNOUSERSITE': '1', 'CRT_TEST_ISOLATION': 'windows-restricted-token',
           'CRT_TEST_HIDDEN_ROOTS': json.dumps([str(p) for p in hidden])}
    search = [str(Path(os.environ['SystemRoot']) / 'System32')]
    for variable, default in [('CRT_CC', 'clang.exe'), ('CRT_CXX', 'clang++.exe'),
                              ('CRT_AR', 'llvm-ar.exe'), ('CRT_RANLIB', 'llvm-ranlib.exe'),
                              ('CRT_TEST_OPENSSL', 'openssl.exe')]:
        tool = shutil.which(os.environ.get(variable, default))
        if not tool:
            raise ValueError(f'build-host tool missing: {default}')
        tool = Path(tool).absolute()
        if any(tool.resolve().is_relative_to(path) for path in hidden):
            raise ValueError(f'build-host tool is inside a hidden producer tree: {tool}')
        env[variable] = tool.as_posix()
        search.insert(0, str(tool.parent))
    sdk_lib = Path(os.environ.get('CRT_WINDOWS_SDK_LIBPATH', '')).resolve()
    if not (sdk_lib / 'kernel32.lib').is_file() or any(sdk_lib.is_relative_to(p) for p in hidden):
        raise ValueError('CRT_WINDOWS_SDK_LIBPATH must identify external native Windows SDK import libraries')
    env['CRT_WINDOWS_SDK_LIBPATH'] = sdk_lib.as_posix()
    env['PATH'] = os.pathsep.join(dict.fromkeys(search))
    return env


def windows_python(work):
    """Copy only build-host Python, since a per-user installation is private too."""
    prefix, output = Path(sys.base_prefix), work / 'host Python'
    output.mkdir()
    for name in ('Lib', 'DLLs'):
        shutil.copytree(prefix / name, output / name,
                        ignore=shutil.ignore_patterns('site-packages', '__pycache__', 'test', 'tests'))
    for path in prefix.iterdir():
        if path.is_file() and (path.suffix.lower() == '.dll' or path.name in ('python.exe', 'LICENSE.txt')):
            shutil.copy2(path, output / path.name)
    if not (output / 'python.exe').is_file():
        raise ValueError('a standalone native Windows host Python installation is required')
    return output / 'python.exe'


def windows_bootstrap():
    # No producer handles are inherited. Supply private valid standard handles
    # after startup so subprocess can duplicate them without console windows.
    return '''import sys, os, runpy, ctypes, msvcrt
log = open('consumer.log', 'w', buffering=1)
null = open(os.devnull)
sys.stdout = sys.stderr = log
sys.stdin = null
set_std = ctypes.WinDLL('kernel32', use_last_error=True).SetStdHandle
set_std.argtypes = [ctypes.c_ulong, ctypes.c_void_p]
set_std.restype = ctypes.c_int
for number, stream in ((-10, null), (-11, log), (-12, log)):
    if not set_std(number & 0xffffffff, msvcrt.get_osfhandle(stream.fileno())):
        raise ctypes.WinError(ctypes.get_last_error())
sys.path.insert(0, os.path.dirname(sys.argv[1]))
sys.argv = sys.argv[1:]
runpy.run_path(sys.argv[0], run_name='__main__')
'''


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
    target_os = native_target(manifest)
    tool_name = 'bwrap' if target_os == 'linux' else 'sandbox-exec'
    sandbox = None if target_os == 'windows' else shutil.which(tool_name)
    if target_os != 'windows' and not sandbox:
        raise ValueError(f'source-hidden {target_os} acceptance requires {tool_name}; no unisolated fallback')
    make_command = sandbox_command if target_os == 'linux' else macos_sandbox_command
    hidden = hidden_roots(sdk, args.hide_root)
    protected_files = [ROOT / 'AGENTS.md', sdk / 'manifest.json']
    if target_os == 'windows':
        # Prove the original headers/runtime cannot supply the build/load, not
        # just that one unrelated producer document is unreadable.
        protected_files += [ROOT / 'libstdc++/tests/development_sdk_consumer.cc',
                            sdk / 'include/stdio.h', sdk / 'lib/libc.a', sdk / 'bin/libc.dll']
        if not all(path.is_file() for path in protected_files):
            raise ValueError('Windows visibility proof requires known producer/header/runtime files')
    if target_os == 'windows':
        import crt_windows_external as windows
        if args.work_root:
            require_disjoint(args.work_root.resolve(), hidden)
        work, sid = windows.create_work(args.work_root)
    else:
        work = args.work_root.resolve() if args.work_root else Path(tempfile.mkdtemp(prefix='crt dev external '))
        if args.work_root:
            work.mkdir(parents=True, exist_ok=False)
    # Validate disjoint paths before copying the SDK.
    require_disjoint(work, hidden)
    (work / 'home').mkdir()
    (work / 'tmp').mkdir()
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
isolation = os.environ.get('CRT_TEST_ISOLATION', 'linux-bubblewrap')
for path in hidden:
    try:
        entries = list(pathlib.Path(path).iterdir())
    except PermissionError:
        if isolation not in ('macos-seatbelt', 'windows-restricted-token'):
            raise
    else:
        if isolation != 'linux-bubblewrap' or entries:
            raise RuntimeError('producer/SDK root remains visible: ' + path)
for path in json.loads(os.environ['CRT_TEST_HIDDEN_FILES']):
    try:
        with open(path, 'rb') as stream:
            stream.read(1)
    except PermissionError:
        if isolation not in ('macos-seatbelt', 'windows-restricted-token'):
            raise
    except FileNotFoundError:
        if isolation != 'linux-bubblewrap':
            raise
    else:
        raise RuntimeError('producer/SDK file remains readable: ' + path)
if isolation == 'windows-restricted-token':
    # A fresh descendant must inherit the restriction too, not just this probe.
    import subprocess
    check = "import pathlib,sys; p=pathlib.Path(sys.argv[1]); p.read_bytes()"
    for path in json.loads(os.environ['CRT_TEST_HIDDEN_FILES']):
        child = subprocess.run([sys.executable, '-I', '-c', check, path], capture_output=True,
                               timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
        if child.returncode == 0 or b'PermissionError' not in child.stderr:
            raise RuntimeError('descendant restriction not proven: ' + path)
pathlib.Path('visibility.json').write_text(json.dumps({'hidden_roots': hidden, 'all_inaccessible': True,
    'all_empty': isolation == 'linux-bubblewrap', 'files_unreadable': True,
    'descendant_denial_checked': isolation == 'windows-restricted-token'}))
sdk = pathlib.Path(sys.argv[1])
problems = crt_dev_profile.check(sdk, json.loads((sdk / 'manifest.json').read_text()))
if problems:
    raise RuntimeError('relocated Dev profile rejected: ' + '; '.join(problems))
sys.argv = ['test_dev_sdk_consumer.py', '--sdk-root', sys.argv[1], '--output', sys.argv[2]]
runpy.run_path(str(pathlib.Path(__file__).with_name('test_dev_sdk_consumer.py')), run_name='__main__')
''')
    arguments = [str(probe), str(relocated), str(work / 'consumer build with spaces')]
    command = None if target_os == 'windows' else make_command(sandbox, work, hidden, sys.executable, arguments)
    env = None
    if target_os == 'linux':
        command[command.index('--'):command.index('--')] = [
            '--setenv', 'CRT_TEST_HIDDEN_ROOTS', json.dumps([str(p) for p in hidden]),
            '--setenv', 'CRT_TEST_HIDDEN_FILES', json.dumps([str(p) for p in protected_files])]
    elif target_os == 'macos':
        env = macos_environment(work, hidden)
        env['CRT_TEST_HIDDEN_FILES'] = json.dumps([str(p) for p in protected_files])
    else:
        env = windows_environment(work, hidden)
        env['CRT_TEST_HIDDEN_FILES'] = json.dumps([str(p) for p in protected_files])
        python = windows_python(work)
        command = [str(python), '-I', '-S', '-c', windows_bootstrap(), *arguments]
    (output / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
    if target_os == 'windows':
        try:
            windows.run(command, work=work, sid=sid, env=env)
        finally:
            if (work / 'consumer.log').is_file():
                shutil.copy2(work / 'consumer.log', output / 'consumer.log')
    else:
        with (output / 'consumer.log').open('w') as log:
            subprocess.run(command, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=600)
    visibility = json.loads((work / 'visibility.json').read_text())
    consumer = json.loads((work / 'consumer build with spaces/result.json').read_text())
    if not visibility.get('all_inaccessible') or not visibility.get('files_unreadable') or not consumer.get('passed'):
        raise ValueError('hidden-tree consumer validation did not complete')
    if target_os == 'windows' and not visibility.get('descendant_denial_checked'):
        raise ValueError('Windows descendant file denial was not proven')
    if inventory(sdk) != before or inventory(relocated) != before:
        raise ValueError('consumer replay changed original or relocated SDK payload')
    result = {'passed': True, 'isolation': {'linux': 'linux-bubblewrap', 'macos': 'macos-seatbelt',
                                           'windows': 'windows-restricted-token'}[target_os],
              'target': manifest['target'],
              'producer_source_and_build_hidden': True, 'original_sdk_hidden': True,
              'hidden_roots': visibility['hidden_roots'], 'visibility_probe': visibility, 'work_root': str(work),
              'sdk_inventory_sha256': inventory_digest(before), 'sdk_inventory_entries': len(before),
              'sdk_payload_unchanged': True, 'runtime_sha256': runtime_digests(sdk),
              'fixture_scripts': {p.name: digest(p) for p in scripts},
              'consumer': consumer}
    if target_os == 'windows':
        result['windows_host_fixture'] = {
            'launcher_sha256': digest(Path(windows.__file__)),
            'private_host_python': str(python), 'host_python_sha256': digest(python),
            'new_work_sid': sid, 'producer_acl_modified': False,
            'external_tools': {key: value for key, value in env.items()
                               if key in ('CRT_CC', 'CRT_CXX', 'CRT_AR', 'CRT_RANLIB',
                                          'CRT_TEST_OPENSSL', 'CRT_WINDOWS_SDK_LIBPATH')}}
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print('External Dev SDK: hidden producer/SDK, relocated static/shared HTTPS+inflate and trust negatives pass')


if __name__ == '__main__':
    main()
