#!/usr/bin/env python3
"""Assemble an independently pinned CRT-native host-tools rootfs from a public SDK."""
import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import tempfile
from pathlib import Path

from create_rootfs import TOYBOX_APPLETS
from crt_binary_dependencies import binary_dependencies
from crt_dist_prerequisites import external_prerequisites_for
from crt_elf import runtime_paths, interpreter
from crt_macho import rpaths as macho_rpaths
from crt_runtime_notices import install_component, load_registry

ROOT = Path(__file__).resolve().parent.parent
SCHEMA = 'crt.experimental.host-tools'
ROLES = ('shell', 'toybox', 'awk', 'make')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical_hash(value):
    return hashlib.sha256(json.dumps(value, ensure_ascii=False, separators=(',', ':')).encode()).hexdigest()


def native_host():
    os_name = {'Linux': 'linux', 'Darwin': 'macos', 'Windows': 'windows'}.get(platform.system())
    arch = {'amd64': 'x86_64', 'x86_64': 'x86_64', 'arm64': 'aarch64', 'aarch64': 'aarch64'}.get(platform.machine().lower())
    if not os_name or not arch:
        raise ValueError('unsupported host-tools execution host')
    return {'os': os_name, 'arch': arch}


def native_image(path, host):
    data = path.read_bytes()
    arch, os_name = host['arch'], host['os']
    if os_name == 'linux':
        return len(data) > 20 and data[:4] == b'\x7fELF' and data[4:6] == b'\x02\x01' and int.from_bytes(data[18:20], 'little') == {'x86_64': 62, 'aarch64': 183}[arch]
    if os_name == 'macos':
        return len(data) > 8 and data[:4] == b'\xcf\xfa\xed\xfe' and int.from_bytes(data[4:8], 'little') == {'x86_64': 0x1000007, 'aarch64': 0x100000c}[arch]
    if len(data) < 64 or data[:2] != b'MZ':
        return False
    offset = int.from_bytes(data[60:64], 'little')
    return data[offset:offset+4] == b'PE\0\0' and int.from_bytes(data[offset+4:offset+6], 'little') == {'x86_64': 0x8664, 'aarch64': 0xaa64}[arch]


def payload_inventory(root):
    result = []
    for path in sorted(root.rglob('*'), key=lambda p: p.relative_to(root).parts):
        relative = path.relative_to(root).as_posix()
        if relative == 'manifest.json':
            continue
        if path.is_symlink():
            path.resolve(strict=True).relative_to(root.resolve())
            result.append([relative, 'symlink', os.readlink(path)])
        elif path.is_file():
            result.append([relative, 'file', digest(path)])
    return result


