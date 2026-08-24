#!/usr/bin/env python3
"""Build Wayland core client-side (wayland-scanner host tool +
wayland-client) directly via tools/crt-cc, with NO Meson/Ninja/pkg-config
host-tool dependency at all -- every compile/link step below is a plain,
explicit subprocess call this script itself drives, mirroring the manual
embedded-cross-compile recipe the user pointed this session at (scanner
built and run on the host, the real library cross-compiled against a
pre-built libffi/libexpat), adapted onto this project's own tools/crt-cc
wrapper and its own already-ported expat/libffi.

This REPLACES an earlier Meson-based version of this file (2026-08-24):
that version worked in principle (the compiler was already tools/crt-cc,
same as here) but needed meson/ninja/pkg-config as real host build tools,
which this project's own sandboxed tooling cannot install and which real
embedded target environments often cannot host either. Removing the
orchestration layer entirely -- not just working around its absence --
matches this project's own stated "own the toolchain" philosophy more
directly: the only things this script depends on that aren't project-
owned are the C compiler itself (already true everywhere else in this
project) and this project's own already-built expat/libffi ports.

Scope, narrowed further from the original core-wide plan (recipe.json's
own notes): wayland-scanner + wayland-client ONLY for this pass, no
wayland-server/-cursor/-egl yet -- crtgfx-wayland-smoke (the only current
consumer) only needs the client library. Static archive only (no shared
.so this pass): avoids the two-copies-of-this-project's-own-libc risk a
shared libwayland-client.so linked against this project's own shared libc
would create for a smoke-test executable that otherwise links statically,
and a real .so needs its own SONAME-versioning ceremony matching this
project's own tools/crt-port-build.py::build_amalgamation_shared_library()
convention -- deferred until something actually needs it.

Every generated-file step below (wayland.dtd.h, wayland-version.h,
wayland-*-protocol*.h/.c) replicates real upstream Wayland 1.26.0
mechanics exactly (confirmed by fetching and reading the real src/embed.py,
src/scanner.c, src/wayland-version.h.in, src/meson.build from
libcrtgfx/third_party/wayland/recipe.json's own pinned commit) rather than
inventing an equivalent -- see each generator function's own docstring for
what it was checked against.
"""

import argparse
import os
import shutil
import subprocess
from pathlib import Path


def run(args, cwd=None, env=None):
    print("+", " ".join(str(arg) for arg in args), flush=True)
    subprocess.run(args, cwd=cwd, env=env, check=True)


def windows_short_path(path):
    """Exactly matches tools/build_skia.py's own windows_short_path()."""
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
    """Exactly matches tools/build_skia.py's own normalize_target_arch()."""
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


def find_llvm_tool(name):
    """Exactly matches tools/crt-port-build.py's own find_llvm_tool():
    a plain shutil.which(name) can miss a real, present LLVM install
    (confirmed for real on Debian/Ubuntu, where llvm-ar/llvm-ranlib are
    never symlinked onto PATH even though clang is) -- fall back to
    looking in the real clang binary's own resolved directory. Duplicated
    here rather than imported, matching this project's own tools/*.py
    convention of not sharing a common helper module."""
    found = shutil.which(name)
    if found:
        return found
    clang = shutil.which("clang")
    if not clang:
        return None
    candidate = Path(os.path.realpath(clang)).parent / name
    return str(candidate) if candidate.is_file() else None


def generate_dtd_header(dtd_path, dest_path, ident="wayland_dtd"):
    """Replicates real upstream src/embed.py's own output byte for byte
    (fetched and read directly from libcrtgfx/third_party/wayland/
    recipe.json's own pinned commit): a `static const char <ident>[] = {
    0x.., 0x.., ... };` byte-array literal of the DTD file's raw bytes.
    src/scanner.c #includes this unconditionally (`#include
    "wayland.dtd.h"`) even though the array is only ever *read* inside a
    `#if HAVE_LIBXML` block -- so the header must exist and declare the
    array regardless of whether DTD validation is actually compiled in."""
    data = dtd_path.read_bytes()
    lines = [f"static const char {ident}[] = {{", "\t"]
    parts = [f"0x{byte:02x}, " for byte in data]
    lines.append("".join(parts))
    lines.append("\n};\n")
    dest_path.parent.mkdir(parents=True, exist_ok=True)
    dest_path.write_text("".join(lines), encoding="utf-8")


