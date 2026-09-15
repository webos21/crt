#!/usr/bin/env python3
"""Verify the structural and toolchain-boundary invariants of a CRT stage."""

import argparse
import json
import re
from pathlib import Path, PurePosixPath, PureWindowsPath

from create_dist import DIST_PORTING_TOOLS, DIST_PORTING_DIRS, DIST_WRAPPER_TOOLS
from crt_dist_prerequisites import external_prerequisites_for
from crt_elf import is_absolute_runtime_entry, runtime_paths
from crt_stage_recipe import recipe_filename_for_stage, validate_recipe


BANNED_TOOLS = {
    "clang", "clang.exe", "clang++", "clang++.exe", "lld", "lld.exe",
    "ld.lld", "ld.lld.exe", "gcc", "gcc.exe", "g++", "g++.exe",
}

SUPPORTED_STAGES = (
    "01-c", "02-cxx", "03-gfx-simple", "04-gfx-media", "05-js",
)
SUPPORTED_TARGET_OSES = {"linux", "macos", "windows"}
EXTERNAL_PREREQUISITE_KINDS = {
    "device-driver", "host-library", "os-framework", "os-runtime",
    "runtime-service",
}


def require(path: Path) -> None:
    if not path.exists():
        raise SystemExit(f"distribution is missing: {path}")


def require_any(directory: Path, patterns: tuple[str, ...], description: str) -> None:
    if not any(path.is_file() for pattern in patterns for path in directory.glob(pattern)):
        raise SystemExit(f"distribution is missing {description} under {directory}")


def require_string(value: object, description: str) -> str:
    if not isinstance(value, str) or not value:
        raise SystemExit(f"manifest {description} must be a non-empty string")
    return value


def require_string_list(value: object, description: str) -> list[str]:
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise SystemExit(f"manifest {description} must be a string list")
    return value


def validate_elf_runtime_paths(dist: Path) -> None:
    """Reject checkout/build paths embedded in packaged Linux ELF files."""
    for path in dist.rglob("*"):
        if not path.is_file():
            continue
        try:
            paths = runtime_paths(path)
        except (OSError, UnicodeDecodeError, ValueError) as exc:
            raise SystemExit(f"cannot inspect ELF runtime path in {path}: {exc}") from exc
        for value in paths:
            absolute = [entry for entry in value.split(":")
                        if is_absolute_runtime_entry(entry)]
            if absolute:
                relative = path.relative_to(dist)
                raise SystemExit(
                    f"distribution ELF {relative} contains absolute RPATH/RUNPATH: "
                    + ", ".join(absolute))


def validate_manifest_schema(manifest: object, expected_stage: str) -> dict:
    """Validate host-independent manifest structure before stage policy."""
    if not isinstance(manifest, dict):
        raise SystemExit("manifest root must be an object")
    if expected_stage not in SUPPORTED_STAGES:
        raise SystemExit(f"unsupported distribution stage: {expected_stage}")
    if manifest.get("format") != 1:
        raise SystemExit("manifest format must be 1")
    stage = require_string(manifest.get("stage"), "stage")
    if stage not in SUPPORTED_STAGES or stage != expected_stage:
        raise SystemExit("manifest stage does not match the requested stage")
    if manifest.get("compiler_bundled") is not False:
        raise SystemExit("manifest compiler_bundled must be false")

    target = manifest.get("target")
    if not isinstance(target, dict):
        raise SystemExit("manifest target must be an object")
    target_os = require_string(target.get("os"), "target.os")
    if target_os not in SUPPORTED_TARGET_OSES:
        raise SystemExit(f"manifest target.os is unsupported: {target_os}")
    require_string(target.get("arch"), "target.arch")
    require_string(manifest.get("target_triple"), "target_triple")
    require_string(manifest.get("created_utc"), "created_utc")

    tools = manifest.get("external_tools_used_to_build")
    if not isinstance(tools, dict):
        raise SystemExit("manifest external_tools_used_to_build must be an object")
    for name in ("cc", "cxx", "ar", "ranlib"):
        require_string(tools.get(name), f"external_tools_used_to_build.{name}")
    require_string_list(manifest.get("default_compile_options"),
                        "default_compile_options")
    require_string_list(manifest.get("external_toolchain_environment"),
                        "external_toolchain_environment")
    return manifest


