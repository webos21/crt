"""App-local PE runtime closure for independently relocated Windows host tools."""
import hashlib
from pathlib import Path, PureWindowsPath

from crt_pe import imported_libraries


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def private_dll_name(dependency, system_components):
    if (not isinstance(dependency, str) or not dependency or any(c in dependency for c in '/\\:\0') or
            dependency.rstrip(' .') != dependency or not dependency.lower().endswith('.dll') or
            PureWindowsPath(dependency).is_reserved()):
        raise ValueError('non-flat host-tool PE dependency: ' + str(dependency))
    return None if dependency.casefold() in {name.casefold() for name in system_components} else dependency


def copy_alias_runtimes(root, aliases, tools, dependencies, system_components):
    private = [Path(name) for name in dependencies if name.lower().endswith('.dll')]
    for alias in aliases:
        if alias['kind'] != 'file' or not any(private_dll_name(name, system_components) is not None
                for name in dependencies.get(tools[alias['role']], [])):
            continue
        directory = root / Path(alias['path']).parent
        for library in private:
            source, target = root / library, directory / library.name
            matches = [p for p in directory.iterdir() if p.name.casefold() == library.name.casefold()]
            if len(matches) > 1:
                raise ValueError('case-colliding private alias DLL: ' + str(target))
            if matches:
                if not matches[0].is_file() or digest(matches[0]) != digest(source):
                    raise ValueError('conflicting private alias DLL: ' + str(target))
            else:
                import shutil
                shutil.copy2(source, target)


def verify_closure(root, manifest, native_image):
    host = manifest['host']
    system = [name for entry in manifest['external_prerequisites'] for name in entry['components']]
    hashes = {name: value for name, kind, value in manifest['inventory'] if kind == 'file'}
    canonical = {}
    for path in (root / 'system/bin').iterdir():
        if path.suffix.casefold() != '.dll':
            continue
        if private_dll_name(path.name, system) is None:
            raise ValueError('OS runtime cannot be bundled as a private DLL: ' + path.name)
        if path.name.casefold() in canonical:
            raise ValueError('case-colliding private DLL: ' + path.name)
        canonical[path.name.casefold()] = path
    pending, dependencies = list(manifest['tools'].values()), {}
    while pending:
        relative = pending.pop()
        if relative in dependencies:
            continue
        image = root / relative
        if not native_image(image, host):
            raise ValueError('non-native PE closure image: ' + relative)
        if image.suffix.casefold() == '.dll':
            data = image.read_bytes(); pe = int.from_bytes(data[60:64], 'little')
            if not int.from_bytes(data[pe + 22:pe + 24], 'little') & 0x2000:
                raise ValueError('private PE runtime is not a DLL: ' + relative)
        imports = imported_libraries(image)
        dependencies[relative] = imports
        for dependency in imports:
            name = private_dll_name(dependency, system)
            if name is None:
                continue
            target = canonical.get(name.casefold())
            if target is None:
                raise ValueError('missing canonical private DLL: ' + name)
            pending.append(target.relative_to(root).as_posix())
    if dependencies != manifest['dependencies']:
        raise ValueError('host-tools PE dependency graph mismatch')
    for relative in manifest['executable_files']:
        image = root / relative
        primary = next((path for path in manifest['tools'].values() if path == relative), None)
        if primary is None:
            alias = next(a for a in manifest['aliases'] if a['path'] == relative)
            primary = manifest['tools'][alias['role']]
            actual = hashes.get(relative) or digest(image)
            if actual != hashes[primary]:
                raise ValueError('PE alias differs from its primary: ' + relative)
        required, seen = list(dependencies[primary]), set()
        while required:
            name = private_dll_name(required.pop(), system)
            if name is None or name.casefold() in seen:
                continue
            seen.add(name.casefold())
            target = canonical[name.casefold()]
            adjacent = [p for p in image.parent.iterdir() if p.name.casefold() == name.casefold()]
            if len(adjacent) != 1 or not adjacent[0].is_file():
                raise ValueError('private DLL missing beside PE alias: ' + relative + ': ' + name)
            adjacent[0].resolve(strict=True).relative_to(root.resolve())
            if digest(adjacent[0]) != hashes[target.relative_to(root).as_posix()]:
                raise ValueError('private alias DLL differs from canonical closure: ' + name)
            required.extend(dependencies[target.relative_to(root).as_posix()])
