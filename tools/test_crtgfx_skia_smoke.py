#!/usr/bin/env python3
"""Build and run crtgfx_skia_raster_smoke, regardless of the calling build
directory's own current CRTGFX_ENABLE_SKIA/CRT_USE_IMPORTED_LIBCXX cache
values.

Mirrors tools/test_libcxx_runtime.py's own role for crt-libcxx-smoke: a
target a user can build directly (`cmake --build <dir> --target
crtgfx-skia-smoke`) without first hand-running the multi-step reconfigure
dance libcrtgfx/CMakeLists.txt's own CRTGFX_ENABLE_SKIA guard otherwise
requires (crt-libcxx-sysroot, then crtgfx-skia-build, then a reconfigure
with CRTGFX_ENABLE_SKIA=ON, only *then* can crtgfx_skia_raster_smoke even
be built) -- see that file's own FATAL_ERROR guard comments for why each
step has to happen in that exact order.

Unlike test_libcxx_runtime.py (which compiles its own smoke source
directly through tools/crt-c++, entirely independent of the outer CMake
project), crtgfx_skia_raster_smoke's own real build recipe -- Skia's own
include path, SK_BUILD_FOR_UNIX, the imported-libc++ swap, and (Linux)
the --start-group/-fuse-ld=lld fix, (Windows) uuid.lib/--allow-multiple-
definition -- already lives correctly in libcrtgfx/CMakeLists.txt, tested
and proven working there. Reimplementing that recipe a second time, by
hand, in this script would risk drifting out of sync with it. Instead,
this script drives a *separate, dedicated* nested CMake build directory
(never the calling directory's own -- its own cached flags are left
completely untouched) that reuses that exact, real CMakeLists.txt target.
The first invocation is genuinely slow (a full libcxx + Skia fetch/build,
matching crt-libcxx-sysroot/crtgfx-skia-build's own real cost elsewhere in
this project); the dedicated directory is kept and reused incrementally
on every later invocation, the same as any other CRT build directory.
"""

import argparse
import subprocess
import sys
from pathlib import Path