def dependency_path(dist: Path, name: str, field: str, relative: str) -> Path:
    """Resolve one portable manifest path and keep it inside the SDK root."""
    posix = PurePosixPath(relative)
    windows = PureWindowsPath(relative)
    if (not relative or "\\" in relative or posix.is_absolute() or
            windows.is_absolute() or windows.drive or
            posix.as_posix() != relative or ".." in posix.parts):
        raise SystemExit(
            f"redistributed dependency {name}.{field} has an unsafe path: {relative!r}")
    candidate = dist / Path(*posix.parts)
    try:
        candidate.resolve().relative_to(dist.resolve())
    except ValueError as exc:
        raise SystemExit(
            f"redistributed dependency {name}.{field} escapes the distribution: "
            f"{relative!r}") from exc
    return candidate


def validate_redistributed_dependencies(dist: Path, manifest: dict) -> dict[str, dict]:
    """Validate every declared dependency without hardcoding its file layout."""
    dependencies = manifest.get("redistributed_dependencies", [])
    if not isinstance(dependencies, list):
        raise SystemExit("manifest redistributed_dependencies must be a list")
    by_name: dict[str, dict] = {}
    for item in dependencies:
        if not isinstance(item, dict):
            raise SystemExit("redistributed dependency entries must be objects")
        name = require_string(item.get("name"), "redistributed dependency name")
        if name in by_name:
            raise SystemExit(f"redistributed dependency is declared twice: {name}")
        require_string(item.get("kind"), f"redistributed dependency {name}.kind")
        for field in ("headers", "link_artifacts", "runtime_artifacts", "notices"):
            paths = require_string_list(item.get(field),
                                        f"redistributed dependency {name}.{field}")
            for relative in paths:
                require(dependency_path(dist, name, field, relative))
        provenance = require_string(item.get("provenance"),
                                    f"redistributed dependency {name}.provenance")
        require(dependency_path(dist, name, "provenance", provenance))
        by_name[name] = item
    return by_name


