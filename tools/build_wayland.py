#!/usr/bin/env python3
"""Build Wayland core (wayland-scanner + wayland-client/-server/-cursor/-egl)
with the CRT wrapper toolchain, via Meson/Ninja.

Mirrors tools/build_skia.py's own role -- an external, non-CMake build
driven by a project-owned Python script, whose CC/CXX/AR wiring points at
tools/crt-cc et al so the produced binaries genuinely link against this
project's own libc/libm/libdl, not a host toolchain's -- but for a
different real build system. Meson generates a "native file" ([binaries]
c = ..., pkg-config = ...) rather than GN's own --args file. This project's
build is always host==target (Meson's own "native", non-cross case: the
machine that runs `meson setup`/`ninja` is the same OS/arch the produced
binaries target, exactly like every CMake preset in this project), so no
[host_machine]/[binaries] cross-file section or exe_wrapper is ever needed
-- wayland-scanner, itself built by this exact script, is a real, natively
executable binary on the same host, and Meson runs it directly mid-build
(to generate wayland-client-protocol.c/.h) the same way it would any other
native build tool.

Unlike Skia's GN build (C++, needs this project's own imported libc++),
Wayland is plain C (`project('wayland', 'c', ...)` in its own meson.build)
-- there is no CRT_CXX_STANDARD_INCLUDE_FLAGS/imported-libc++ requirement
here at all.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def run(args, cwd=None, env=None):
    print("+", " ".join(str(arg) for arg in args), flush=True)
    subprocess.run(args, cwd=cwd, env=env, check=True)


def windows_short_path(path):
    """Convert an absolute Windows path to its 8.3 short-name form.

    Exactly matches tools/build_skia.py's own windows_short_path() (same
    GetShortPathNameW win32 call, same reason: mksh's own exec()/command
    lookup cannot run a program whose path contains a space) -- duplicated
    here rather than imported, matching that file's own stated convention
    that none of this project's tools/*.py scripts share a common helper
    module today."""
    if os.name != "nt":
        return str(path)
    import ctypes

    value = str(path)
    get_short_path_name_w = ctypes.windll.kernel32.GetShortPathNameW
    needed = get_short_path_name_w(value, None, 0)
    if needed == 0:
        return value
    buffer = ctypes.create_unicode_buffer(needed)
    if get_short_path_name_w(value, buffer, needed) == 0:
        return value
    return buffer.value


def normalize_target_arch(target_arch_arg):
    """Exactly matches tools/build_skia.py's own normalize_target_arch():
    resolves an explicit --target-arch (which libcrtgfx/CMakeLists.txt
    passes straight from a CMAKE_SYSTEM_PROCESSOR-shaped CMake variable,
    e.g. Windows' own "AMD64"/"ARM64" spelling) to the GNU-triple spelling
    ("x86_64"/"aarch64") tools/crt-cc's own --target=${arch}-w64-mingw32
    construction requires. "host" passes through unchanged: the sentinel
    for "no explicit arch, let CRT_TARGET_ARCH stay unset"."""
    if target_arch_arg == "host":
        return target_arch_arg
    arch = target_arch_arg.lower()
    if arch in ("aarch64", "arm64"):
        return "aarch64"
    if arch in ("x86_64", "amd64", "x64"):
        return "x86_64"
    raise SystemExit(
        f"build_wayland.py: unrecognized --target-arch {target_arch_arg!r} "
        "(expected aarch64/arm64 or x86_64/amd64/x64, or \"host\")"
    )


def meson_ini_string(value):
    escaped = str(value).replace("\\", "\\\\").replace("'", "\\'")
    return f"'{escaped}'"


def meson_ini_array(values):
    return "[" + ", ".join(meson_ini_string(value) for value in values) + "]"


def write_native_file(path, binaries, properties):
    """Meson "native file" (--native-file): tells `meson setup` which real
    binaries to use for this native (non-cross) build, overriding its own
    default PATH-search-based auto-detection. [binaries] values are always
    written as a one-element array (`c = ['/abs/path']`), matching Meson's
    own documented convention for a binary entry that might carry fixed
    leading arguments (e.g. `['ccache', 'gcc']») -- a bare string also
    works for the plain single-executable case this file always uses, but
    the array form is what Meson's own docs and every real-world native/
    cross file example use, so it is used here for the same reason
    tools/build_skia.py's own default_gn_args() writes real GN list/string
    literals rather than relying on looser forms GN might also tolerate."""
    lines = ["[binaries]"]
    for key, value in binaries.items():
        lines.append(f"{key} = {meson_ini_array([value])}")
    if properties:
        lines.append("")
        lines.append("[properties]")
        for key, value in properties.items():
            lines.append(f"{key} = {meson_ini_string(value)}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def default_meson_options():
    """-D option values for `meson setup`, matching this recipe's own
    documented core-only scope (libcrtgfx/third_party/wayland/recipe.json's
    own notes): scanner+libraries on (wayland-scanner, wayland-client/
    -server/-cursor/-egl -- egl here is Wayland's own thin, dependency-free
    frontend shim, not real EGL headers, see that meson.build's own
    contents), tests/documentation/dtd_validation off (no test-runner
    dependencies, no Doxygen/xmlto/xsltproc, no libxml2 -- none of which
    this project has ported and none of which this build actually needs:
    dtd_validation only validates the protocol DTD as a build-time nicety,
    it does not affect what gets built)."""
    return {
        "scanner": "true",
        "libraries": "true",
        "tests": "false",
        "documentation": "false",
        "dtd_validation": "false",
        # both, not Meson's own default (shared): crtgfx-wayland-smoke
        # links a plain static crt-cc executable against libwayland-client
        # directly (see libcrtgfx/tests/wayland_client_smoke.c), and a
        # future consumer of this build may reasonably want either shape
        # -- building both up front avoids a second, separate reconfigure
        # later just to add the one this recipe didn't happen to pick.
        "default_library": "both",
    }


def install_artifacts_note(install_prefix):
    print(f"Wayland installed into {install_prefix}")


def main():
    parser = argparse.ArgumentParser(description="Build Wayland core with the CRT wrapper toolchain.")
    parser.add_argument("--root", required=True, help="CRT repository root")
    parser.add_argument("--source", required=True, help="Wayland source checkout")
    parser.add_argument("--build-dir", required=True, help="Wayland Meson output directory")
    parser.add_argument("--install-prefix", required=True, help="Wayland install prefix used by libcrtgfx")
    parser.add_argument("--sysroot", required=True, help="CRT sysroot")
    parser.add_argument("--rootfs", default="", help="CRT rootfs (Windows only, for mksh.exe)")
    parser.add_argument("--port-prefix", required=True,
                         help="shared porting/recipes install prefix (out/<preset>/port-tests/install) "
                              "holding expat.pc/libffi.pc under lib/pkgconfig")
    parser.add_argument("--target-os", required=True, choices=["linux", "macos", "windows"])
    parser.add_argument("--target-arch", default="host")
    parser.add_argument("--meson", default="", help="path to meson; defaults to PATH")
    parser.add_argument("--ninja", default="", help="path to ninja; defaults to PATH")
    parser.add_argument("--pkg-config", default="", help="path to pkg-config/pkgconf; defaults to PATH")
    parser.add_argument("--meson-arg", action="append", default=[], help="extra raw `meson setup` -D... arg")
    parser.add_argument("--configure-only", action="store_true")
    args = parser.parse_args()
    args.target_arch = normalize_target_arch(args.target_arch)

    root = Path(args.root).resolve()
    source = Path(args.source).resolve()
    build_dir = Path(args.build_dir).resolve()
    install_prefix = Path(args.install_prefix).resolve()
    sysroot = Path(args.sysroot).resolve()
    port_prefix = Path(args.port_prefix).resolve()

    meson = args.meson or shutil.which("meson")
    if not meson:
        raise SystemExit(
            "build_wayland.py: meson was not found on PATH. Install it "
            "(e.g. `sudo apt-get install meson` on Debian/Ubuntu) and retry -- "
            "this project's own sandboxed tooling cannot install host packages "
            "for you."
        )
    ninja = args.ninja or shutil.which("ninja")
    if not ninja:
        raise SystemExit("build_wayland.py: ninja was not found on PATH.")
    pkg_config = args.pkg_config or shutil.which("pkg-config") or shutil.which("pkgconf")
    if not pkg_config:
        raise SystemExit(
            "build_wayland.py: pkg-config/pkgconf was not found on PATH. Install "
            "it (e.g. `sudo apt-get install pkg-config` on Debian/Ubuntu) and "
            "retry -- Meson needs a real pkg-config binary to resolve "
            "dependency('expat')/dependency('libffi') against --port-prefix."
        )

    # Absolute paths resolved via *this* process's own, unrestricted PATH,
    # before any Windows-specific env rewriting below -- matches
    # tools/build_skia.py's own CRT_HOST_CC/CRT_HOST_AR resolution ordering
    # exactly, for the identical reason (a later, narrowed PATH would no
    # longer find these).
    cmd_suffix = ".cmd" if args.target_os == "windows" else ""
    crt_cc = root / "tools" / f"crt-cc{cmd_suffix}"
    ar = shutil.which("llvm-ar") or shutil.which("ar") or "ar"

    env = os.environ.copy()
    env["CRT_SYSROOT"] = str(sysroot)
    env["CRT_TARGET_OS"] = args.target_os
    if args.target_arch != "host":
        env["CRT_TARGET_ARCH"] = args.target_arch

    if args.target_os == "windows":
        # crt-cc.cmd (a plain, directly-executable native-Windows launcher
        # for tools/crt-cc, a #!/bin/sh script CreateProcess cannot run
        # directly) needs CRT_MKSH_EXE set to the *rootfs* copy of
        # mksh.exe -- exactly matching tools/build_skia.py's own handling,
        # see that file's own comment for why the rootfs copy specifically
        # (toybox applet aliases live there, not in the bare sysroot).
        if not args.rootfs:
            raise SystemExit("--rootfs is required on Windows (its mksh.exe launches crt-cc.cmd)")
        mksh = Path(args.rootfs).resolve() / "system" / "bin" / "mksh.exe"
        if not mksh.is_file():
            raise SystemExit(f"CRT mksh is missing from the rootfs: {mksh} (build the \"rootfs\" target first)")
        env["CRT_MKSH_EXE"] = str(mksh)
        # CRT_ROOTFS pre-seeded ahead of time for the identical reason
        # tools/build_skia.py's own main() already documents in detail:
        # libc/src/env.c's __crt_rootfs_bootstrap() auto-detects it from
        # argv[0] and unconditionally chdir("/")s whenever it is *not*
        # already set, which would otherwise silently break every relative
        # source-file path Ninja's own generated build.ninja passes to
        # crt-cc.cmd (Ninja invokes build commands with cwd already set to
        # the build directory, matching `ninja -C <build-dir>` below).
        env["CRT_ROOTFS"] = str(Path(args.rootfs).resolve())
        # Unlike tools/build_skia.py's own GN-driven build, this script
        # does NOT need to narrow PATH to a POSIX-only rootfs-relative form
        # here: confirmed for real by reading tools/crt-cc's own script
        # body end to end -- once CRT_HOST_CC/CRT_TARGET_OS/CRT_TARGET_ARCH
        # are all set explicitly (as they are here), it performs zero
        # bare-name PATH lookups of its own (the one conditional PATH-
        # dependent path, a bare `uname` call, only runs when
        # CRT_TARGET_OS/CRT_TARGET_ARCH are left unset). Leaving PATH at
        # its real, normal Windows value throughout means `meson`/`ninja`/
        # `pkg-config` -- all genuine host tools, unlike crt-cc's own
        # target compiler -- keep resolving normally too, without needing
        # GN/build_skia.py's own separate is_clang.py/"python3"-token
        # patching dance (which existed specifically to route around a
        # PATH-format conflict this script's simpler compiler-invocation
        # shape never creates in the first place).
        host_cc = shutil.which("clang")
        if host_cc:
            env["CRT_HOST_CC"] = windows_short_path(host_cc).replace("\\", "/")
        host_ar = shutil.which("llvm-ar")
        if host_ar:
            ar = host_ar

    native_binaries = {
        "c": str(crt_cc),
        "ar": ar,
        "pkg-config": pkg_config,
    }
    native_file = build_dir / "crt-native.ini"
    write_native_file(native_file, native_binaries, properties={})

    pkgconfig_libdir = str(port_prefix / "lib" / "pkgconfig")
    env["PKG_CONFIG_PATH"] = pkgconfig_libdir
    env["PKG_CONFIG_LIBDIR"] = pkgconfig_libdir

    meson_options = default_meson_options()
    setup_cmd = [
        meson, "setup",
        "--native-file", str(native_file),
        "--prefix", str(install_prefix),
    ]
    for key, value in meson_options.items():
        setup_cmd += [f"-D{key}={value}"]
    setup_cmd += args.meson_arg
    setup_cmd += [str(build_dir), str(source)]

    if (build_dir / "build.ninja").exists():
        # Meson refuses to `setup` into an already-configured build
        # directory a second time without --reconfigure -- reconfigure
        # in place instead of wiping the directory, matching this
        # project's own established "kept and reused incrementally across
        # invocations" pattern for other shadow/dedicated build directories
        # (see tools/test_crtgfx_skia_smoke.py's own docstring).
        setup_cmd.insert(2, "--reconfigure")
    run(setup_cmd, cwd=root, env=env)
    if args.configure_only:
        return

    run([ninja, "-C", str(build_dir)], cwd=root, env=env)
    run([ninja, "-C", str(build_dir), "install"], cwd=root, env=env)
    install_artifacts_note(install_prefix)


if __name__ == "__main__":
    main()
