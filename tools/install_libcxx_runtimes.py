#!/usr/bin/env python3
"""Stage a successfully built Android libc++ runtime into a CRT sysroot."""

import argparse
import shutil
from pathlib import Path


def copy_tree(source, destination):
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(source, destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install-prefix", required=True, type=Path)
    parser.add_argument("--sysroot", required=True, type=Path)
    args = parser.parse_args()

    install = args.install_prefix.resolve()
    sysroot = args.sysroot.resolve()
    headers = install / "include" / "c++" / "v1"
    libraries = install / "lib"
    if not headers.is_dir() or not libraries.is_dir():
        raise SystemExit("Android libc++ install is incomplete; run crt-libcxx-build successfully first")

    copy_tree(headers, sysroot / "include" / "c++" / "v1")
    copied = []
    for library in libraries.iterdir():
        if library.is_file() and (library.name.startswith("libc++") or library.name.startswith("libunwind")):
            shutil.copy2(library, sysroot / "lib" / library.name)
            copied.append(library.name)
    if not copied:
        raise SystemExit("Android libc++ install has no runtime libraries to stage")
    print("CRT libc++ staged into sysroot:", ", ".join(sorted(copied)))


if __name__ == "__main__":
    main()