def run(command, **kwargs):
    print("+", " ".join(str(c) for c in command), flush=True)
    subprocess.run(command, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path, help="CRT source root")
    parser.add_argument("--build-dir", required=True, type=Path,
                         help="Dedicated build directory for this smoke test (kept, reused across invocations -- never the calling build directory)")
    parser.add_argument("--cmake-c-compiler", required=True)
    parser.add_argument("--cmake-cxx-compiler", required=True)
    parser.add_argument("--target-os", required=True, choices=["linux", "macos", "windows"])
    parser.add_argument("--target-arch", default="host")
    args = parser.parse_args()

    root = args.root.resolve()
    build_dir = args.build_dir.resolve()
    build_dir.mkdir(parents=True, exist_ok=True)

    # Phase 1: configure with CRT_USE_IMPORTED_LIBCXX=ON but CRTGFX_ENABLE_
    # SKIA still off -- crt-libcxx-sysroot and crtgfx-skia-build both need a
    # clean configure to exist as real targets, and crtgfx-skia-build's own
    # GN build needs the imported libc++ already staged, but CRTGFX_ENABLE_
    # SKIA=ON's own FATAL_ERROR guard (libcrtgfx/CMakeLists.txt) requires
    # libskia.a to already exist on disk -- turning it on this early would
    # just fail configure outright on a from-scratch build directory.
    base_configure_command = [
        "cmake", "-G", "Ninja",
        "-S", str(root),
        "-B", str(build_dir),
        "-DCMAKE_BUILD_TYPE=Debug",
        f"-DCMAKE_C_COMPILER={args.cmake_c_compiler}",
        f"-DCMAKE_CXX_COMPILER={args.cmake_cxx_compiler}",
        f"-DCRT_TARGET_OS={args.target_os}",
        f"-DCRT_TARGET_ARCH={args.target_arch}",
        # Explicit OFF, not just omitted: this directory is deliberately
        # kept and reused across invocations (see this script's own
        # docstring), and a CMake cache variable simply omitted from a
        # reconfigure command keeps whatever value it was last set to --
        # it is not reset to option()'s own declared default. Confirmed
        # for real (2026-08-23): an earlier version of this script left
        # this omitted, and a *second* run against a directory a first,
        # since-fixed bug had already configured with the flag ON reused
        # that stale cached value here, recreating the exact rootfs/
        # crt-libcxx-sysroot cycle this phase exists to avoid.
        "-DCRT_USE_IMPORTED_LIBCXX=OFF",
        # Same exact reasoning, applied to the *other* flag this script's
        # own Phase 2 (below) turns on -- found missing for real
        # (2026-08-25, Windows): a build directory that had already
        # completed a full previous run (Phase 2 left CRTGFX_ENABLE_SKIA=ON
        # cached) hit libcrtgfx/CMakeLists.txt's own new "CRTGFX_ENABLE_
        # SKIA=ON requires CRT_USE_IMPORTED_LIBCXX=ON on Windows" guard
        # right here in Phase 0, because this phase forces
        # CRT_USE_IMPORTED_LIBCXX=OFF while the stale CRTGFX_ENABLE_SKIA=ON
        # from the *previous* invocation was left untouched -- the same
        # class of stale-cache bug the comment above already learned to
        # avoid for CRT_USE_IMPORTED_LIBCXX, just not yet applied here too.
        "-DCRTGFX_ENABLE_SKIA=OFF",
    ]
    if args.target_os == "windows":
        # Matches CMakePresets.json's own windows-host-ninja-debug entry.
        base_configure_command += [
            "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
            "-DCMAKE_MSVC_RUNTIME_LIBRARY=",
            "-DCMAKE_C_STANDARD_LIBRARIES=",
            "-DCMAKE_CXX_STANDARD_LIBRARIES=",
        ]

    # Phase 0: configure with CRT_USE_IMPORTED_LIBCXX still off (the plain
    # default) and build `rootfs`. This project's own CMakeLists.txt
    # deliberately makes rootfs's own dependency on crt-libcxx-sysroot
    # (needed once CRT_USE_IMPORTED_LIBCXX=ON, for its own bundled libc++
    # runtime DLLs) conditional -- it only exists *while that flag is ON*
    # -- specifically to avoid a real cycle in the other, bootstrap
    # direction: crt-libcxx-configure's own recipe.json-driven build
    # (tools/crt-libcxx-build.py) shells out to tools/crt-cc/tools/crt-c++
    # through the rootfs's own mksh.exe on Windows (native Windows cannot
    # run a shebang script directly, and the bare sysroot's own mksh.exe
    # copy has no toybox alongside it, so neither PATH-resolved "printf"
    # nor "uname -m" work from there -- see tools/test_libcxx_runtime.py's
    # own matching comment), so rootfs has to exist *first*, at least
    # once, before crt-libcxx-sysroot can build at all. This project's own
    # regular preset directories never hit this ordering problem in
    # practice only because `rootfs` is already part of their own default
    # `ALL` target and normally gets built at least once with the flag
    # still off, long before anyone reaches for crt-libcxx-sysroot
    # directly -- a dedicated, from-scratch directory like this one has no
    # such implicit head start. Confirmed for real (2026-08-23): jumping
    # straight to CRT_USE_IMPORTED_LIBCXX=ON here (skipping this phase)
    # produces exactly the cycle CMakeLists.txt's own comment warns about
    # -- "CRT mksh is missing from the rootfs ... (build the rootfs target
    # first)" surfaces from *inside* crt-libcxx-configure itself, because
    # rootfs's own dependency direction had already flipped onto crt-
    # libcxx-sysroot by the time anything tried to build it.
    run(base_configure_command)
    if args.target_os == "windows":
        run(["cmake", "--build", str(build_dir), "--target", "rootfs"])

    # Phase 1: reconfigure with CRT_USE_IMPORTED_LIBCXX=ON (rootfs, already
    # built above, is simply left as-is/up to date by this) and build the
    # imported libc++ sysroot, then Skia's own CPU-raster archive (which
    # itself needs that same imported libc++ already staged).
    run(base_configure_command + ["-DCRT_USE_IMPORTED_LIBCXX=ON"])
    run(["cmake", "--build", str(build_dir), "--target", "crt-libcxx-sysroot"])
    run(["cmake", "--build", str(build_dir), "--target", "crtgfx-skia-build"])

    # Phase 2: now that libskia.a and the imported libc++ sysroot both
    # exist, CRTGFX_ENABLE_SKIA=ON's own guard is satisfied -- reconfigure
    # in place (same build directory) to pick it up, then build and run
    # the real smoke executable.
    run(["cmake", "-S", str(root), "-B", str(build_dir), "-DCRTGFX_ENABLE_SKIA=ON"])
    run(["cmake", "--build", str(build_dir), "--target", "crtgfx_skia_raster_smoke"])

    suffix = ".exe" if args.target_os == "windows" else ""
    binary = build_dir / "libcrtgfx" / f"crtgfx_skia_raster_smoke{suffix}"
    if not binary.is_file():
        raise SystemExit(f"expected smoke binary missing: {binary}")
    # Matches crtgfx_skia_raster_smoke_runs' own ctest WORKING_DIRECTORY
    # (libcrtgfx/CMakeLists.txt) for consistency.
    run([str(binary)], cwd=str(root))


if __name__ == "__main__":
    main()
