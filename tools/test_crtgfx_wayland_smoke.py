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
freshly installed libwayland-client.

Like crtgfx-skia-smoke, this drives a *separate, dedicated* nested build
directory (never the calling directory's own -- its own cached flags are
left completely untouched), kept and reused incrementally across
invocations.
"""

import argparse
import os
import shutil
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

    # pkg-config, not a hand-picked archive/-l flag: libwayland-client.a
    # does not embed libffi's own object code (Meson's pkgconfig module
    # only bundles targets pulled in via link_with -- wayland-util/
    # wayland-private, both internal static_library()s -- directly into
    # wayland-client's own .a/.so; libffi/threads, found via
    # dependency('libffi')/dependency('threads'), are real *external*
    # deps recorded in the installed wayland-client.pc's own Libs.private/
    # Requires.private instead). `pkg-config --static` is what correctly
    # expands those, exactly the way any real external Meson/CMake/
    # autotools consumer of an installed pkg-config-described library
    # would resolve them -- hand-guessing the archive list here would
    # silently drift the moment upstream's own dependency set changes.
    # PKG_CONFIG_PATH covers both this build's own install (wayland-
    # client.pc itself) and the shared port_prefix (libffi.pc, resolved
    # transitively through wayland-client.pc's own Requires.private).
    pkg_config = shutil.which("pkg-config") or shutil.which("pkgconf")
    if not pkg_config:
        raise SystemExit("test_crtgfx_wayland_smoke.py: pkg-config/pkgconf was not found on PATH.")
    pkgconfig_env = env.copy()
    port_prefix_pkgconfig = build_dir / "port-tests" / "install" / "lib" / "pkgconfig"
    pkgconfig_env["PKG_CONFIG_PATH"] = os.pathsep.join(
        str(p) for p in (lib_dir / "pkgconfig", port_prefix_pkgconfig)
    )
    pkgconfig_env.pop("PKG_CONFIG_LIBDIR", None)
    # --static forces Libs.private/Requires.private into the output (a
    # plain `pkg-config --libs` only emits the public Libs: line, which
    # for a Meson-generated .pc omits private/internal deps by design --
    # correct for a *shared*-library consumer, which resolves those at
    # dlopen/runtime-link time instead, but this smoke test links the
    # static archive, so every dependency has to be explicit up front).
    cflags = subprocess.check_output(
        [pkg_config, "--cflags", "wayland-client"], env=pkgconfig_env, text=True
    ).split()
    libs_static = subprocess.check_output(
        [pkg_config, "--libs", "--static", "wayland-client"], env=pkgconfig_env, text=True
    ).split()

    compile_command = [str(crt_cc), f"-I{include_dir}"] + cflags + [str(source)] + libs_static + ["-o", str(binary)]

    # A dynamic loader search path so the freshly-built, freshly-linked
    # libwayland-client.so (installed under a non-standard, project-owned
    # prefix -- never the host system's own library search path) would be
    # found at run time -- kept even though this smoke test links the
    # static archive above, since a future variant of this script (or a
    # copy-pasted invocation) may reasonably want the shared build
    # instead, matching tools/crt-port-build.py's own port_test_env()
    # helper for the identical reason.
    if args.target_os == "linux":
        env["LD_LIBRARY_PATH"] = f"{lib_dir}{os.pathsep}{env.get('LD_LIBRARY_PATH', '')}"
    elif args.target_os == "macos":
        env["DYLD_LIBRARY_PATH"] = f"{lib_dir}{os.pathsep}{env.get('DYLD_LIBRARY_PATH', '')}"
    elif args.target_os == "windows":
        env["PATH"] = f"{lib_dir}{os.pathsep}{env.get('PATH', '')}"

    run(compile_command, env=env)
    run([str(binary)], cwd=str(root), env=env)


if __name__ == "__main__":
    main()
