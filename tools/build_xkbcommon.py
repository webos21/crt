#!/usr/bin/env python3
"""Build libxkbcommon's core keymap/state API (xkb_context/xkb_keymap/
xkb_state only -- no xkbregistry, no Compose) directly via tools/crt-cc,
with NO Meson/Ninja/pkg-config/bison host-tool dependency at all, mirroring
tools/build_wayland.py's own established "own the toolchain" shape exactly
-- see that file's own top-of-file docstring for the shared reasoning.

The one real difference from Wayland's own build: libxkbcommon's core
library has exactly one generated-file requirement (src/xkbcomp/parser.y,
a real bison/yacc grammar). Regenerating it would need a new bison-as-
host-tool port -- a large, general-purpose parser-generator project, wildly
out of proportion to what one grammar file needs -- so instead this
project pre-generated it once (see libcrtgfx/third_party/xkbcommon/
generated/README.md for the exact command/provenance) and commits the
result; this script copies that committed output into place rather than
invoking bison itself.

Scope, matching libcrtgfx/third_party/xkbcommon/recipe.json's own notes:
core xkb_context/xkb_keymap/xkb_state only. xkbregistry (a separate UI-
layout-picker library) and the Compose/dead-key subsystem are both
deferred -- neither is needed to turn a wl_keyboard::key event into real
UTF-8 text, this port's only actual consumer right now.
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
    """Exactly matches tools/build_wayland.py's own windows_short_path()."""
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
    """Exactly matches tools/build_wayland.py's own normalize_target_arch()."""
    if target_arch_arg == "host":
        return target_arch_arg
    arch = target_arch_arg.lower()
    if arch in ("aarch64", "arm64"):
        return "aarch64"
    if arch in ("x86_64", "amd64", "x64"):
        return "x86_64"
    raise SystemExit(
        f"build_xkbcommon.py: unrecognized --target-arch {target_arch_arg!r} "
        "(expected aarch64/arm64 or x86_64/amd64/x64, or \"host\")"
    )


def find_llvm_tool(name):
    """Exactly matches tools/build_wayland.py's own find_llvm_tool()."""
    found = shutil.which(name)
    if found:
        return found
    clang = shutil.which("clang")
    if not clang:
        return None
    candidate = Path(os.path.realpath(clang)).parent / name
    return str(candidate) if candidate.is_file() else None


# The 30 real core-library sources this port needs, in the exact order
# real upstream meson.build's own libxkbcommon_sources lists them
# (confirmed by reading it directly) -- minus src/compose/*.c (deferred,
# see this file's own top-of-file docstring) and minus src/xkbcomp/
# parser.y itself, which is pre-generated (see CORE_GENERATED_SOURCES
# below) rather than compiled from the .y grammar directly.
CORE_SOURCES = [
    "src/xkbcomp/action.c",
    "src/xkbcomp/ast-build.c",
    "src/xkbcomp/compat.c",
    "src/xkbcomp/expr.c",
    "src/xkbcomp/include.c",
    "src/xkbcomp/keycodes.c",
    "src/xkbcomp/keymap.c",
    "src/xkbcomp/keymap-dump.c",
    "src/xkbcomp/keywords.c",
    "src/xkbcomp/rules.c",
    "src/xkbcomp/scanner.c",
    "src/xkbcomp/symbols.c",
    "src/xkbcomp/types.c",
    "src/xkbcomp/vmod.c",
    "src/xkbcomp/xkbcomp.c",
    "src/atom.c",
    "src/context.c",
    "src/context-priv.c",
    "src/keysym.c",
    "src/keysym-case-mappings.c",
    "src/keysym-utf.c",
    "src/keymap.c",
    "src/keymap-priv.c",
    "src/scanner-utils.c",
    "src/state.c",
    "src/text.c",
    "src/utf8.c",
    "src/utf8-decoding.c",
    "src/utils.c",
    "src/utils-paths.c",
]

