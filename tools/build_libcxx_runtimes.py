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


def install_darwin_link_metadata(libcxx, install_prefix, target_os):
    if target_os != "macos":
        return
    source = libcxx / "lib" / "notweak.exp"
    destination = install_prefix / "lib" / "libc++.notweak.exp"
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--build-root", required=True, type=Path)
    parser.add_argument("--install-prefix", required=True, type=Path)
    parser.add_argument("--sysroot", required=True, type=Path)
    parser.add_argument("--target-os", required=True, choices=["linux", "macos", "windows"])
    parser.add_argument("--windows-sdk-libpath", type=Path)
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
    env["CRT_CXX_BUILDING_RUNTIME"] = "1"
    if args.windows_sdk_libpath:
        env["CRT_WINDOWS_SDK_LIBPATH"] = str(args.windows_sdk_libpath.resolve())

    # The external projects are intentionally compiled through the CRT
    # wrappers. -U__APPLE__ prevents an accidental macOS SDK personality;
    # pthread is our Bionic-shaped public ABI on all three hosts.
    # Android's current libc++abi has a startup guard for Clang's typed
    # new/delete optimization. Disable that optimization for this standalone
    # runtime build so static initialization cannot call the guarded operator
    # new before libc++ has initialized its dispatch state.
    cxx_flags = "-D__BIONIC__ -fno-typed-cxx-new-delete"
    if args.target_os == "macos":
        cxx_flags = f"-U__APPLE__ {cxx_flags}"
    c_compiler = root / "tools" / "crt-cc"
    cxx_compiler = root / "tools" / "crt-c++"
    compiler_arg_options = []
    if args.target_os == "windows":
        # A native Windows process cannot execute the wrappers' shebangs.
        # Use the CRT mksh PE executable as CMake's compiler launcher and pass
        # the real wrapper as argv[1], matching the porting-test toolchain.
        mksh = sysroot / "system" / "bin" / "mksh.exe"
        if not mksh.is_file():
            raise SystemExit(f"CRT mksh is missing from the sysroot: {mksh}")
        c_compiler = mksh
        cxx_compiler = mksh
        compiler_arg_options = [
            f"-DCMAKE_C_COMPILER_ARG1={root / 'tools' / 'crt-cc'}",
            f"-DCMAKE_CXX_COMPILER_ARG1={root / 'tools' / 'crt-c++'}",
        ]

    common = [
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        f"-DCMAKE_C_COMPILER={c_compiler}",
        f"-DCMAKE_CXX_COMPILER={cxx_compiler}",
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
        f"-DCMAKE_INSTALL_PREFIX={install_prefix}",
        "-DCMAKE_BUILD_TYPE=Debug",
        f"-DCMAKE_CXX_FLAGS={cxx_flags}",
    ] + compiler_arg_options
    libcxx_options = [
        "-DLIBCXX_ENABLE_SHARED=ON",
        "-DLIBCXX_ENABLE_STATIC=ON",
        "-DLIBCXX_INCLUDE_TESTS=OFF",
        "-DLIBCXX_INCLUDE_BENCHMARKS=OFF",
        "-DLIBCXX_INCLUDE_DOCS=OFF",
        "-DLIBCXX_CXX_ABI=libcxxabi",
        f"-DLIBCXX_CXX_ABI_INCLUDE_PATHS={libcxxabi / 'include'}",
        f"-DLIBCXX_CXX_ABI_LIBRARY_PATH={install_prefix / 'lib'}",
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
    # Modern Bionic provides the historical librt surface from libc. The
    # standalone libc++ probe is also compiled as a static library and can
    # report a false positive without performing a link, so pin this off on
    # every host instead of leaking a host librt dependency into Linux.
    libcxx_options.append("-DLIBCXX_HAS_RT_LIB=OFF")
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
    # libc++'s shared image contains references to the Itanium ABI runtime.
    # Build and install libc++abi first so both libc++ link shapes resolve
    # against the CRT-built ABI library rather than the host C++ runtime.
    builds = (("libcxxabi", libcxxabi, libcxxabi_options), ("libcxx", libcxx, libcxx_options))

    if args.phase == "configure":
        for name, source, options in builds:
            cmake_configure(cmake, source, build_root / name, install_prefix, common, options, env)
        return

    for name, source, options in builds:
        build = build_root / name
        if not (build / "build.ninja").is_file():
            cmake_configure(cmake, source, build, install_prefix, common, options, env)
        if args.phase == "build":
            targets = (
                ["cxx", "cxx_filesystem", "cxx_experimental", "cxx-generated-config"]
                if name == "libcxx"
                else ["cxxabi"]
            )
            run([cmake, "--build", str(build), "--target"] + targets, env=env)
            run([cmake, "--install", str(build)], env=env)
            if name == "libcxx":
                install_darwin_link_metadata(libcxx, install_prefix, args.target_os)
        else:
            run([cmake, "--install", str(build)], env=env)
            if name == "libcxx":
                install_darwin_link_metadata(libcxx, install_prefix, args.target_os)


if __name__ == "__main__":
    main()
