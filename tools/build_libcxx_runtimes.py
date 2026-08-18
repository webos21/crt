#!/usr/bin/env python3
"""Configure and build Android libc++ sources through the CRT wrappers."""

import argparse
import os
import shutil
import subprocess
from pathlib import Path


def run(args, cwd=None, env=None):
    print("+", " ".join(str(arg) for arg in args), flush=True)
    subprocess.run(args, cwd=cwd, env=env, check=True)


def cmake_configure(cmake, source, build, install, common, extra, env):
    build.mkdir(parents=True, exist_ok=True)
    run([cmake, "-S", str(source), "-B", str(build), "-G", "Ninja"] + common + extra, env=env)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--build-root", required=True, type=Path)
    parser.add_argument("--install-prefix", required=True, type=Path)
    parser.add_argument("--sysroot", required=True, type=Path)
    parser.add_argument("--target-os", required=True, choices=["linux", "macos", "windows"])
    parser.add_argument("--phase", required=True, choices=["configure", "build", "install"])
    args = parser.parse_args()

    root = args.root.resolve()
    source_root = args.source_root.resolve()
    build_root = args.build_root.resolve()
    install_prefix = args.install_prefix.resolve()
    sysroot = args.sysroot.resolve()
    libcxx = source_root / "libcxx"
    libcxxabi = source_root / "libcxxabi"
    for source in (libcxx, libcxxabi):
        if not (source / "CMakeLists.txt").is_file():
            raise SystemExit(f"Android runtime source is missing: {source}; run crt-libcxx-fetch first")

    cmake = shutil.which("cmake") or "cmake"
    env = os.environ.copy()
    env["CRT_SYSROOT"] = str(sysroot)
    env["CRT_TARGET_OS"] = args.target_os
    env["CRT_CXX_ENABLE_EXCEPTIONS"] = "1"
    env["CRT_CXX_ENABLE_RTTI"] = "1"

    # The external projects are intentionally compiled through the CRT
    # wrappers. -U__APPLE__ prevents an accidental macOS SDK personality;
    # pthread is our Bionic-shaped public ABI on all three hosts.
    cxx_flags = "-U__APPLE__" if args.target_os == "macos" else ""
    common = [
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        f"-DCMAKE_C_COMPILER={root / 'tools' / 'crt-cc'}",
        f"-DCMAKE_CXX_COMPILER={root / 'tools' / 'crt-c++'}",
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
        f"-DCMAKE_INSTALL_PREFIX={install_prefix}",
        "-DCMAKE_BUILD_TYPE=Debug",
        f"-DCMAKE_CXX_FLAGS={cxx_flags}",
    ]
    libcxx_options = [
        "-DLIBCXX_ENABLE_SHARED=ON",
        "-DLIBCXX_ENABLE_STATIC=ON",
        "-DLIBCXX_INCLUDE_TESTS=OFF",
        "-DLIBCXX_INCLUDE_BENCHMARKS=OFF",
        "-DLIBCXX_INCLUDE_DOCS=OFF",
        "-DLIBCXX_CXX_ABI=none",
        "-DLIBCXX_ENABLE_EXCEPTIONS=ON",
        "-DLIBCXX_ENABLE_RTTI=ON",
        "-DLIBCXX_ENABLE_THREADS=ON",
        "-DLIBCXX_HAS_PTHREAD_API=ON",
        "-DLIBCXX_USE_COMPILER_RT=ON",
        # Android's build uses a newer dialect than this legacy standalone
        # CMake default. Keeping this cache value also avoids the removed C11
        # gets import while preserving the public CRT headers.
        "-DLIBCXX_STANDARD_VER=c++14",
    ]
    libcxxabi_options = [
        "-DLIBCXXABI_ENABLE_SHARED=ON",
        "-DLIBCXXABI_ENABLE_STATIC=ON",
        "-DLIBCXXABI_INCLUDE_TESTS=OFF",
        "-DLIBCXXABI_ENABLE_EXCEPTIONS=ON",
        "-DLIBCXXABI_ENABLE_THREADS=ON",
        "-DLIBCXXABI_HAS_PTHREAD_API=ON",
        "-DLIBCXXABI_USE_COMPILER_RT=ON",
        f"-DLLVM_EXTERNAL_LIBCXX_SOURCE_DIR={libcxx}",
    ]
    builds = (("libcxx", libcxx, libcxx_options), ("libcxxabi", libcxxabi, libcxxabi_options))

    if args.phase == "configure":
        for name, source, options in builds:
            cmake_configure(cmake, source, build_root / name, install_prefix, common, options, env)
        print("Android libunwind is source-only/Soong-driven in this import; its CRT CMake adapter is a separate follow-up.")
        return

    for name, source, options in builds:
        build = build_root / name
        if not (build / "build.ninja").is_file():
            cmake_configure(cmake, source, build, install_prefix, common, options, env)
        if args.phase == "build":
            run([cmake, "--build", str(build), "--target", "cxx" if name == "libcxx" else "cxxabi"], env=env)
            run([cmake, "--install", str(build)], env=env)
        else:
            run([cmake, "--install", str(build)], env=env)


if __name__ == "__main__":
    main()