def validate_external_prerequisites(manifest: dict) -> dict[str, dict]:
    """Require the canonical cumulative, non-redistributed runtime contract."""
    prerequisites = manifest.get("external_prerequisites")
    if not isinstance(prerequisites, list):
        raise SystemExit("manifest external_prerequisites must be a list")
    by_id: dict[str, dict] = {}
    for item in prerequisites:
        if not isinstance(item, dict):
            raise SystemExit("external prerequisite entries must be objects")
        prerequisite_id = require_string(item.get("id"),
                                         "external prerequisite id")
        if prerequisite_id in by_id:
            raise SystemExit(
                f"external prerequisite is declared twice: {prerequisite_id}")
        kind = require_string(item.get("kind"),
                              f"external prerequisite {prerequisite_id}.kind")
        if kind not in EXTERNAL_PREREQUISITE_KINDS:
            raise SystemExit(
                f"external prerequisite {prerequisite_id}.kind is unsupported: {kind}")
        if item.get("bundled") is not False:
            raise SystemExit(
                f"external prerequisite {prerequisite_id}.bundled must be false")
        if not require_string_list(
                item.get("required_for"),
                f"external prerequisite {prerequisite_id}.required_for"):
            raise SystemExit(
                f"external prerequisite {prerequisite_id}.required_for must not be empty")
        if not require_string_list(
                item.get("components"),
                f"external prerequisite {prerequisite_id}.components"):
            raise SystemExit(
                f"external prerequisite {prerequisite_id}.components must not be empty")
        by_id[prerequisite_id] = item

    target_os = manifest["target"]["os"]
    stage = manifest["stage"]
    expected = external_prerequisites_for(target_os, stage)
    expected_by_id = {item["id"]: item for item in expected}
    if by_id.keys() != expected_by_id.keys():
        missing = sorted(expected_by_id.keys() - by_id.keys())
        unexpected = sorted(by_id.keys() - expected_by_id.keys())
        raise SystemExit(
            "manifest external_prerequisites do not match the target/stage contract; "
            f"missing={missing}, unexpected={unexpected}")
    for prerequisite_id, expected_item in expected_by_id.items():
        if by_id[prerequisite_id] != expected_item:
            raise SystemExit(
                f"external prerequisite contradicts the target/stage contract: "
                f"{prerequisite_id}")
    return by_id


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
    manifest = validate_manifest_schema(
        json.loads((dist / "manifest.json").read_text(encoding="utf-8")),
        args.stage)
    validate_external_prerequisites(manifest)
    redistributed = validate_redistributed_dependencies(dist, manifest)
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
    if args.stage in ("01-c", "02-cxx"):
        recipe_path = dist / "stages" / "recipes" / recipe_filename_for_stage(args.stage)
        require(recipe_path)
        recipe = json.loads(recipe_path.read_text(encoding="utf-8"))
        try:
            validate_recipe(recipe)
        except ValueError as exc:
            raise SystemExit(f"{args.stage} contains an invalid successor recipe: {exc}") from exc
        if (recipe["input_stage"] != args.stage or
                recipe["target"]["os"] != manifest.get("target", {}).get("os")):
            raise SystemExit(f"{args.stage} contains a successor recipe for the wrong stage or OS")
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
    if manifest.get("target", {}).get("os") == "linux":
        validate_elf_runtime_paths(dist)

    require(dist / "include" / "stdio.h")
    require(dist / "system" / "bin")
    # An extracted SDK is also the Windows CRT_ROOTFS.  Both general POSIX
    # temporary-file users (/tmp) and crt_mksh heredocs (/data/local) need
    # their writable namespace present before any configure script runs.
    for relative in ("tmp", "data/local", "data/local/tmp"):
        require(dist / relative)
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
        if manifest.get("target", {}).get("os") == "linux":
            if "xkbcommon" not in redistributed:
                raise SystemExit("Linux Simple Graphics does not declare its xkbcommon port")
            for relative in (
                    "include/xkbcommon/xkbcommon.h",
                    "lib/libxkbcommon.a",
                    "share/licenses/xkbcommon/LICENSE",
                    "share/crt/dependencies/xkbcommon/recipe.json"):
                require(dist / relative)
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
        if manifest.get("built_from", {}).get("stage") == "03-gfx-simple":
            # The isolated transition is deliberately option-ON, unlike the
            # ordinary cumulative packaging target that shares this stage
            # label and remains default-OFF for Skia/FFmpeg.
            for name in ("freetype", "ffmpeg", "skia"):
                if name not in redistributed:
                    raise SystemExit(
                        f"isolated 04-gfx-media does not declare its {name} dependency")
            for relative in (
                    "include/freetype2",
                    "include/libavformat/avformat.h",
                    "include/libavcodec/avcodec.h",
                    "include/libswresample/swresample.h",
                    "include/libavutil/avutil.h",
                    "include/include/core/SkSurface.h",
                    "include/modules/skcms/skcms.h",
                    "include/modules/skcms/src/skcms_public.h",
                    "lib/libfreetype.a",
                    "lib/libavformat.a", "lib/libavcodec.a",
                    "lib/libswresample.a", "lib/libavutil.a",
                    "lib/libskia.a",
                    "include/crtgfx/skia.h"):
                require(dist / relative)
            require_any(dist / "lib", ("libcrtgfx_gpu.a",), "static crtgfx_gpu")
            require_any(dist / "lib", ("libcrtgfx_skia.a",), "static crtgfx_skia")
            require_any(dist / "lib", ("libcrtmedia.a",), "static crtmedia")
            require_any(dist / "lib", ("*crtgfx_gpu*dll*", "libcrtgfx_gpu.so*",
                                        "libcrtgfx_gpu.dylib"), "shared crtgfx_gpu")
            require_any(dist / "lib", ("*crtgfx_skia*dll*", "libcrtgfx_skia.so*",
                                        "libcrtgfx_skia.dylib"), "shared crtgfx_skia")
            require_any(dist / "lib", ("*crtmedia*dll*", "libcrtmedia.so*",
                                        "libcrtmedia.dylib"), "shared crtmedia")
    if args.stage >= "05-js":
        require(dist / "include" / "crtjs")
    print(f"CRT distribution verified: {dist}")


if __name__ == "__main__":
    main()