# Public headers a consumer needs -- just include/xkbcommon/xkbcommon.h
# (the core context/keymap/state API this port actually builds); xkbcommon-
# compat.h/xkbcommon-names.h are small standalone headers xkbcommon.h
# itself #includes, so they need installing alongside it. xkbcommon-
# compose.h/xkbcommon-x11.h/xkbregistry.h are for the deferred subsystems
# (Compose, X11 helpers, xkbregistry) this build does not compile at all --
# installing them would advertise API this static archive cannot satisfy.
PUBLIC_HEADERS = [
    "xkbcommon.h",
    "xkbcommon-keysyms.h",
    "xkbcommon-names.h",
    "xkbcommon-compat.h",
]


def generate_config_header(dest_path, target_os):
    """Replicates real upstream meson.build's own configh_data setup
    (confirmed by reading it directly, not assumed) with every HAVE_*
    value decided against this project's own real libc surface -- see
    libcrtgfx/third_party/xkbcommon/recipe.json's own notes for the full
    per-macro reasoning (which functions this project's own libc really
    has, which are dead config entries no core source file even
    references, and why PATH_MAX is deliberately NOT set here since this
    project's own include/limits.h already defines it -- redefining it
    here would collide, exactly matching upstream's own `if not
    cc.has_header_symbol('limits.h', 'PATH_MAX', ...)` guard)."""
    # /usr/share/X11/xkb: the real, standard system XKB data path -- see
    # this port's own recipe.json notes for why a real, already-installed
    # xkb-data package is the correct thing to point at here, not
    # something this port bundles itself. Only meaningful on Linux today
    # (no real consumer on macOS/Windows yet, see recipe.json's own
    # status block), kept as one path for every target_os for now rather
    # than branching on a distinction with no actual difference to make.
    xkb_config_root = "/usr/share/X11/xkb"
    text = f"""#ifndef XKBCOMMON_CONFIG_H
#define XKBCOMMON_CONFIG_H

#define EXIT_INVALID_USAGE 2
#define LIBXKBCOMMON_VERSION "1.9.2"
#define LIBXKBCOMMON_TOOL_PATH ""
#define _GNU_SOURCE 1
#define DFLT_XKB_CONFIG_ROOT "{xkb_config_root}"
#define DFLT_XKB_CONFIG_EXTRA_PATH "/etc/xkb"
#define XLOCALEDIR "/usr/share/X11/locale"
#define DEFAULT_XKB_RULES "evdev"
#define DEFAULT_XKB_MODEL "pc105"
#define DEFAULT_XKB_LAYOUT "us"
#define DEFAULT_XKB_VARIANT NULL
#define DEFAULT_XKB_OPTIONS NULL
#define HAVE_UNISTD_H 1
#define HAVE___BUILTIN_EXPECT 1
#define HAVE_EACCESS 1
#define HAVE_MMAP 1
#define HAVE_STRNDUP 1
#define HAVE_ASPRINTF 1
#define HAVE_VASPRINTF 1
#define HAVE_OPEN_MEMSTREAM 1

#endif
"""
    dest_path.parent.mkdir(parents=True, exist_ok=True)
    dest_path.write_text(text, encoding="utf-8")


