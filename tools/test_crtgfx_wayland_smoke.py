#!/usr/bin/env python3
"""Build and run libcrtgfx/tests/wayland_client_smoke.c, self-sufficient
regardless of the calling build directory's own current state.

Mirrors tools/test_crtgfx_skia_smoke.py's own role for crtgfx-skia-smoke:
a target a user can build directly (`cmake --build <dir> --target
crtgfx-wayland-smoke`) without first hand-running the multi-step chain
(sysroot/rootfs, port-build-expat, port-build-libffi, crtgfx-wayland-
configure, crtgfx-wayland-build) that would otherwise be required. Unlike
that script, there is no CRTGFX_ENABLE_*-style outer-project flag to flip
here at all -- wayland-client is plain C with no equivalent to Skia's own
CRT_USE_IMPORTED_LIBCXX requirement -- so this drives a *simpler* two-
phase flow: configure once with default flags, build the one CRT
prerequisite (rootfs on Windows, sysroot elsewhere -- matching
tools/crt-port-build.py's own --skip-sysroot-build/--use-crt-shell
target-selection logic exactly, since crtgfx-wayland-build's own
port-build-expat/port-build-libffi dependencies shell out to that same
script), then build crtgfx-wayland-build (whose own CMake DEPENDS chain
handles fetching Wayland and building expat/libffi first), then compile
and run the standalone smoke test directly via crt-cc against the
freshly installed libwayland-client. No pkg-config: tools/build_wayland.py
no longer runs Meson at all (2026-08-24 rewrite, see that file's own
top-of-file docstring), so there is no generated wayland-client.pc to
query -- the static archive and its one real dependency (libffi) are
linked by explicit, full path instead, matching the same static-link
discipline porting/recipes/*.json's own roundtrip-static tests already
use.

Like crtgfx-skia-smoke, this drives a *separate, dedicated* nested build
directory (never the calling directory's own -- its own cached flags are
left completely untouched), kept and reused incrementally across
invocations.
"""

import argparse
import os
import subprocess
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

    configure_command = [
        "cmake", "-G", "Ninja",
        "-S", str(root),
        "-B", str(build_dir),
        "-DCMAKE_BUILD_TYPE=Debug",
        f"-DCMAKE_C_COMPILER={args.cmake_c_compiler}",
        f"-DCMAKE_CXX_COMPILER={args.cmake_cxx_compiler}",
        f"-DCRT_TARGET_OS={args.target_os}",
        f"-DCRT_TARGET_ARCH={args.target_arch}",
    ]
    if args.target_os == "windows":
        # Matches CMakePresets.json's own windows-host-ninja-debug entry,
        # the same as tools/test_crtgfx_skia_smoke.py's own base_configure_
        # command.
        configure_command += [
            "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
            "-DCMAKE_MSVC_RUNTIME_LIBRARY=",
            "-DCMAKE_C_STANDARD_LIBRARIES=",
            "-DCMAKE_CXX_STANDARD_LIBRARIES=",
        ]
    run(configure_command)

    # The one CRT prerequisite crt-cc itself needs (a populated CRT_SYSROOT)
    # -- matches tools/crt-port-build.py's own main() target selection
    # exactly (`target = "rootfs" if args.use_crt_shell else "sysroot"`),
    # since crtgfx-wayland-build's own port-build-expat/port-build-libffi
    # DEPENDS shell out to that identical script with --use-crt-shell on
    # Windows. Unlike tools/test_crtgfx_skia_smoke.py, there is no
    # CRT_USE_IMPORTED_LIBCXX reconfigure dance here at all: Wayland is
    # plain C (project('wayland', 'c', ...) in its own meson.build), so
    # nothing in this whole chain ever needs the imported libc++ sysroot.
    run(["cmake", "--build", str(build_dir), "--target", "rootfs" if args.target_os == "windows" else "sysroot"])

    # This one target's own CMake DEPENDS graph (libcrtgfx/CMakeLists.txt)
    # handles everything else: port-build-expat, port-build-libffi,
    # crtgfx-wayland-configure (fetch + meson setup), then the real Meson
    # build + install.
    run(["cmake", "--build", str(build_dir), "--target", "crtgfx-wayland-build"])

    install_prefix = build_dir / "external" / "wayland" / "install"
    include_dir = install_prefix / "include"
    lib_dir = install_prefix / "lib"
    if not (include_dir / "wayland-client.h").is_file():
        raise SystemExit(f"expected header missing after crtgfx-wayland-build: {include_dir / 'wayland-client.h'}")

    suffix = ".exe" if args.target_os == "windows" else ""
    test_root = build_dir / "wayland-client-smoke"
    test_root.mkdir(parents=True, exist_ok=True)
    binary = test_root / f"wayland_client_smoke{suffix}"
    source = root / "libcrtgfx" / "tests" / "wayland_client_smoke.c"

    cc_suffix = ".cmd" if args.target_os == "windows" else ""
    crt_cc = root / "tools" / f"crt-cc{cc_suffix}"

    env = os.environ.copy()
    env["CRT_SYSROOT"] = str(build_dir / "sysroot")
    env["CRT_TARGET_OS"] = args.target_os
    if args.target_arch != "host":
        env["CRT_TARGET_ARCH"] = args.target_arch
    if args.target_os == "windows":
        mksh = build_dir / "rootfs" / "system" / "bin" / "mksh.exe"
        if not mksh.is_file():
            raise SystemExit(f"CRT mksh is missing from the rootfs: {mksh}")
        env["CRT_MKSH_EXE"] = str(mksh)
        env["CRT_ROOTFS"] = str(build_dir / "rootfs")

    # Full, explicit archive paths -- libwayland-client.a itself already
    # embeds wayland-util.c/connection.c/wayland-os.c's own object code
    # (tools/build_wayland.py compiles and archives all four together, see
    # that file's own Phase 3), so the only *separate* archive this link
    # still needs is libffi.a (connection.c's own closure-based argument
    # marshaling) -- matching porting/recipes/*.json's own roundtrip-static
    # test convention (a literal @PORT_PREFIX@/lib/lib*.a path, never a
    # bare -l flag) for the same reason: no pkg-config/.pc file exists to
    # resolve this from, and a bare -lwayland-client/-lffi search would be
    # fragile without one.
    port_prefix_lib = build_dir / "port-tests" / "install" / "lib"
    static_archive = lib_dir / "libwayland-client.a"
    libffi_archive = port_prefix_lib / "libffi.a"
    for required in (static_archive, libffi_archive):
        if not required.is_file():
            raise SystemExit(f"expected static library missing: {required}")

    compile_command = [
        str(crt_cc), f"-I{include_dir}", str(source),
        str(static_archive), str(libffi_archive),
        "-o", str(binary),
    ]

    run(compile_command, env=env)
    run([str(binary)], cwd=str(root), env=env)


if __name__ == "__main__":
    main()
