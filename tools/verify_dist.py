#!/usr/bin/env python3
"""Verify the structural and toolchain-boundary invariants of a CRT stage."""

import argparse
import json
from pathlib import Path


BANNED_TOOLS = {
    "clang", "clang.exe", "clang++", "clang++.exe", "lld", "lld.exe",
    "ld.lld", "ld.lld.exe", "gcc", "gcc.exe", "g++", "g++.exe",
}


def require(path: Path) -> None:
    if not path.exists():
        raise SystemExit(f"distribution is missing: {path}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist", required=True, type=Path)
    parser.add_argument("--stage", required=True)
    args = parser.parse_args()
    dist = args.dist.resolve()

    for relative in ("manifest.json", "VERSION", "LICENSE.md", "tools/crt-cc",
                     "tools/crt-c++", "crt-toolchain.cmake"):
        require(dist / relative)
    manifest = json.loads((dist / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("stage") != args.stage or manifest.get("compiler_bundled") is not False:
        raise SystemExit("manifest stage/compiler_bundled contract is invalid")
    bundled = sorted(path for path in dist.rglob("*") if path.is_file() and path.name.lower() in BANNED_TOOLS)
    if bundled:
        raise SystemExit("compiler/linker executable was bundled: " + ", ".join(map(str, bundled)))

    require(dist / "include" / "stdio.h")
    require(dist / "system" / "bin")
    if not any((dist / "system" / "bin").glob("make*")):
        raise SystemExit("C stage does not contain make")
    if not any((dist / "lib").glob("libc.a")):
        raise SystemExit("C stage does not contain the static libc archive")
    suffix = ".exe" if manifest.get("target", {}).get("os") == "windows" else ""
    for applet in ("printf", "sed", "uname"):
        require(dist / "system" / "bin" / f"{applet}{suffix}")
        if suffix:
            require(dist / "system" / "bin" / applet)

    if args.stage >= "02-cxx":
        require(dist / "include" / "c++" / "v1")
        if not any((dist / "lib").glob("libc++*")):
            raise SystemExit("C++ stage does not contain libc++")
    if args.stage >= "03-gfx-simple":
        require(dist / "include" / "crtgfx" / "window.h")
    if args.stage == "03-gfx-simple":
        if (dist / "include" / "crtgfx" / "gpu.h").exists() or (dist / "include" / "crtgfx" / "skia.h").exists():
            raise SystemExit("Simple Graphics unexpectedly exposes GPU/Skia headers")
    if args.stage >= "04-gfx-media":
        require(dist / "include" / "crtgfx" / "gpu.h")
        require(dist / "include" / "crtmedia")
    if args.stage >= "05-js":
        require(dist / "include" / "crtjs")
    print(f"CRT distribution verified: {dist}")


if __name__ == "__main__":
    main()