def compile_object(crt_cc, source, obj_path, include_dirs, defines, env, cwd):
    obj_path.parent.mkdir(parents=True, exist_ok=True)
    # -std=c11: real upstream meson.build's own default_options (confirmed
    # by reading it directly: project(..., default_options: ['c_std=c11',
    # ...])) -- src/keymap.h's own real, bare `static_assert(...)` uses
    # (no #include <assert.h> in that translation unit) rely on C11's own
    # _Static_assert being a genuine *keyword* at this standard level, not
    # a macro this project's own tools/crt-cc default (older than C11)
    # provides -- confirmed for real: without this flag, clang parsed
    # `static_assert(...)` as an implicit-int function declaration and
    # failed outright (`type specifier missing, defaults to 'int'`).
    # -fPIC: libxkbcommon.a is linked into both crtgfx (a static archive,
    # PIC-agnostic) and crtgfx_shared (libcrtgfx.so). Found for real via a
    # full crtgfx_shared link attempt: without -fPIC, x86_64-linux-gnu-ld
    # rejected src_xkbcomp_keymap.c.o outright ("relocation R_X86_64_PC32
    # against symbol `default_interpret` can not be used when making a
    # shared object; recompile with -fPIC") -- this is this project's own
    # first third-party static archive actually linked into crtgfx_shared
    # (the hand-rolled Wayland wire-protocol client in window_wayland.c
    # never linked libwayland-client.a itself), so no prior port needed
    # this flag; libcrtgfx's own CMake-driven objects get it automatically
    # via CMAKE_POSITION_INDEPENDENT_CODE, which this ad hoc crt-cc-driven
    # build bypasses entirely and must therefore set explicitly.
    command = [str(crt_cc), "-std=c11", "-fPIC"]
    for include_dir in include_dirs:
        command.append(f"-I{include_dir}")
    for define in defines:
        command.append(f"-D{define}")
    command += ["-c", str(source), "-o", str(obj_path)]
    run(command, cwd=cwd, env=env)