def generate_version_header(dest_path, major, minor, micro):
    """Replicates real upstream src/wayland-version.h.in's own @VAR@
    substitution (the same four tokens Meson's own configuration_data()
    fills in from meson.project_version()) directly, without needing
    Meson itself to do it."""
    text = f"""#ifndef WAYLAND_VERSION_H
#define WAYLAND_VERSION_H

#define WAYLAND_VERSION_MAJOR {major}
#define WAYLAND_VERSION_MINOR {minor}
#define WAYLAND_VERSION_MICRO {micro}
#define WAYLAND_VERSION "{major}.{minor}.{micro}"

#endif
"""
    dest_path.parent.mkdir(parents=True, exist_ok=True)
    dest_path.write_text(text, encoding="utf-8")


def generate_config_header(dest_path):
    """src/connection.c and src/wayland-os.c both `#include "../config.h"`
    and check a handful of HAVE_* feature macros from it (confirmed by
    grepping the real fetched sources): HAVE_ACCEPT4, HAVE_SYS_UCRED_H,
    HAVE_XUCRED_CR_PID, HAVE_BROKEN_MSG_CMSG_CLOEXEC, HAVE_GETTID. Every
    one of them gates an *optional* code path with a real, already-present
    portable fallback when left undefined (accept()+fcntl() instead of
    accept4(), the SO_PEERCRED/struct ucred branch instead of FreeBSD's
    xucred one, the normal MSG_CMSG_CLOEXEC path instead of the FreeBSD-
    only broken-kernel workaround, an unlabeled thread id in one debug
    string instead of a real tid) -- confirmed directly by reading each
    call site, not assumed. Leaving all of them undefined (a header with
    no #defines at all) is therefore a real, correct, upstream-sanctioned
    configuration, not a workaround -- the same conservative "don't have
    it" answer a genuinely minimal/embedded host's own real Meson run
    would give for most of these anyway. SO_PEERCRED/struct ucred/
    MSG_CMSG_CLOEXEC themselves (the one branch with no such optional
    fallback -- src/wayland-os.c's own #else #error "Don't know how to
    read ucred on this platform" leaves no portable path at all) were
    instead added directly to this project's own include/sys/socket.h
    (2026-08-24), the real CRT/PAL surface Bionic-style ports are
    supposed to gain when a real gap like this is found."""
    dest_path.parent.mkdir(parents=True, exist_ok=True)
    dest_path.write_text(
        "/* Deliberately empty -- see tools/build_wayland.py's own\n"
        " * generate_config_header() docstring for why every HAVE_*\n"
        " * feature macro connection.c/wayland-os.c check is safe to\n"
        " * leave undefined here. */\n",
        encoding="utf-8",
    )


def compile_object(crt_cc, source, obj_path, include_dirs, defines, env, cwd):
    obj_path.parent.mkdir(parents=True, exist_ok=True)
    command = [str(crt_cc)]
    for include_dir in include_dirs:
        command.append(f"-I{include_dir}")
    for define in defines:
        command.append(f"-D{define}")
    command += ["-c", str(source), "-o", str(obj_path)]
    run(command, cwd=cwd, env=env)


