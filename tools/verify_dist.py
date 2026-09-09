#!/usr/bin/env python3
"""Verify the structural and toolchain-boundary invariants of a CRT stage."""

import argparse
import json
import re
from pathlib import Path

from create_dist import DIST_PORTING_TOOLS, DIST_PORTING_DIRS, DIST_WRAPPER_TOOLS


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

    for relative in ("manifest.json", "VERSION", "LICENSE.md", "crt-toolchain.cmake"):
        require(dist / relative)
    for name in DIST_WRAPPER_TOOLS:
        require(dist / "tools" / name)
    manifest = json.loads((dist / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("stage") != args.stage or manifest.get("compiler_bundled") is not False:
        raise SystemExit("manifest stage/compiler_bundled contract is invalid")
    porting = manifest.get("optional_tools", {}).get("porting", {})
    if porting.get("included") is not True or porting.get("python", {}).get("required") is not True:
        raise SystemExit("manifest does not describe the optional packaged porting tools")
    shell = manifest.get("shell_environment", {})
    if manifest.get("target", {}).get("os") == "windows":
        if (shell.get("provider") != "crt-mksh-toybox" or
                shell.get("external_posix_shell_required") is not False or
                shell.get("crt_shell_default") is not True):
            raise SystemExit("Windows manifest does not declare the CRT-owned shell environment")
    elif (shell.get("provider") != "host-posix-shell" or
          shell.get("external_posix_shell_required") is not True or
          shell.get("crt_shell_default") is not False):
        raise SystemExit("Linux/macOS manifest does not declare the host-shell policy")
    for name in DIST_PORTING_TOOLS:
        require(dist / "tools" / name)
    for directory in DIST_PORTING_DIRS:
        target = dist / "porting" / directory
        require(target)
        if not any(target.iterdir()):
            raise SystemExit(f"porting/{directory} was packaged empty")
    for relative in (
            "porting/README.md",
            "porting/recipes/zlib.json", "porting/recipes/make.json",
            "porting/tests/zlib_roundtrip.c"):
        require(dist / relative)
    if args.stage == "01-c":
        recipe_path = dist / "stages" / "recipes" / "02-cxx.json"
        require(recipe_path)
        recipe = json.loads(recipe_path.read_text(encoding="utf-8"))
        source = recipe.get("source", {})
        if (recipe.get("input_stage") != "01-c" or
                recipe.get("output_stage") != "02-cxx" or
                not re.fullmatch(r"[0-9a-f]{64}", source.get("sha256", ""))):
            raise SystemExit("01-c does not contain a valid pinned 02-cxx stage recipe")
    make_recipe = json.loads((dist / "porting" / "recipes" / "make.json").read_text(encoding="utf-8"))
    make_source = make_recipe.get("source", {})
    # A real sha256 pin is preferred, but porting/recipes/make.json's own
    # notes document a genuine exception: its source is a Gitiles
    # `+archive` tarball, and Gitiles does not produce byte-stable gzip
    # output for the same commit across separate downloads (confirmed for
    # real, 2026-09-09 -- pinning a sha256 here broke the very next fresh
    # fetch with a real mismatch, same extracted content, different
    # archive bytes). The commit hash already embedded in the pinned
    # `.../+archive/<40-hex-chars>.tar.gz` URL is this recipe's own
    # equivalent integrity anchor -- the content is cryptographically tied
    # to that immutable git commit regardless of the archive container's
    # own byte instability -- so accept either form here rather than
    # forcing a checksum this one source genuinely cannot supply.
    commit_pinned_url = re.search(r"/[0-9a-f]{40}\.tar\.gz(?:\?|$)", make_source.get("url", ""))
    if not make_source.get("sha256") and not commit_pinned_url:
        raise SystemExit("packaged make recipe pins neither its source archive SHA-256 nor a commit-locked URL")
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
    for tool in ("mksh", "sh", "make", "awk", "toybox"):
        require(dist / "system" / "bin" / f"{tool}{suffix}")
        if suffix:
            require(dist / "system" / "bin" / tool)
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
        require(dist / "examples" / "README.md")
        require(dist / "examples" / "gfx-simple" / "main.c")
        require(dist / "examples" / "gfx-simple" / "CMakeLists.txt")
        require(dist / "examples" / "bin" / f"crtgfx_window_demo{suffix}")
    if args.stage == "03-gfx-simple":
        if (dist / "include" / "crtgfx" / "gpu.h").exists() or (dist / "include" / "crtgfx" / "skia.h").exists():
            raise SystemExit("Simple Graphics unexpectedly exposes GPU/Skia headers")
        advanced_libraries = [
            path for directory in (dist / "lib", dist / "bin", dist / "examples") if directory.is_dir()
            for path in directory.rglob("*") if path.is_file()
            if "crtgfx_gpu" in path.name.lower() or "crtgfx_skia" in path.name.lower() or
               "gfx-gpu" in path.as_posix().lower() or "gfx-skia" in path.as_posix().lower()
        ]
        if advanced_libraries:
            raise SystemExit("Simple Graphics contains advanced libraries: " +
                             ", ".join(map(str, advanced_libraries)))
    if args.stage >= "04-gfx-media":
        require(dist / "include" / "crtgfx" / "gpu.h")
        require(dist / "include" / "crtmedia")
        require(dist / "examples" / "gfx-gpu" / "main.c")
        require(dist / "examples" / "gfx-gpu" / "CMakeLists.txt")
        require(dist / "examples" / "bin" / f"crtgfx_gpu_window_demo{suffix}")
        if (dist / "include" / "crtgfx" / "skia.h").exists():
            require(dist / "examples" / "gfx-skia" / "main.cc")
            require(dist / "examples" / "gfx-skia" / "CMakeLists.txt")
            require(dist / "examples" / "gfx-skia" / "skia_reference_scene.h")
            require(dist / "examples" / "bin" / f"crtgfx_skia_gpu_window_demo{suffix}")
    if args.stage >= "05-js":
        require(dist / "include" / "crtjs")
    print(f"CRT distribution verified: {dist}")


if __name__ == "__main__":
    main()
