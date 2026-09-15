#!/usr/bin/env python3
"""Classify packaged binary dependencies against the SDK manifest."""

from pathlib import Path, PurePosixPath, PureWindowsPath

from crt_elf import needed_libraries
from crt_pe import imported_libraries
from crt_macho import MH_MAGIC, MH_MAGIC_64
from crt_macho import dependencies as macho_dependencies
from crt_macho import dylib_id as macho_dylib_id
from crt_macho import rpaths as macho_rpaths
from crt_macho import is_executable as macho_is_executable

import struct


def binary_dependencies(path: Path, target_os: str) -> list[str]:
    if target_os == "linux":
        return needed_libraries(path)
    if target_os == "windows":
        return imported_libraries(path)
    if target_os == "macos":
        return macho_dependencies(path)
    return []


def _embedded_path(name: str) -> bool:
    return (PurePosixPath(name).name != name or
            PureWindowsPath(name).name != name or
            PurePosixPath(name).is_absolute() or
            PureWindowsPath(name).is_absolute())


# Real macOS system roots an absolute Mach-O dependency is allowed to name --
# frameworks under /System/Library and plain dylibs under /usr/lib, exactly
# the two forms tools/crt_dist_prerequisites.py's own macos-* components
# declare. Anything else absolute (a private build-time temp path, say) is
# the same class of bug _embedded_path() above catches for ELF/PE: a path
# that leaked into a load command where only a portable reference belongs.
_MACHO_SYSTEM_ROOTS = ("/System/Library/", "/usr/lib/")


def _macho_embedded_path(name: str) -> bool:
    if name.startswith(("@rpath/", "@loader_path/", "@executable_path/")):
        return False
    if not PurePosixPath(name).is_absolute():
        return False
    return not name.startswith(_MACHO_SYSTEM_ROOTS)


def _macho_component(name: str) -> str:
    """Normalize one Mach-O dependency name to a manifest-comparable
    component: a bundled @rpath/@loader_path/@executable_path reference or
    a plain /usr/lib dylib both reduce to their basename (matching this
    project's own bundled-file-by-basename and declared-component
    spellings); a real framework path reduces to its own `Name.framework`
    bundle name (a framework is a directory, never one of this project's
    own bundled files, so it can only ever be classified as declared
    external)."""
    if "/Frameworks/" in name:
        return name.split("/Frameworks/", 1)[1].split("/", 1)[0]
    return PurePosixPath(name).name


_MACHO_MAGICS = frozenset(struct.pack("<I", magic) for magic in (MH_MAGIC, MH_MAGIC_64))


def _is_target_binary(path: Path, target_os: str) -> bool:
    with path.open("rb") as stream:
        magic = stream.read(4)
    if target_os == "linux":
        return magic == b"\x7fELF"
    if target_os == "windows":
        return magic[:2] == b"MZ"
    if target_os == "macos":
        return magic in _MACHO_MAGICS
    return False


def validate_binary_dependencies(dist: Path, manifest: dict) -> None:
    """Require every ELF/PE/Mach-O dependency to be bundled or declared
    external, every packaged macOS dylib's own self-identify to be a
    portable @rpath/... spelling, never an absolute build-time path, and
    every packaged macOS *executable* (MH_EXECUTE; a shared library is
    deliberately exempt, see the comment at its own check below) carrying
    an @rpath dependency to also carry at least one portable
    @loader_path/@executable_path RPATH entry -- an exclusively-absolute
    RPATH list resolves correctly only until the SDK is next moved (a
    real, confirmed bug, 2026-09-15: `mv`-ing a freshly published
    04-gfx-media SDK broke `crtgfx_skia_gpu_window_demo` outright, `dyld:
    Library not loaded: @rpath/libcrtgfx_skia.dylib`, because its only
    RPATH was the absolute path baked in at publish time). This mirrors
    the already-established Linux policy (every shared-runtime target's
    own $ORIGIN-first, absolute-fallback-second RPATH) rather than
    rejecting the absolute fallback outright, since a configure-time
    try_compile/try_run probe genuinely needs it and it is harmless once a
    portable entry sorts ahead of it."""
    target_os = manifest.get("target", {}).get("os")
    if target_os not in ("linux", "windows", "macos"):
        return
    normalize = str.lower if target_os == "windows" else str
    files = [path for path in dist.rglob("*") if path.is_file()]
    bundled = {
        normalize(path.name)
        for path in files
        if _is_target_binary(path, target_os)
    }
    external = {
        normalize(component)
        for prerequisite in manifest.get("external_prerequisites", [])
        for component in prerequisite.get("components", [])
        if isinstance(component, str)
    }
    failures = []
    for path in files:
        try:
            dependencies = binary_dependencies(path, target_os)
        except (OSError, UnicodeDecodeError, ValueError) as exc:
            raise SystemExit(f"cannot inspect binary dependencies in {path}: {exc}") from exc
        for dependency in dependencies:
            if target_os == "macos":
                if _macho_embedded_path(dependency):
                    failures.append(f"{path.relative_to(dist)} -> path {dependency}")
                    continue
                component = normalize(_macho_component(dependency))
            else:
                if _embedded_path(dependency):
                    failures.append(f"{path.relative_to(dist)} -> path {dependency}")
                    continue
                component = normalize(dependency)
            if component not in bundled | external:
                failures.append(f"{path.relative_to(dist)} -> undeclared {dependency}")
        if target_os == "macos":
            try:
                dylib_name = macho_dylib_id(path)
            except (OSError, UnicodeDecodeError, ValueError) as exc:
                raise SystemExit(f"cannot inspect dylib id in {path}: {exc}") from exc
            if dylib_name is not None and not dylib_name.startswith("@rpath/"):
                failures.append(f"{path.relative_to(dist)} -> absolute dylib id {dylib_name}")
            # Scoped to MH_EXECUTE only: dyld accumulates LC_RPATH entries
            # across the whole load chain, so a shared library resolving
            # its own @rpath dependency via whatever *executable* loads it
            # is by design here, not a gap of its own -- confirmed for
            # real, every one of this project's own shared libraries
            # (crt_configure_shared_runtime()'s macOS branch sets
            # INSTALL_RPATH "" deliberately) has always relied on exactly
            # this and keeps working correctly, move or no move, as long
            # as the loading executable's own RPATH is portable.
            if macho_is_executable(path) and any(
                    dependency.startswith("@rpath/") for dependency in dependencies):
                try:
                    entries = macho_rpaths(path)
                except (OSError, UnicodeDecodeError, ValueError) as exc:
                    raise SystemExit(f"cannot inspect rpaths in {path}: {exc}") from exc
                if not any(entry.startswith(("@loader_path", "@executable_path"))
                           for entry in entries):
                    failures.append(
                        f"{path.relative_to(dist)} -> @rpath dependency with no portable "
                        "@loader_path/@executable_path RPATH (stale after any future move)")
    if failures:
        raise SystemExit("unresolved packaged binary dependencies:\n  " + "\n  ".join(failures))