def main():
    parser = argparse.ArgumentParser(description="Build Wayland core (scanner + client) directly via tools/crt-cc.")
    parser.add_argument("--root", required=True, help="CRT repository root")
    parser.add_argument("--source", required=True, help="Wayland source checkout")
    parser.add_argument("--build-dir", required=True, help="Wayland build output directory")
    parser.add_argument("--install-prefix", required=True, help="Wayland install prefix used by libcrtgfx")
    parser.add_argument("--sysroot", required=True, help="CRT sysroot")
    parser.add_argument("--rootfs", default="", help="CRT rootfs (Windows only, for mksh.exe)")
    parser.add_argument("--windows-sdk-libpath", default="",
                         help="Windows only: directory containing kernel32.lib/synchronization.lib "
                              "(the calling CMake configure's own ${CRT_WINDOWS_KERNEL32_LIB} directory) "
                              "-- needed because this script links wayland-scanner.exe directly via "
                              "tools/crt-cc, bypassing the target_link_libraries(${CRT_WINDOWS_SYSTEM_LIBS}) "
                              "path every regular CMake-built Windows executable in this project already uses")
    parser.add_argument("--port-prefix", required=True,
                         help="shared porting/recipes install prefix (out/<preset>/port-tests/install) "
                              "holding expat/libffi headers+libs")
    parser.add_argument("--target-os", required=True, choices=["linux", "macos", "windows"])
    parser.add_argument("--target-arch", default="host")
    parser.add_argument("--configure-only", action="store_true",
                         help="stop after generating headers/building wayland-scanner (no wayland-client build)")
    args = parser.parse_args()
    args.target_arch = normalize_target_arch(args.target_arch)

    root = Path(args.root).resolve()
    source = Path(args.source).resolve()
    build_dir = Path(args.build_dir).resolve()
    install_prefix = Path(args.install_prefix).resolve()
    sysroot = Path(args.sysroot).resolve()
    port_prefix = Path(args.port_prefix).resolve()
    build_dir.mkdir(parents=True, exist_ok=True)

    cmd_suffix = ".cmd" if args.target_os == "windows" else ""
    crt_cc = root / "tools" / f"crt-cc{cmd_suffix}"
    exe_suffix = ".exe" if args.target_os == "windows" else ""

    ar = find_llvm_tool("llvm-ar") or shutil.which("ar") or "ar"
    ranlib = find_llvm_tool("llvm-ranlib") or shutil.which("ranlib") or "ranlib"

    env = os.environ.copy()
    env["CRT_SYSROOT"] = str(sysroot)
    env["CRT_TARGET_OS"] = args.target_os
    if args.target_arch != "host":
        env["CRT_TARGET_ARCH"] = args.target_arch

    if args.target_os == "windows":
        # Exactly matches tools/build_skia.py's own Windows setup: crt-cc.cmd
        # needs CRT_MKSH_EXE/CRT_ROOTFS set explicitly (see that file's own
        # comments for the full "why"). CRT_HOST_CC/host_ar/host_ranlib are
        # all resolved via *this* process's own, unrestricted PATH first
        # (below), before env["PATH"] gets narrowed.
        if not args.rootfs:
            raise SystemExit("--rootfs is required on Windows (its mksh.exe launches crt-cc.cmd)")
        mksh = Path(args.rootfs).resolve() / "system" / "bin" / "mksh.exe"
        if not mksh.is_file():
            raise SystemExit(f"CRT mksh is missing from the rootfs: {mksh} (build the \"rootfs\" target first)")
        env["CRT_MKSH_EXE"] = str(mksh)
        env["CRT_ROOTFS"] = str(Path(args.rootfs).resolve())
        host_cc = shutil.which("clang")
        if host_cc:
            env["CRT_HOST_CC"] = windows_short_path(host_cc).replace("\\", "/")
        host_ar = shutil.which("llvm-ar")
        if host_ar:
            ar = host_ar
        host_ranlib = shutil.which("llvm-ranlib")
        if host_ranlib:
            ranlib = host_ranlib
        # CORRECTED (2026-08-24): an earlier version of this file's own
        # comment here claimed PATH never needs narrowing on Windows,
        # reasoning that tools/crt-cc's own script never does a bare-name
        # PATH lookup once CRT_HOST_CC/CRT_TARGET_OS/CRT_TARGET_ARCH are
        # set. Confirmed wrong for real, running this exact script for the
        # first time on Windows: crt-cc's own script body calls the plain
        # shell command `printf` (a toybox applet, e.g. windows_host_arg()'s
        # `printf '%s\n' "$1"`, and the user_args-building loop's `$(printf
        # '%s\n' "$arg")` -- unconditional, on every single invocation) and
        # `sed` (inside windows_host_arg()'s own rootfs-path rewriting) via
        # mksh's own bare-name PATH search -- `crt-cc[71]: printf:
        # inaccessible or not found`, the compiler wrapper itself failing
        # before clang is ever reached. mksh's own PATH search only works
        # against the deliberate ":"-separated, rootfs-relative POSIX form
        # (see tools/build_skia.py's own matching comment for the fuller
        # "why"), so it has to be set here exactly like Skia's own GN build
        # needs it -- this script is not actually exempt from that
        # requirement after all. Every OTHER subprocess call this script
        # makes (ar/ranlib -- absolute host paths, resolved above, invoked
        # directly by Python's own subprocess without going through mksh at
        # all; the built wayland-scanner.exe itself -- a plain native
        # executable with no PATH dependency of its own) is unaffected by
        # this narrowing, so it is safe to apply for this whole script's
        # env, not just the crt-cc calls specifically.
        env["PATH"] = "/system/bin:/bin:/usr/bin"
        # tools/crt-cc's own Windows link branch falls back to bare
        # `-Wl,kernel32.lib -Wl,synchronization.lib` (resolved via ld.lld's
        # own default library search, which does not include the real
        # Windows SDK lib directory) whenever CRT_WINDOWS_SYSTEM_LIBS is
        # unset -- confirmed for real: `ld.lld: error: could not open
        # 'kernel32.lib'` linking wayland-scanner.exe, the first time this
        # script tried to link a real Windows executable directly via
        # crt-cc at all (every other Windows executable in this project
        # links through regular CMake's own target_link_libraries(
        # ${CRT_WINDOWS_SYSTEM_LIBS}), which passes full absolute paths
        # and never hits this fallback; tools/build_skia.py never links an
        # executable at all, only libskia.a, so it never hit this either).
        # CRT_WINDOWS_SDK_LIBPATH (not CRT_WINDOWS_SYSTEM_LIBS itself) is
        # what crt-cc's own script actually reads to fix this -- see its
        # own `-Wl,/libpath:${CRT_WINDOWS_SDK_LIBPATH}` handling.
        if args.windows_sdk_libpath:
            env["CRT_WINDOWS_SDK_LIBPATH"] = args.windows_sdk_libpath

    src = source / "src"
    generated = build_dir / "generated"
    obj_dir = build_dir / "obj"
    generated.mkdir(parents=True, exist_ok=True)
    obj_dir.mkdir(parents=True, exist_ok=True)

    # --- Generated headers (real upstream mechanics, replicated directly -- see each function's own docstring) ---
    generate_version_header(generated / "wayland-version.h", 1, 26, 0)
    generate_dtd_header(source / "protocol" / "wayland.dtd", generated / "wayland.dtd.h")
    generate_config_header(build_dir / "config.h")

    common_includes = [src, generated, build_dir]
    posix_define = "_POSIX_C_SOURCE=200809L"

    # --- Phase 1: wayland-scanner, a host tool -- but still built via
    # tools/crt-cc against this project's own expat, exactly like every
    # other executable this project's toolchain produces (host==target
    # throughout this whole project, so the result is directly runnable
    # on the same machine that built it -- no separate "host compiler"
    # is needed the way a real embedded cross-build's own guide would use
    # one). ---
    expat_include = port_prefix / "include"
    expat_lib = port_prefix / "lib" / "libexpat.a"
    if not expat_lib.is_file():
        raise SystemExit(f"expected expat static library missing: {expat_lib} (build port-build-expat first)")

    scanner_objs = []
    for source_file in ("scanner.c", "wayland-util.c"):
        obj = obj_dir / f"{source_file}.o"
        compile_object(
            crt_cc, src / source_file, obj,
            include_dirs=common_includes + [expat_include],
            defines=[posix_define],
            env=env, cwd=root,
        )
        scanner_objs.append(obj)

    scanner_bin = build_dir / f"wayland-scanner{exe_suffix}"
    run([str(crt_cc)] + [str(o) for o in scanner_objs] + [str(expat_lib), "-o", str(scanner_bin)], cwd=root, env=env)

    if args.configure_only:
        print(f"wayland-scanner built: {scanner_bin}")
        return

    # --- Phase 2: run wayland-scanner to generate the client-side protocol
    # bindings from the real core protocol/wayland.xml (no wayland-
    # protocols/xdg-shell needed for this -- core wayland.xml alone is
    # what libwayland-client.c itself needs to build; see recipe.json's
    # own notes on why wayland-protocols is deferred). ---
    protocol_xml = source / "protocol" / "wayland.xml"
    client_protocol_core_h = generated / "wayland-client-protocol-core.h"
    client_protocol_h = generated / "wayland-client-protocol.h"
    protocol_c = generated / "wayland-protocol.c"

    run([str(scanner_bin), "-c", "client-header", str(protocol_xml), str(client_protocol_core_h)], cwd=root, env=env)
    run([str(scanner_bin), "client-header", str(protocol_xml), str(client_protocol_h)], cwd=root, env=env)
    run([str(scanner_bin), "public-code", str(protocol_xml), str(protocol_c)], cwd=root, env=env)

    # --- Phase 3: compile wayland-client's own real sources against
    # this project's own expat is not needed here (only the scanner needs
    # it) -- libffi is, for connection.c's own closure-based argument
    # marshaling. wayland-util.c/wayland-os.c are shared with wayland-
    # server (deferred, see this file's own top-of-file scope note) but
    # still required here: wayland-client.c calls into both directly. ---
    libffi_include = port_prefix / "include"
    libffi_lib = port_prefix / "lib" / "libffi.a"
    if not libffi_lib.is_file():
        raise SystemExit(f"expected libffi static library missing: {libffi_lib} (build port-build-libffi first)")

    client_sources = [
        src / "wayland-client.c",
        src / "connection.c",
        src / "wayland-os.c",
        src / "wayland-util.c",
        protocol_c,
    ]
    client_objs = []
    for source_file in client_sources:
        obj = obj_dir / f"{source_file.name}.client.o"
        compile_object(
            crt_cc, source_file, obj,
            include_dirs=common_includes + [libffi_include],
            defines=[posix_define],
            env=env, cwd=root,
        )
        client_objs.append(obj)

    lib_dir = install_prefix / "lib"
    include_dir = install_prefix / "include"
    lib_dir.mkdir(parents=True, exist_ok=True)
    include_dir.mkdir(parents=True, exist_ok=True)

    static_archive = lib_dir / "libwayland-client.a"
    run([ar, "rcs", str(static_archive)] + [str(o) for o in client_objs], cwd=root, env=env)
    run([ranlib, str(static_archive)], cwd=root, env=env)

    for header in ("wayland-util.h", "wayland-client.h", "wayland-client-core.h"):
        shutil.copy2(src / header, include_dir / header)
    shutil.copy2(client_protocol_h, include_dir / "wayland-client-protocol.h")
    shutil.copy2(client_protocol_core_h, include_dir / "wayland-client-protocol-core.h")
    # wayland-client-core.h itself #includes "wayland-version.h" -- a
    # consumer compiling against the installed headers alone (not this
    # build's own -I.../generated) needs it installed alongside the
    # others, not just left sitting in the build directory.
    shutil.copy2(generated / "wayland-version.h", include_dir / "wayland-version.h")

    print(f"Wayland client installed into {install_prefix}")


if __name__ == "__main__":
    main()