def main():
    parser = argparse.ArgumentParser(description="Build libxkbcommon's core API directly via tools/crt-cc.")
    parser.add_argument("--root", required=True, help="CRT repository root")
    parser.add_argument("--source", required=True, help="libxkbcommon source checkout")
    parser.add_argument("--build-dir", required=True, help="libxkbcommon build output directory")
    parser.add_argument("--install-prefix", required=True, help="libxkbcommon install prefix used by libcrtgfx")
    parser.add_argument("--sysroot", required=True, help="CRT sysroot")
    parser.add_argument("--rootfs", default="", help="CRT rootfs (Windows only, for mksh.exe)")
    parser.add_argument("--windows-sdk-libpath", default="",
                         help="Windows only: directory containing kernel32.lib/synchronization.lib "
                              "-- see tools/build_wayland.py's own matching flag for the full reasoning")
    parser.add_argument("--target-os", required=True, choices=["linux", "macos", "windows"])
    parser.add_argument("--target-arch", default="host")
    parser.add_argument("--configure-only", action="store_true",
                         help="stop after generating config.h and copying in the pre-generated parser")
    args = parser.parse_args()
    args.target_arch = normalize_target_arch(args.target_arch)

    root = Path(args.root).resolve()
    source = Path(args.source).resolve()
    build_dir = Path(args.build_dir).resolve()
    install_prefix = Path(args.install_prefix).resolve()
    sysroot = Path(args.sysroot).resolve()
    build_dir.mkdir(parents=True, exist_ok=True)

    cmd_suffix = ".cmd" if args.target_os == "windows" else ""
    crt_cc = root / "tools" / f"crt-cc{cmd_suffix}"

    ar = find_llvm_tool("llvm-ar") or shutil.which("ar") or "ar"
    ranlib = find_llvm_tool("llvm-ranlib") or shutil.which("ranlib") or "ranlib"

    env = os.environ.copy()
    env["CRT_SYSROOT"] = str(sysroot)
    env["CRT_TARGET_OS"] = args.target_os
    if args.target_arch != "host":
        env["CRT_TARGET_ARCH"] = args.target_arch

    if args.target_os == "windows":
        # Exactly matches tools/build_wayland.py's own Windows setup -- see
        # that file's own comments for the full "why" of every piece here.
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
        env["PATH"] = "/system/bin:/bin:/usr/bin"
        if args.windows_sdk_libpath:
            env["CRT_WINDOWS_SDK_LIBPATH"] = args.windows_sdk_libpath

    src = source / "src"
    include = source / "include"
    generated_dir = root / "libcrtgfx" / "third_party" / "xkbcommon" / "generated"

    # --- config.h + the pre-generated parser.c/parser.h (see this file's
    # own top-of-file docstring for why parser.c is committed, not
    # regenerated here). Copied into the real source tree's own
    # src/xkbcomp/ directory (not just referenced via an extra -I) so
    # #include "parser.h" from other xkbcomp/*.c files resolves exactly
    # the way it would in a real upstream build tree. ---
    generate_config_header(build_dir / "config.h", args.target_os)
    shutil.copy2(generated_dir / "parser.c", src / "xkbcomp" / "parser.c")
    shutil.copy2(generated_dir / "parser.h", src / "xkbcomp" / "parser.h")

    if args.configure_only:
        print(f"libxkbcommon configured: {build_dir}")
        return

    # root / "include": this project's own raw libc headers (stdio.h,
    # assert.h, string.h, ...), included directly from the source tree --
    # NOT via tools/crt-cc's own default -isystem${CRT_SYSROOT}/include.
    # Found for real via a GitHub Actions CI failure (2026-08-25) on a
    # from-scratch checkout: "fatal error: 'stdio.h' file not found" at
    # xkbcommon.h's own #include <stdio.h>, because crtgfx-xkbcommon-build
    # has no ninja dependency edge forcing the `sysroot` custom target
    # (the thing that actually populates ${CRT_SYSROOT}/include by
    # copying this project's own include/ into the build tree) to run
    # first -- and it deliberately CAN'T have one: `sysroot` itself
    # DEPENDS on crtgfx/crtgfx_shared (CMakeLists.txt), which in turn
    # DEPEND on crtgfx-xkbcommon-build (libcrtgfx/CMakeLists.txt) to link
    # libxkbcommon.a, so crtgfx-xkbcommon-build -> sysroot -> crtgfx ->
    # crtgfx-xkbcommon-build would be a real ninja dependency cycle (the
    # same class of cycle libcrtgfx/CMakeLists.txt's own CRTGFX_ENABLE_SKIA
    # comments already document and deliberately avoid for Skia, by never
    # creating a real edge onto crtgfx-skia-build in the first place). This
    # never surfaced locally because a build directory reused across
    # sessions already had ${CRT_SYSROOT}/include populated from an
    # earlier, unrelated full build -- only a genuinely fresh checkout
    # (i.e. CI) exposes it. Since this project's own include/ is plain
    # source-controlled header files with no build step of their own, an
    # extra -I straight at the repo's include/ directory is always valid
    # immediately, with no ordering dependency on anything at all.
    common_includes = [src, include, build_dir, root / "include"]
    defines = ["HAVE_CONFIG_H"]

    obj_dir = build_dir / "obj"
    objs = []
    for rel_source in CORE_SOURCES:
        source_file = src.parent / rel_source
        obj = obj_dir / (rel_source.replace("/", "_") + ".o")
        compile_object(crt_cc, source_file, obj, include_dirs=common_includes, defines=defines, env=env, cwd=root)
        objs.append(obj)
    parser_obj = obj_dir / "src_xkbcomp_parser.c.o"
    compile_object(
        crt_cc, src / "xkbcomp" / "parser.c", parser_obj,
        include_dirs=common_includes, defines=defines, env=env, cwd=root,
    )
    objs.append(parser_obj)

    lib_dir = install_prefix / "lib"
    include_dir = install_prefix / "include" / "xkbcommon"
    lib_dir.mkdir(parents=True, exist_ok=True)
    include_dir.mkdir(parents=True, exist_ok=True)

    static_archive = lib_dir / "libxkbcommon.a"
    run([ar, "rcs", str(static_archive)] + [str(o) for o in objs], cwd=root, env=env)
    run([ranlib, str(static_archive)], cwd=root, env=env)

    for header in PUBLIC_HEADERS:
        shutil.copy2(include / "xkbcommon" / header, include_dir / header)

    print(f"libxkbcommon installed into {install_prefix}")


if __name__ == "__main__":
    main()