def assemble(sdk, output, version, make_source=None, project=ROOT):
    sdk, output = sdk.resolve(), output.absolute()
    manifest = json.loads((sdk / 'manifest.json').read_text(encoding='utf-8'))
    host = native_host()
    if manifest.get('target') != host:
        raise ValueError('host tools must come from an SDK for the execution host, not a cross target')
    if output.exists() or output.is_symlink():
        raise ValueError('host-tools output already exists; select a new candidate')
    output = output.resolve()
    if output == sdk or sdk in output.parents or output in sdk.parents:
        raise ValueError('host-tools output and source SDK must be disjoint')
    suffix = '.exe' if host['os'] == 'windows' else ''
    role_names = {'shell': 'mksh', 'toybox': 'toybox', 'awk': 'awk', 'make': 'make'}
    tools = {role: 'system/bin/' + name + suffix for role, name in role_names.items()}
    for relative in tools.values():
        path = sdk / relative
        path.resolve(strict=True).relative_to(sdk)
        if not native_image(path, host) or not os.access(path, os.X_OK):
            raise ValueError('not an executable native host tool: ' + relative)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix='.host-tools-', dir=output.parent))
    try:
        aliases = []
        def copy(relative):
            source, destination = sdk / relative, temporary / relative
            if not (source.is_file() or source.is_symlink()):
                raise ValueError('required host-tools input is missing: ' + relative)
            source.resolve(strict=True).relative_to(sdk)
            destination.parent.mkdir(parents=True, exist_ok=True)
            if source.is_symlink():
                target = os.readlink(source)
                if Path(target).is_absolute():
                    raise ValueError('absolute SDK alias: ' + relative)
                destination.symlink_to(target)
            else:
                shutil.copy2(source, destination)
        for relative in tools.values():
            copy(relative)
        for directory in ('system/bin', 'bin', 'usr/bin'):
            names = ['mksh', 'make', 'sh', 'awk', *TOYBOX_APPLETS] if directory == 'system/bin' else ['mksh', 'make', 'sh', 'awk']
            for name in names:
                variants = [name + suffix] + ([name] if suffix else [])
                for alias in variants:
                    relative = directory + '/' + alias
                    if relative in tools.values():
                        continue
                    copy(relative)
                    path = sdk / relative
                    role = 'shell' if name in ('sh', 'mksh') else 'make' if name == 'make' else 'awk' if name == 'awk' else 'toybox'
                    primary = sdk / tools[role]
                    if not os.access(path, os.X_OK) or digest(path) != digest(primary):
                        raise ValueError('alias differs from its declared CRT dispatcher: ' + relative)
                    aliases.append({'path': relative, 'role': role, 'kind': 'symlink' if path.is_symlink() else 'file',
                                    'target': os.readlink(path) if path.is_symlink() else tools[role]})
        copy('system/etc/mkshrc')
        external = external_prerequisites_for(host['os'], '01-c', host['arch'])
        allowed = {name.lower() for item in external for name in item['components']}
        dependencies, interpreters, pending, seen = {}, {}, list(tools.values()), set()
        while pending:
            relative = pending.pop()
            if relative in seen:
                continue
            seen.add(relative)
            path = temporary / relative
            imports = binary_dependencies(path, host['os'])
            dependencies[relative] = imports
            if host['os'] == 'linux':
                loader = interpreter(path)
                expected_loader = {'x86_64': '/lib64/ld-linux-x86-64.so.2', 'aarch64': '/lib/ld-linux-aarch64.so.1'}[host['arch']]
                if loader is not None:
                    if loader != expected_loader:
                        raise ValueError('undeclared ELF host-tools interpreter: ' + loader)
                    interpreters[relative] = loader
            if host['os'] == 'linux' and any(entry not in ('', '$ORIGIN') for entry in runtime_paths(path)):
                raise ValueError('host tool carries a runtime search path: ' + relative)
            if host['os'] == 'macos' and any(entry not in ('', '@loader_path') for entry in macho_rpaths(path)):
                raise ValueError('host tool carries a runtime search path: ' + relative)
            for dependency in imports:
                name = Path(dependency).name
                if name.lower() in allowed:
                    continue
                target = 'system/bin/' + name
                if target in seen or target in pending:
                    continue
                matches = [p for d in ('system/bin', 'bin', 'lib', 'system/lib') for p in (sdk / d).glob('*')
                           if p.is_file() and p.name.lower() == name.lower()]
                if not matches:
                    raise ValueError('undeclared host-tool runtime dependency: ' + dependency)
                if len({digest(p) for p in matches}) != 1:
                    raise ValueError('conflicting host-tool runtime dependency: ' + dependency)
                matches[0].resolve(strict=True).relative_to(sdk)
                shutil.copy2(matches[0], temporary / target)
                pending.append(target)
        if interpreters:
            external.append({'id': 'linux-elf-loader', 'kind': 'os-runtime', 'required_for': ['host-tools-executable-startup'],
                             'components': [Path(expected_loader).name], 'paths': [expected_loader], 'bundled': False})
        for name in ('crt-libc', 'llvm-runtimes', 'mksh', 'toybox', 'onetrue-awk'):
            if name == 'llvm-runtimes' and not (sdk / 'share/licenses' / name).is_dir():
                component = next(c for c in load_registry(project / 'third_party/crt-runtime/components.json')['components'] if c['name'] == name)
                install_component(project, temporary, component)
                continue
            for prefix in ('share/licenses', 'share/crt/dependencies'):
                source = sdk / prefix / name
                if not source.is_dir():
                    raise ValueError('host-tool notices/provenance are missing: ' + str(source))
                shutil.copytree(source, temporary / prefix / name, symlinks=True)
        make_recipe = json.loads((project / 'porting/recipes/make.json').read_text())
        copying = make_source / 'COPYING' if make_source else project / 'third_party/crt-runtime/make.COPYING'
        notice_pin = json.loads((project / 'third_party/crt-runtime/make.COPYING.source.json').read_text())
        if notice_pin['commit'] not in make_recipe['source']['url'] or digest(copying) != notice_pin['sha256']:
            raise ValueError('GNU make notice does not match the adopted source pin')
        sdk_recipe = json.loads((sdk / 'porting/recipes/make.json').read_text())
        if sdk_recipe['source'] != make_recipe['source'] or sdk_recipe['build'] != make_recipe['build']:
            raise ValueError('source SDK make recipe differs from the adopted host-tools recipe')
        notice = temporary / 'share/licenses/make'; notice.mkdir(parents=True)
        shutil.copy2(copying, notice / 'COPYING')
        provenance = temporary / 'share/crt/dependencies/make'; provenance.mkdir(parents=True)
        shutil.copy2(project / 'porting/recipes/make.json', provenance / 'recipe.json')
        (provenance / 'provenance.json').write_text(json.dumps({'source': make_recipe['source'], 'notice_sha256': digest(notice / 'COPYING'),
            'source_sdk_tool_sha256': digest(sdk / tools['make'])}, indent=2) + '\n', encoding='utf-8')
        shutil.copy2(sdk / 'LICENSE.md', temporary / 'LICENSE.md')
        files = payload_inventory(temporary)
        result = {'schema': SCHEMA, 'format': 1, 'promotion': 'local-only', 'version': version, 'host': host,
                  'assembler': {'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=project, text=True).strip(),
                                'working_tree_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=project)),
                                'script_sha256': digest(Path(__file__))},
                  'source_sdk': {'manifest_sha256': digest(sdk / 'manifest.json'), 'source_commit': manifest.get('source_commit'),
                                 'target': host, 'version': (sdk / 'VERSION').read_text().strip()},
                  'tools': tools, 'applets': TOYBOX_APPLETS, 'aliases': aliases,
                  'executable_files': sorted(set(tools.values()) | {a['path'] for a in aliases}), 'dependencies': dependencies,
                  'external_prerequisites': external, 'interpreters': interpreters,
                  'rootfs': {'layout': ['system', 'bin', 'usr'], 'writable': ['tmp', 'data/local', 'data/local/tmp'],
                             'shell': '/system/bin/sh' + suffix, 'make': '/system/bin/make' + suffix, 'awk': '/system/bin/awk' + suffix},
                  'inventory': files, 'content_sha256': canonical_hash(files)}
        (temporary / 'manifest.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        # Validate links after all aliases and their targets are present.
        payload_inventory(temporary)
        temporary.rename(output)
        return result
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


def verify_bundle(root):
    root = root.resolve()
    manifest = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))
    if manifest.get('schema') != SCHEMA or manifest.get('format') != 1 or manifest.get('host') != native_host():
        raise ValueError('unsupported host-tools manifest/host')
    actual = payload_inventory(root)
    if actual != manifest['inventory'] or canonical_hash(actual) != manifest['content_sha256']:
        raise ValueError('host-tools inventory mismatch')
    executable_files = set(manifest['tools'].values()) | {a['path'] for a in manifest['aliases']}
    if sorted(executable_files) != manifest['executable_files']:
        raise ValueError('host-tools executable inventory mismatch')
    for relative in executable_files:
        path = root / relative
        path.resolve(strict=True).relative_to(root)
        if not os.access(path, os.X_OK) or not native_image(path, manifest['host']):
            raise ValueError('host-tool alias is not executable/native: ' + relative)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--make-source', type=Path, help='optional pristine make cache; its COPYING must match the pinned notice')
    args = parser.parse_args()
    result = assemble(args.sdk_root, args.output, args.version, args.make_source)
    print(json.dumps({'output': str(args.output.resolve()), 'manifest_sha256': digest(args.output / 'manifest.json'),
                      'content_sha256': result['content_sha256'], 'host': result['host']}, indent=2))


if __name__ == '__main__':
    main()
