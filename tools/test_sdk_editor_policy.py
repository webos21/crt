#!/usr/bin/env python3
"""Compare real Linux CLI compile commands (x86_64 or aarch64, native) with an installed Dev SDK policy.

Inputs are public release files independently verified against publisher pins.
This producer replay does not implement or invoke a compilation database API.
"""

import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import shutil
import subprocess

from crt_sdk_install_metadata import content_sha256, inventory

ROOT = Path(__file__).resolve().parents[1]
NATIVE_ARCH = {'x86_64': 'x86_64', 'amd64': 'x86_64', 'aarch64': 'aarch64', 'arm64': 'aarch64'}.get(platform.machine().lower())
TARGET = {'os': 'linux', 'arch': NATIVE_ARCH}
MODES = {
    'app-static': ('executable', 'static', 'executable-static'),
    'app-shared': ('executable', 'shared', 'executable-shared'),
    'archive': ('static-library', 'static', 'static-library'),
    'module': ('shared-library', 'shared', 'shared-library'),
}
SOURCES = {'소스/probe.c': ('c', 'clang', '-std=c99'), '소스/main.cc': ('cxx', 'clang++', '-std=c++17')}


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def write(path, data):
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def compare_compile(argv, recipe, substitutions, expected_flags):
    """Check all policy arguments in order, with one slot for project flags."""
    def expand(arg):
        for token, value in substitutions.items():
            arg = arg.replace(token, value)
        return arg

    marker = recipe.index('@EXTRA@')
    prefix = [expand(arg) for arg in recipe[:marker]]
    suffix = [expand(arg) for arg in recipe[marker + 1:]]
    require(argv[:len(prefix)] == prefix and argv[-len(suffix):] == suffix,
            'CLI compile argv differs from the SDK compile policy')
    extras = argv[len(prefix):-len(suffix)]
    require(all(flag in extras for flag in expected_flags), 'missing project language/include flags')
    require(not any(arg.startswith(('-Wl,', '-l', '-L', '--ld-path=')) or
                    arg.endswith(('.a', '.so')) or arg == '-shared' for arg in argv),
            'link inputs leaked into the compile command')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('cli', 'archive', 'manifest', 'output', 'llvm-bin'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--manifest-sha256', required=True)
    parser.add_argument('--contract-sha256', required=True)
    parser.add_argument('--cli-sha256', required=True, help='exact executable digest from the verified CLI archive')
    parser.add_argument('--hide-root', action='append', type=Path, default=[])
    args = parser.parse_args()
    require(platform.system() == 'Linux' and NATIVE_ARCH is not None, 'native Linux x86_64 or aarch64 required')
    require(sha(args.cli) == args.cli_sha256, 'CLI executable SHA-256 mismatch')
    require(sha(args.manifest) == args.manifest_sha256, 'release manifest SHA-256 mismatch')
    manifest = read(args.manifest)
    require(manifest['target'] == TARGET and manifest['profile'] == 'dev', 'expected native Dev manifest')
    entry, = (entry for entry in manifest['assets'] if entry['name'] == args.archive.name)
    contract_path = args.manifest.with_name(entry['contract'])
    require(sha(contract_path) == args.contract_sha256, 'contract SHA-256 mismatch')
    require(sha(args.archive) == entry['sha256'], 'SDK archive SHA-256 mismatch')
    contract = read(contract_path)
    expected_content = entry['install']['content_sha256']
    require(contract['schema'] == 'crt.experimental.native-toolchain' and contract['format'] == 1 and
            contract['target'] == TARGET and contract['sdk_content_sha256'] == expected_content and
            contract['producer_source_commit'] == manifest['source_commit'], 'SDK and portable policy identities disagree')
    llvm = args.llvm_bin.absolute()
    tools = ['--cc', llvm / 'clang', '--cxx', llvm / 'clang++',
             '--linker', llvm / 'ld.lld', '--ar', llvm / 'llvm-ar']
    resource = {}
    for clang in ('clang', 'clang++'):
        resource[clang] = Path(subprocess.check_output([llvm / clang, '-print-resource-dir'], text=True).strip())
    bwrap = shutil.which('bwrap')
    require(bwrap, 'bubblewrap is required')
    work = args.output.resolve()
    hidden = [ROOT, *[path.resolve() for path in args.hide_root]]
    require(all(not work.is_relative_to(path) for path in hidden), 'use an evidence directory outside hidden trees')
    require(all(not candidate.resolve().is_relative_to(path) for path in hidden
                for candidate in (args.cli, args.manifest, args.archive, llvm)), 'tool or asset is inside a hidden tree')
    work.mkdir(parents=True)
    home = work / 'home 한글'
    home.mkdir()
    project = work / 'project space 한글' / 'editor-probe'
    project.parent.mkdir()
    result = {'passed': False, 'target': TARGET, 'steps': [], 'compile_commands': [],
              'sdk': {'version': manifest['version'], 'content_sha256': expected_content,
                      'producer_source_commit': manifest['source_commit'], 'archive_sha256': entry['sha256'],
                      'manifest_sha256': args.manifest_sha256, 'contract_sha256': args.contract_sha256},
              'cli_sha256': args.cli_sha256,
              'llvm': subprocess.check_output([llvm / 'clang', '--version'], text=True).splitlines()[0]}
    sandbox = [bwrap, '--die-with-parent', '--unshare-user', '--unshare-net', '--ro-bind', '/', '/',
               '--proc', '/proc', '--dev', '/dev', '--tmpfs', '/tmp', '--bind', str(work), str(work),
               '--clearenv', '--setenv', 'HOME', str(home), '--setenv', 'PATH', '/nonexistent',
               '--setenv', 'LC_ALL', 'C.UTF-8', '--chdir', str(work)]
    for path in hidden:
        sandbox += ['--tmpfs', str(path)]
    store = home / '.crt/sdks'
    readonly_store = []

    def run(label, argv, *, failure=None, cli=False):
        argv = list(map(str, argv))
        proc = subprocess.run([*sandbox, *readonly_store, '--', *argv],
                              text=True, capture_output=True, timeout=90)
        (work / (label + '.stdout')).write_text(proc.stdout, encoding='utf-8')
        (work / (label + '.stderr')).write_text(proc.stderr, encoding='utf-8')
        result['steps'].append({'name': label, 'argv': argv, 'exit': proc.returncode})
        if cli:
            reply = json.loads(proc.stdout)
            require(reply['schema'] == 'crt-cli.bootstrap-result' and reply['format'] == 1,
                    f'{label}: unexpected CLI envelope')
            require(reply['ok'] == (failure is None), f'{label}: {reply}')
            data = reply['data']
            if failure:
                require(proc.returncode != 0 and failure in data.get('message', ''), f'{label}: {data}')
            else:
                require(proc.returncode == 0, f'{label} failed')
            return data
        require(proc.returncode == 0, f'{label} failed: {proc.stderr[-1000:]}')
        return proc

    def invoke(label, *options, failure=None):
        return run(label, [args.cli.resolve(), '--json', *options], failure=failure, cli=True)

    try:
        for index, path in enumerate(hidden):
            run(f'checkout-hidden-{index}', ['/usr/bin/test', '!', '-e', path / 'TODO.md'])
        installed = invoke('install', 'sdk', 'install', '--archive', args.archive.resolve(),
                           '--release-manifest', args.manifest.resolve(),
                           '--release-manifest-sha256', args.manifest_sha256, '--store', store)
        identity = installed['identity']
        require(identity['content_sha256'] == expected_content, 'installed SDK content mismatch')
        sdk = store / identity['directory']
        require(content_sha256(inventory(sdk)) == expected_content, 'installed inventory mismatch')
        readonly_store.extend(['--ro-bind', str(store), str(store)])
        invoke('new', 'new', project)
        declaration_path = project / 'crt.json'
        lock_path = project / 'crt.lock'
        declaration = read(declaration_path)
        declaration.pop('tests')
        declaration['sdk']['version'] = manifest['version']
        declaration['targets'] = [{'name': name, 'kind': kind, 'runtime_linkage': runtime, 'sources': list(SOURCES)}
                                  for name, (kind, runtime, _) in MODES.items()]
        (project / '소스').mkdir()
        fixture = ROOT / 'tools/fixtures/sdk_editor'
        shutil.copy2(fixture / 'editor_probe.h', project / 'include')
        for source in SOURCES:
            shutil.copy2(fixture / Path(source).name, project / source)
        write(declaration_path, declaration)
        manifest_before = declaration_path.read_bytes()
        unresolved = lock_path.read_bytes()
        invalid = json.loads(manifest_before)
        invalid['targets'][-1]['runtime_linkage'] = 'static'
        write(declaration_path, invalid)
        selection_args = ['--project', project, '--sdk', identity['directory'], *tools]
        invoke('unsupported-mode', 'configure', *selection_args, failure='shared runtime linkage')
        require(lock_path.read_bytes() == unresolved and not (project / '.crt/local.json').exists() and
                not (project / 'out').exists(), 'invalid mode wrote project outputs')
        declaration_path.write_bytes(manifest_before)
        configured = invoke('configure', 'configure', *selection_args)
        selection = configured['selection']
        expected_selection = {'profile': 'dev', 'target': TARGET, 'version': manifest['version'],
                              'sdk_content_sha256': expected_content, 'contract_sha256': args.contract_sha256,
                              'producer_source_commit': manifest['source_commit'], 'contract_source': 'store'}
        require(all(selection.get(k) == v for k, v in expected_selection.items()), 'unexpected resolved selection')
        require(read(lock_path)['selection'] == selection, 'configure result and lock differ')
        binding = read(project / '.crt/local.json')
        drivers = {'c': binding['cc'], 'cxx': binding['cxx']}
        require(all(Path(drivers[lang]).resolve() == (llvm / compiler).resolve() and
                    Path(binding['resource_dir']).resolve() == resource[compiler].resolve()
                    for lang, compiler in [('c', 'clang'), ('cxx', 'clang++')]),
                'configured driver or resource path differs from the declared LLVM')
        locked = lock_path.read_bytes()
        cold = invoke('cold-build', 'build', '--project', project)
        require(cold['selection'] == selection, 'build and lock identities differ')
        seen = set()
        for argv in cold['commands']:
            sources = [source for source in SOURCES if str(project / source) in argv]
            if not sources:
                continue
            source, = sources
            language, compiler, standard = SOURCES[source]
            require(argv[0] == drivers[language] and argv[1:3] == ['-MD', '-MF'],
                    'unexpected driver or depfile arguments')
            output = Path(argv[argv.index('-o') + 1])
            name, = (name for name in MODES if output.name.startswith(name + '-'))
            require((name, source) not in seen, 'duplicate compile entry')
            seen.add((name, source))
            mode = MODES[name][2]
            compare_compile(argv[4:], contract['modes'][mode]['compile_' + language],
                            {'@SDK@': str(sdk), '@RESOURCE@': str(resource[compiler]),
                             '@SOURCE@': str(project / source), '@OUTPUT@': str(output)},
                            (standard, '-I' + str(project / 'include')))
            # Use the actual compile argv for a read-only syntax and header trace.
            syntax = argv[4:]
            syntax = syntax[:syntax.index('-o')] + syntax[syntax.index('-o') + 2:]
            syntax.remove('-c')
            trace = run(f'{name}-{language}-headers', [argv[0], *syntax, '-fsyntax-only', '-H'])
            headers = [Path(p).resolve() for p in re.findall(r'^\.+ (.+)$', trace.stderr, re.MULTILINE)]
            required = sdk / ('include/zlib.h' if language == 'c' else 'include/c++/v1/filesystem')
            require(required.resolve() in headers and headers, f'{name}/{language}: SDK header not selected')
            require(all(any(path.is_relative_to(base) for base in (sdk, resource[compiler], project))
                        for path in headers), f'{name}/{language}: host header escaped the SDK policy')
            result['compile_commands'].append({'mode': mode, 'file': str(project / source),
                                               'directory': str(work), 'arguments': argv})
        require(seen == {(name, source) for name in MODES for source in SOURCES},
                'did not capture all C/C++ compile modes')
        for app in ('app-static', 'app-shared'):
            invoke(app + '-run', 'run', '--project', project, '--target', app)
        warm = invoke('warm-build', 'build', '--project', project)
        require(warm['cache']['executed'] == 0 and warm['selection'] == selection, 'warm build changed selection or recompiled')
        require(declaration_path.read_bytes() == manifest_before and lock_path.read_bytes() == locked, 'build changed manifest or lock')
        stale = read(lock_path)
        stale['selection']['contract_sha256'] = '0' * 64
        write(lock_path, stale)
        stale_before = lock_path.read_bytes()
        build_tree = project / 'out/native'
        before_failure = inventory(build_tree)
        binding_before = (project / '.crt/local.json').read_bytes()
        try:
            invoke('stale-contract-lock', 'build', '--project', project, failure='toolchain contract digest mismatch')
            require(lock_path.read_bytes() == stale_before and declaration_path.read_bytes() == manifest_before and
                    (project / '.crt/local.json').read_bytes() == binding_before and inventory(build_tree) == before_failure,
                    'stale-lock failure changed project state')
        finally:
            lock_path.write_bytes(locked)
        require(content_sha256(inventory(sdk)) == expected_content, 'SDK inventory changed during replay')
        result['selection'] = selection
        result['passed'] = True
    finally:
        write(work / 'result.json', result)
    print(f"SDK editor policy: {len(result['compile_commands'])} native C/C++ compile commands passed")


if __name__ == '__main__':
    main()
