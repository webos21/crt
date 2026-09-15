#!/usr/bin/env python3
"""Classify packaged binary dependencies against the SDK manifest."""

from pathlib import Path, PurePosixPath, PureWindowsPath

from crt_elf import needed_libraries
from crt_pe import imported_libraries


def binary_dependencies(path: Path, target_os: str) -> list[str]:
    if target_os == "linux":
        return needed_libraries(path)
    if target_os == "windows":
        return imported_libraries(path)
    return []


def _embedded_path(name: str) -> bool:
    return (PurePosixPath(name).name != name or
            PureWindowsPath(name).name != name or
            PurePosixPath(name).is_absolute() or
            PureWindowsPath(name).is_absolute())


def _is_target_binary(path: Path, target_os: str) -> bool:
    with path.open("rb") as stream:
        magic = stream.read(4)
    return magic == b"\x7fELF" if target_os == "linux" else magic[:2] == b"MZ"


def validate_binary_dependencies(dist: Path, manifest: dict) -> None:
    """Require every ELF/PE dependency to be bundled or declared external."""
    target_os = manifest.get("target", {}).get("os")
    if target_os not in ("linux", "windows"):
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
            if _embedded_path(dependency):
                failures.append(f"{path.relative_to(dist)} -> path {dependency}")
            elif normalize(dependency) not in bundled | external:
                failures.append(f"{path.relative_to(dist)} -> undeclared {dependency}")
    if failures:
        raise SystemExit("unresolved packaged binary dependencies:\n  " + "\n  ".join(failures))
