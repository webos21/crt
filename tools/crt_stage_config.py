#!/usr/bin/env python3
"""Bounded, offline preflight for an already configured in-tree stage.

This is not a second CMake configure or an SDK/CLI contract. Reports under the
build tree's stage-config/ are local diagnostics, not build/runtime acceptance.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parent.parent
STAGES = ("c", "libcxx", "gfx-simple", "gfx-media", "ui", "web")
FEATURES = {
    "libcxx": ("CRT_USE_IMPORTED_LIBCXX",),
    "gfx-media": ("CRTGFX_ENABLE_GPU_BACKEND", "CRTGFX_ENABLE_SKIA",
                  "CRTMEDIA_ENABLE_FFMPEG", "CRTMEDIA_ENABLE_CURL"),
    "ui": ("CRTUI_ENABLE_LVGL",),
}
TRUE = {"1", "ON", "YES", "TRUE", "Y"}


def canonical_arch(value):
    value = value.lower()
    return {"amd64": "x86_64", "x64": "x86_64", "arm64": "aarch64"}.get(value, value)


def read_cache(path):
    result = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith(("#", "//")) or "=" not in line:
            continue
        key, value = line.split("=", 1)
        if ":" in key:
            result[key.rsplit(":", 1)[0]] = value
    return result


def preflight(build_dir, stage, target_os, target_arch, root=ROOT, configured_tools=None):
    cache_file = build_dir / "CMakeCache.txt"
    cache = read_cache(cache_file)
    if not cache.get("CMAKE_HOME_DIRECTORY") or Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() != root.resolve():
        raise ValueError("build tree does not belong to this CRT source tree")
    if cache.get("CRT_TARGET_OS") != target_os:
        raise ValueError("target OS differs from CMakeCache.txt; reconfigure the tree")
    arch = canonical_arch(target_arch)
    if arch not in ("x86_64", "aarch64"):
        raise ValueError(f"unsupported or unresolved target architecture: {target_arch}")
    configured_arch = cache.get("CRT_TARGET_ARCH", "")
    if configured_arch != "host" and canonical_arch(configured_arch) != arch:
        raise ValueError("target architecture differs from CMakeCache.txt; reconfigure the tree")
    if not (build_dir / "CMakeFiles").is_dir():
        raise ValueError("missing CMakeFiles; run cmake --preset <preset> first")
    tools = {}
    for key in ("CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER", "CMAKE_MAKE_PROGRAM"):
        value = (configured_tools or {}).get(key, cache.get(key, ""))
        # Reapplying a preset can leave a bare `clang` in CMakeCache.txt even
        # though configure resolved it successfully. Accept PATH tools too,
        # without executing them or confusing a tool name with a shell command.
        resolved = str(Path(value).resolve()) if value and Path(value).is_file() else shutil.which(value)
        if not resolved:
            raise ValueError(f"configured tool is missing: {key}={value}; reconfigure the tree")
        tools[key] = resolved
    required = [flag for name in STAGES[:STAGES.index(stage) + 1] for flag in FEATURES.get(name, ())]
    missing = [flag for flag in required if cache.get(flag, "").upper() not in TRUE]
    if missing:
        flags = " ".join(f"-D{flag}=ON" for flag in missing)
        raise ValueError(f"complete {stage} stage needs {', '.join(missing)}; "
                         f"run cmake --preset <preset> -B \"{build_dir}\" {flags}. "
                         "Reduced-feature diagnostic build targets remain available.")
    # Target support only: the Web builder still validates its execution host,
    # external tools and cross-link inputs before doing any work.
    if stage == "web" and (target_os, arch) not in {
            ("linux", "x86_64"), ("linux", "aarch64"),
            ("macos", "aarch64"), ("windows", "x86_64")}:
        raise ValueError(f"06-web is unsupported for {target_os}/{arch}; lower-stage configs remain available")
    if stage in ("gfx-media", "ui", "web") and target_os == "linux":
        for key in ("CRTMEDIA_LINUX_VAAPI_LIB", "CRTMEDIA_LINUX_VAAPI_DRM_LIB"):
            if not cache.get(key) or not Path(cache[key]).is_file():
                raise ValueError(f"Linux media stage needs {key}: install libva-dev for the target and reconfigure")
    return {
        "stage": stage, "target": {"os": target_os, "arch": arch},
        "build_type": cache.get("CMAKE_BUILD_TYPE", ""), "generator": cache.get("CMAKE_GENERATOR", ""),
        "compilers": {"c": tools["CMAKE_C_COMPILER"], "cxx": tools["CMAKE_CXX_COMPILER"]},
        "make_program": tools["CMAKE_MAKE_PROGRAM"],
        "required_features": {flag: True for flag in required},
        "cache_sha256": hashlib.sha256(cache_file.read_bytes()).hexdigest(),
        "scope": "configuration-only; dependencies, execution host and runtime acceptance not checked",
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--stage", required=True, choices=STAGES)
    parser.add_argument("--target-os", required=True, choices=("linux", "macos", "windows"))
    parser.add_argument("--target-arch", required=True)
    parser.add_argument("--c-compiler")
    parser.add_argument("--cxx-compiler")
    parser.add_argument("--make-program")
    args = parser.parse_args(argv)
    build_dir = args.build_dir.resolve()
    output = build_dir / "stage-config" / f"{args.stage}.json"
    try:
        tools = {key: value for key, value in (
            ("CMAKE_C_COMPILER", args.c_compiler), ("CMAKE_CXX_COMPILER", args.cxx_compiler),
            ("CMAKE_MAKE_PROGRAM", args.make_program)) if value is not None}
        report = preflight(build_dir, args.stage, args.target_os, args.target_arch, configured_tools=tools)
    except (OSError, ValueError) as exc:
        # Do not leave an older successful report after a failed preflight.
        output.unlink(missing_ok=True)
        print(f"[stage-config] {args.stage}: {exc}", file=sys.stderr)
        return 1
    content = json.dumps(report, indent=2) + "\n"
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text(encoding="utf-8") != content:
        output.write_text(content, encoding="utf-8")
    print(f"[stage-config] {args.stage}: {args.target_os}/{report['target']['arch']} configuration options valid; "
          f"dependency build/run not checked ({output})", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
