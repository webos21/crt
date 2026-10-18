#!/usr/bin/env python3
"""Preserve one ELF C++ runtime image across versioned/unversioned names."""
import hashlib
from pathlib import Path


def normalize_cxx_aliases(sdk: Path) -> None:
    directory = (sdk / "lib").resolve()
    for name in ("libc++", "libc++abi", "libunwind"):
        soname = directory / (name + ".so.1")
        if not soname.is_file():
            raise ValueError(f"missing shared C++ runtime: {soname}")
        primary = directory / (name + ".so.1.0")
        if not primary.is_file():
            primary = soname.resolve()
        primary = primary.resolve()
        if not primary.resolve().is_relative_to(directory):
            raise ValueError(f"C++ runtime alias escapes SDK: {primary}")
        expected = hashlib.sha256(primary.read_bytes()).digest()
        aliases = list(directory.glob(name + ".so*"))
        # Verify all bytes/targets before retiring any generated duplicate.
        for file in aliases:
            if not file.resolve().is_relative_to(directory) or hashlib.sha256(file.read_bytes()).digest() != expected:
                raise ValueError(f"C++ runtime aliases differ: {file}")
        for file in aliases:
            if file == primary:
                continue
            if file.is_symlink() and file.readlink() == Path(primary.name):
                continue
            file.unlink()
            file.symlink_to(primary.name)
