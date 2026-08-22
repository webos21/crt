#!/usr/bin/env python3
import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


def run(args, cwd=None, env=None):
    print("+", " ".join(str(arg) for arg in args), flush=True)
    subprocess.run(args, cwd=cwd, env=env, check=True)


def pin_gn_script_executable(source, host_python):
    """Skia's own .gn dotfile hardcodes `script_executable = "python3"` --
    a real, unconditional upstream requirement `gn gen` needs to resolve,
    normally via a real, native-Windows-format PATH search (this is GN's
    own internal exec logic, not this project's PAL -- it never goes
    through mksh.exe at all). Most Linux/macOS Python installs already
    provide a "python3"-named executable by distro/package-manager
    convention; a stock Windows Python install typically does not (only
    "python.exe"). Confirmed for real: `gn gen` fails outright with
    `ERROR Could not find "python3" from dotfile in PATH` without some
    fix on Windows.

    This used to be solved with a prepended-to-PATH .bat shim, but that
    directly conflicts with a *separate*, unrelated requirement on the
    exact same env["PATH"] value: gn gen's own is_clang.py compiler probe
    (gn/BUILDCONFIG.gn's exec_script("//gn/is_clang.py", ...)) shells out
    to crt-cc.cmd, which launches mksh.exe -- and mksh.exe needs PATH to
    stay this project's own deliberate ":"-separated, rootfs-relative
    POSIX form ("/system/bin:/bin:/usr/bin", see shell/toybox/PATCHES.md's
    own MKSH_CRT_WINPATH writeup for why MKSH_PATHSEPC is ':' and stays
    that way on this project's Windows build). Confirmed for real
    (2026-08-22): prepending a real, backslash-form Windows directory to
    that POSIX PATH still broke mksh's *own* internal PATH search (mksh
    splits PATH on ':' only, so a Windows drive-letter entry like
    "C:\\...\\rootfs\\system\\bin" gets misread as two garbage entries,
    "C" and "\\...\\rootfs\\system\\bin" -- there is no format that
    satisfies both a real Windows PATH search and mksh's POSIX one at the
    same time).

    Patching the fetched .gn dotfile's own `script_executable` value to
    an absolute path instead sidesteps the conflict entirely: GN execs
    that value directly with no PATH search of any kind, so PATH is free
    to stay purely POSIX-style for mksh's sake. Idempotent (checks the
    current content first) so a re-run against an already-patched fetch
    is a no-op.
    """
    dotfile = source / ".gn"
    text = dotfile.read_text(encoding="utf-8")
    pinned = f'script_executable = "{Path(host_python).as_posix()}"'
    if pinned in text:
        return
    patched, count = re.subn(r'^script_executable\s*=.*$', pinned, text, count=1, flags=re.MULTILINE)
    if count != 1:
        raise SystemExit(f'{dotfile}: could not find a `script_executable = ...` line to patch')
    dotfile.write_text(patched, encoding="utf-8")


def pin_gcc_toolchain_python(source, host_python):
    """gn/toolchain/BUILD.gn's own gcc_like_toolchain template (the one this
    project's Windows build actually uses -- see default_gn_args()'s own
    `target_os = "linux"` trick, which selects this template instead of
    msvc_toolchain) hardcodes the bare, unqualified string "python3" into
    three of its own generated ninja `command = ...` lines (tool("alink")'s
    pre-archive rm.py cleanup, tool("copy")/tool("copy_bundle_data")'s own
    cp.py). Unlike script_executable above, GN does not resolve this one
    through its own exec_script machinery at all -- it is literal ninja
    rule text, executed later by ninja.exe's own `cmd.exe /c ...` subprocess
    spawn, once actual compilation starts (not during `gn gen`).

    This project's own env["PATH"] for that ninja invocation is
    deliberately kept pure POSIX/rootfs-relative (see main()'s own
    CRT_MKSH_EXE-adjacent comment for why crt-cc.cmd/crt-c++.cmd need
    that), which a native Windows CreateProcess-based command lookup
    cannot parse as a real search path at all -- confirmed for real
    (2026-08-22): `'python3'은(는) 내부 또는 외부 명령... 아닙니다`
    (Windows' own "not recognized as an internal or external command")
    failing tool("alink")'s own `python3 rm.py "$out" && ar rcs ...`
    compound command specifically, right after a real Windows libcxx
    build finally succeeded and Skia's own build was retried for the
    first time since. Same root cause and same fix shape as
    pin_gn_script_executable() above, applied to a different file: replace
    the bare "python3" token with an absolute, forward-slashed path so no
    PATH search is ever needed for it, regardless of what PATH's own
    content or separator convention is at ninja-run time. Windows-only:
    on real Linux/macOS (this same gcc_like_toolchain template's other
    real users), a "python3"-named executable is normally already on
    PATH by distro/package-manager convention, so leaving those hosts
    unpatched avoids touching an already-working path for no reason.
    Idempotent (checks the current content first) so a re-run against an
    already-patched fetch is a no-op.
    """
    toolchain_file = source / "gn" / "toolchain" / "BUILD.gn"
    text = toolchain_file.read_text(encoding="utf-8")
    # Every occurrence sits bare (unquoted) *inside* an already-open GN
    # `command = "..."` string literal (e.g. `"$shell python3 \"$cp_py\"
    # ..."`), never as its own top-level assignment the way
    # script_executable is above. First attempt (2026-08-22) wrapped the
    # substituted path in GN-string-escaped `\"...\"` (a real, quoted path
    # in the ninja command line GN eventually emits) -- syntactically
    # correct GN, but wrong in practice: confirmed for real, this
    # produces a *second* `cmd.exe /c "..." "..." "..." && ...` compound
    # command with three separate quoted segments, which trips a real,
    # documented cmd.exe quirk (`cmd /?`'s own description of `/C`
    # argument handling): unless the ENTIRE command line is exactly one
    # quoted executable name with nothing else quoted, cmd.exe strips
    # only the very first and very last quote character of the whole
    # line rather than leaving each quoted segment intact -- corrupting
    # the command into "지정된 이름, 디렉터리 이름 또는 볼륨 레이블
    # 구문이 틀렸습니다" (Windows' own "the filename, directory name, or
    # volume label syntax is incorrect"). Fixed the same way this file's
    # own windows_short_path() already exists for: an 8.3 short path
    # never contains spaces by construction, so it can be substituted
    # bare/unquoted -- matching the original "python3" token's own
    # unquoted shape exactly -- without ever needing quoting (or hitting
    # this cmd.exe trap) at all, regardless of where Python is installed.
    python_path = windows_short_path(host_python).replace("\\", "/")
    if python_path in text and "python3" not in text:
        return
    patched, count = re.subn(r"\bpython3\b", python_path, text)
    if count == 0:
        raise SystemExit(f'{toolchain_file}: could not find any bare "python3" token to patch')
    toolchain_file.write_text(patched, encoding="utf-8")


def windows_short_path(path):
    """Convert an absolute Windows path to its 8.3 short-name form.

    mksh's own exec()/command-lookup genuinely cannot run a program whose
    path contains a space, forward slashes or not -- confirmed for real
    (2026-08-22) by isolating to a two-line repro run directly via
    mksh.exe: `"/c/Program Files/LLVM/bin/clang++" --version` failed
    "inaccessible or not found" even though the file exists and the path
    already uses forward slashes (so this is not the separate, already-
    documented "mksh needs forward slashes, not backslashes" gotcha --
    see tools/crt-libcxx-build.py's own comments for that one). A stock
    Windows LLVM install always lands under "C:\\Program Files\\LLVM\\...",
    so CRT_HOST_CC/CRT_HOST_CXX need the short-path form to be usable at
    all. Exactly matches tools/crt-port-build.py's own windows_short_path()
    (same GetShortPathNameW win32 call), duplicated here rather than
    imported since none of this project's own tools/*.py scripts share a
    common helper module today."""
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
    """Resolves an explicit --target-arch to "aarch64" or "x86_64" --
    same normalization tools/crt-libcxx-build.py's own
    detect_target_arch() already applies, needed here for the identical
    reason: libcrtgfx/CMakeLists.txt passes CRTGFX_SKIA_BUILD_ARGS'
    --target-arch straight from a CMake variable (CMAKE_SYSTEM_PROCESSOR-
    shaped, e.g. Windows' own "AMD64"/"ARM64" spelling), not the GNU-
    triple spelling ("x86_64"/"aarch64") tools/crt-cc's own
    `--target=${target_arch}-w64-mingw32` construction requires.
    Confirmed for real (2026-08-22): left unnormalized, CRT_TARGET_ARCH
    ended up literally "AMD64", producing `--target=AMD64-w64-mingw32`
    -- not a real clang triple -- which surfaced as `clang++: error:
    unsupported option '-mavx2' for target 'AMD64-w64-mingw32'` (and,
    for TUs that don't pass any -m flags at all, the blunter `error:
    unknown target triple 'AMD64-w64-windows-gnu'`). The case-insensitive
    match here also covers default_gn_args()'s own target_cpu selection,
    which previously silently matched neither of its lowercase-only
    ("aarch64"/"arm64" or "x86_64"/"amd64"/"x64") branches for "AMD64"
    and fell back on GN's own host-arch autodetection -- happened to
    still be correct on this x86_64 dev machine, but not guaranteed in
    general. "host" is passed through unchanged: it is main()'s own
    sentinel for "no explicit arch, let CRT_TARGET_ARCH stay unset and
    tools/crt-cc/tools/crt-c++ fall back to platform detection"."""
    if target_arch_arg == "host":
        return target_arch_arg
    arch = target_arch_arg.lower()
    if arch in ("aarch64", "arm64"):
        return "aarch64"
    if arch in ("x86_64", "amd64", "x64"):
        return "x86_64"
    raise SystemExit(
        f"build_skia.py: unrecognized --target-arch {target_arch_arg!r} "
        "(expected aarch64/arm64 or x86_64/amd64/x64, or \"host\")"
    )


def gn_string(value):
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def gn_list(values):
    return "[" + ", ".join(gn_string(value) for value in values) + "]"


# REMOVED (2026-08-21): this file used to probe the host compiler
# (`clang++ -x c++ -E -v -`) for its own default C++ standard-library
# include directories and inject whichever it found -- real libc++'s own
# /c++/v1 *or* real MSVC's own VC/Tools/MSVC/.../include -- into every
# Skia compile via extra_cflags_cc, on the theory that Skia genuinely
# needs a real <vector>/<string>/etc. implementation from somewhere and
# this project had not yet imported its own. Confirmed for real
# (2026-08-21) that this was the root cause of a genuine, previously-
# undiscovered Windows link failure: `crtgfx_skia_raster_smoke.exe`
# failed with a long list of `lld-link: error: duplicate symbol`
# (printf, fprintf, snprintf, fabsf, fabsl, frexpl, wmemcpy, wmemset,
# wmemcmp, plus a silently auto-linked libcpmt.lib) between this
# project's own c.lib/m.lib and objects (libskia.a's own members, and a
# transitively-auto-linked real MSVC C++ runtime library) that carry
# their own copies of the same symbols -- because probing found real
# MSVC's own STL/UCRT headers on Windows (no imported libc++ existed at
# the time this file was first written) and Skia's own compiled code
# materialized real UCRT inline printf/wmemcpy/etc. as strong symbols,
# which then collided with this project's own freestanding libc/libm the
# moment both got linked into one final executable. No per-symbol
# suppression macro exists for most of these (`_NO_CRT_STDIO_INLINE`
# covers only the stdio ones; wmemcpy/fabsl/frexpl/the libcpmt.lib
# auto-link have no equivalent escape hatch at all), so patching around
# each individual collision was not a real fix.
#
# The project-owned imported libc++ (CRT_USE_IMPORTED_LIBCXX, verified
# working end to end -- static and shared, all three hosts -- earlier
# this same day) already exists specifically to replace this kind of
# "borrow the host's C++ standard library" fallback, matching this
# project's own "own the toolchain, never substitute a host-provided
# runtime" principle already applied everywhere else (see libcrtgfx/
# CMakeLists.txt's own comment: "never make ordinary CRT workflows
# depend on a host libc++ as a hidden substitute"). tools/crt-c++ (which
# Skia's own GN build already calls as its `cc`/`cxx`) already knows how
# to wire up the imported libc++ correctly on its own -- unconditional
# -nostdinc++, CRT_CXX_STANDARD_INCLUDE_FLAGS read directly from the
# environment, _LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS automatically
# applied for the default static-linkage case -- confirmed by exactly
# matching tools/test_libcxx_runtime.py's own already-verified
# CRT_CXX_STANDARD_INCLUDE_FLAGS=f"-isystem{sysroot/'include'/'c++'/'v1'}"
# setup below, instead of reinventing include-path detection here.


def default_gn_args(root, sysroot, target_os, target_arch):
    # .cmd wrappers on Windows: tools/crt-cc/tools/crt-c++ are #!/bin/sh
    # scripts CreateProcess cannot run directly, and (unlike this
    # project's own CMake integration, which launches them via mksh.exe
    # as a compiler-launcher argument) GN's own generic gcc_like_
    # toolchain() invokes `cc`/`cxx` via a plain `subprocess.check_output
    # (..., shell=True)` probe (gn/is_clang.py) and later a bare {{cc}}
    # substitution in each compile rule's own command string -- neither
    # form can pass mksh.exe as a separate leading argument the way CMake's
    # own CMAKE_C_COMPILER_ARG1-avoiding trick does. Confirmed for real:
    # `is_clang.py` failed with `crt-cc --version` exiting 1 (cmd.exe
    # trying to execute a shebang script directly) the first time Skia's
    # GN build was actually routed through tools/crt-cc/tools/crt-c++ at
    # all (see the target_os="linux" comment below for why that took
    # this long to even reach this point). crt-cc.cmd/crt-c++.cmd already
    # exist for exactly this reason (built for tools/crt-libcxx-build.py's
    # own CMake integration) -- reused here rather than inventing a
    # second wrapper mechanism. CRT_MKSH_EXE (which these .cmd files need
    # set, see their own comments) is set in main() below, matching
    # tools/crt-libcxx-build.py's own common_cmake_args()/rootfs handling.
    cmd_suffix = ".cmd" if target_os == "windows" else ""
    crt_cc = root / "tools" / f"crt-cc{cmd_suffix}"
    crt_cxx = root / "tools" / f"crt-c++{cmd_suffix}"
    crt_ar = root / "tools" / ("crt-ar.cmd" if target_os == "windows" else "crt-ar")

    args = {
        "is_official_build": "true",
        "is_component_build": "false",
        "skia_enable_fontmgr_custom_empty": "true",
        "skia_enable_ganesh": "false",
        "skia_enable_graphite": "false",
        "skia_enable_pdf": "false",
        "skia_enable_skottie": "false",
        "skia_enable_svg": "false",
        "skia_use_fonthost_mac": "false",
        "skia_use_perfetto": "false",
        "skia_enable_tools": "false",
        "skia_use_dawn": "false",
        "skia_use_direct3d": "false",
        "skia_use_egl": "false",
        "skia_use_expat": "false",
        "skia_use_gl": "false",
        "skia_use_metal": "false",
        "skia_use_vulkan": "false",
        "skia_use_fontconfig": "false",
        "skia_use_freetype": "false",
        "skia_use_harfbuzz": "false",
        "skia_use_icu": "false",
        "skia_use_libavif": "false",
        "skia_use_libjpeg_turbo_decode": "false",
        "skia_use_libjpeg_turbo_encode": "false",
        "skia_use_libpng_decode": "false",
        "skia_use_libpng_encode": "false",
        "skia_use_libwebp_decode": "false",
        "skia_use_libwebp_encode": "false",
        "skia_use_zlib": "false",
        # skia_use_wuffs (GIF decode support, via a small vendored
        # third_party/externals/wuffs checkout) defaults to true in
        # Skia's own gn/skia.gni, unlike every other optional codec
        # above -- confirmed for real via `ninja -t inputs skia`: with
        # every other codec already off, third_party/externals/wuffs is
        # the ONLY third-party external source this minimal CPU-raster
        # config still pulls in. Disabled for the same reason as every
        # sibling codec flag above: nothing in this project's current
        # Skia integration needs GIF decoding yet, and this keeps the
        # build needing zero third_party/externals content at all.
        "skia_use_wuffs": "false",
        "cc": gn_string(str(crt_cc)),
        "cxx": gn_string(str(crt_cxx)),
        # Skia's GN archives use @response files. Apple ar does not expand
        # those files itself, so keep that detail in the project-owned build
        # wrapper rather than depending on a particular host LLVM install.
        "ar": gn_string(str(crt_ar)),
        "extra_cflags": gn_list(
            [f"-isystem{sysroot / 'include'}"] +
            # SK_BUILD_FOR_UNIX here overrides Skia's own include/private/
            # base/SkFeatures.h auto-detection (an #if !defined(SK_BUILD_
            # FOR_*) guard around the whole block, so defining any one of
            # them up front skips the rest entirely) -- needed on both
            # platforms that use the target_os="linux" GN toolchain-
            # selection trick (see this function's own target_os comment
            # below), since each one's *compiler* still predefines its own
            # real-platform macro regardless of what GN's target_os says.
            # macOS's own real clang defines __APPLE__, which SkFeatures.h
            # would otherwise turn into SK_BUILD_FOR_MAC (its catch-all
            # #else, since TARGET_OS_IPHONE is false) -- already handled
            # here before this comment existed. Windows needs the exact
            # same override for a different reason: clang's mingw target
            # (--target=x86_64-w64-mingw32, see tools/crt-cc's own
            # comment) predefines _WIN32/_WIN64, which SkFeatures.h reads
            # as SK_BUILD_FOR_WIN -- and SK_ALWAYS_INLINE (include/
            # private/base/SkAttributes.h) expands to the bare MSVC
            # keyword `__forceinline` under SK_BUILD_FOR_WIN, which this
            # project's own mingw-target clang invocation (real MSVC
            # compatibility mode, i.e. clang-cl, is never used here)
            # does not recognize at all. Confirmed for real (2026-08-22):
            # `error: unknown type name '__forceinline'` in SkAssert.h,
            # the very first SK_ALWAYS_INLINE use reached. SK_BUILD_FOR_
            # UNIX is the internally consistent choice either way: GN's
            # own target_os="linux" selection already picked Skia's
            # generic POSIX *source files* (e.g. src/ports/SkOSFile_
            # posix.cpp, not the real Win32 SkOSFile_win.cpp) for both
            # platforms, so the preprocessor macro should agree.
            (["-DSK_BUILD_FOR_UNIX"] if target_os in ("macos", "windows") else []) +
            # win32_shim: NOT every Windows-conditional Skia source file
            # is dodged by the target_os="linux"/SK_BUILD_FOR_UNIX trick
            # above -- src/ports/SkOSFile_stdio.cpp's own `#ifdef _WIN32`
            # (a raw preprocessor check, not funneled through Skia's own
            # SkFeatures.h/SK_BUILD_FOR_* abstraction at all) still
            # correctly reflects the real compiler target regardless of
            # what GN's target_os string says, since tools/crt-cc's own
            # --target=x86_64-w64-mingw32 always predefines _WIN32/_WIN64.
            # That branch needs <direct.h>/<io.h> (_mkdir/_get_osfhandle),
            # real MSVC-CRT headers this project's freestanding build has
            # no access to -- confirmed for real (2026-08-22): `fatal
            # error: 'direct.h' file not found` compiling this file, the
            # first time this project's Windows GN/Skia build was retried
            # after the imported libc++ recipe itself finally finished
            # building clean. Same win32_shim/{direct,io,windows}.h shim
            # directory already established for libunwind/libcxx's own
            # matching Windows needs (see tools/crt-libcxx-build.py's own
            # common_cmake_args()) -- reused here rather than duplicated,
            # since it already covers everything this file needs too.
            ([f"-I{root / 'libstdc++' / 'third_party' / 'win32_shim'}"] if target_os == "windows" else [])
        ),
        # -fno-exceptions/-fno-rtti here are Skia's own genuine preference
        # (matches CRTGFX_SKIA's own "no fake Skia headers, no exceptions
        # in the CPU-raster core" scope) -- redundant with, not a
        # replacement for, tools/crt-c++'s own default of the same two
        # flags whenever CRT_CXX_ENABLE_EXCEPTIONS/CRT_CXX_ENABLE_RTTI
        # are left unset (see that file's own comment). No C++ standard-
        # library include path is injected here at all any more -- see
        # this file's own top-of-function comment for why that used to
        # be here and why it was actively wrong; tools/crt-c++ (Skia's
        # own `cc`/`cxx`) already wires up the imported libc++ correctly
        # by itself, reading CRT_CXX_STANDARD_INCLUDE_FLAGS from the
        # environment (set in main() below).
        "extra_cflags_cc": gn_list(["-fno-exceptions", "-fno-rtti"]),
        "extra_ldflags": gn_list([f"-L{sysroot / 'lib'}"]),
    }

    if target_os == "windows":
        # target_os = "win" (NOT used here, deliberately) makes Skia's own
        # gn/BUILDCONFIG.gn call set_default_toolchain("//gn/toolchain:
        # msvc") -- a GN toolchain template (gn/toolchain/BUILD.gn's own
        # msvc_toolchain()) that is hardcoded to real cl.exe (or
        # clang-cl.exe if clang_win is set) and completely IGNORES the
        # top-level cc/cxx args this file sets above, unlike the generic
        # gcc_like_toolchain() template selected for every other target_os
        # value (BUILDCONFIG.gn's own if/else: target_os == "win" ->
        # :msvc, everything else -> :gcc_like, and gcc_like_toolchain()
        # genuinely does `cc = invoker.cc` / `cxx = invoker.cxx`).
        # Confirmed for real (2026-08-21): even after fixing this file's
        # own CRT_CXX_STANDARD_INCLUDE_FLAGS/extra_cflags_cc (see the
        # removed-detect_cxx_standard_include_dirs() comment above), a
        # fully clean rebuild still linked real MSVC UCRT symbols into
        # libskia.a -- traced to `cl : warning D9002: unknown option
        # '-isystem...' ignored` in the build log, meaning real cl.exe
        # was compiling Skia the entire time, never tools/crt-c++ at all.
        # Same fix already established for macOS just below (see that
        # branch's own comment): select "linux" so Skia's own GN build
        # takes the generic POSIX/gcc-like source and toolchain path,
        # while the wrapper compiler (tools/crt-c++, itself already
        # handling the real --target=x86_64-w64-mingw32 selection
        # regardless of what GN's own target_os string says) still emits
        # a genuine Windows binary. This also means src/ports/SkFontHost_
        # win.cpp (real Win32 GDI/DirectWrite code, confirmed compiling
        # before this fix) is no longer selected either -- consistent
        # with skia_enable_fontmgr_custom_empty=true already requesting
        # the empty/stub font manager, not a real platform one.
        args["target_os"] = gn_string("linux")
        if target_arch in ("aarch64", "arm64"):
            args["target_cpu"] = gn_string("arm64")
        elif target_arch in ("x86_64", "amd64", "x64"):
            args["target_cpu"] = gn_string("x64")
    elif target_os == "macos":
        # This is a CRT/PAL build of Skia, not Skia's native Cocoa/CoreGraphics
        # port. Select Skia's generic POSIX source set while the wrapper
        # compiler still emits a macOS binary via CRT_TARGET_OS=macos.
        args["target_os"] = gn_string("linux")
        if target_arch in ("aarch64", "arm64"):
            args["target_cpu"] = gn_string("arm64")
    elif target_os == "linux":
        args["target_os"] = gn_string("linux")
        if target_arch in ("aarch64", "arm64"):
            args["target_cpu"] = gn_string("arm64")

    return args


def write_args_file(path, args, extra_args):
    lines = [f"{key} = {value}" for key, value in sorted(args.items())]
    lines.extend(extra_args)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def copy_tree(src, dst):
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def install_artifacts(source, build_dir, install_prefix):
    include_src = source / "include"
    include_dst = install_prefix / "include"
    lib_dst = install_prefix / "lib"
    lib_dst.mkdir(parents=True, exist_ok=True)
    copy_tree(include_src, include_dst / "include")

    candidates = [
        build_dir / "libskia.a",
        build_dir / "skia.lib",
        build_dir / "obj" / "libskia.a",
    ]
    for candidate in candidates:
        if candidate.exists():
            shutil.copy2(candidate, lib_dst / candidate.name)
            if candidate.name != "libskia.a":
                shutil.copy2(candidate, lib_dst / "libskia.a")
            return
    raise SystemExit(f"Skia library artifact was not found under {build_dir}")


def main():
    parser = argparse.ArgumentParser(description="Build Skia with the CRT wrapper toolchain.")
    parser.add_argument("--root", required=True, help="CRT repository root")
    parser.add_argument("--source", required=True, help="Skia source checkout")
    parser.add_argument("--build-dir", required=True, help="Skia GN output directory")
    parser.add_argument("--install-prefix", required=True, help="Skia install prefix used by libcrtgfx")
    parser.add_argument("--sysroot", required=True, help="CRT sysroot")
    parser.add_argument("--rootfs", default="", help="CRT rootfs (Windows only, for mksh.exe)")
    parser.add_argument("--target-os", required=True, choices=["linux", "macos", "windows"])
    parser.add_argument("--target-arch", default="host")
    parser.add_argument("--gn", default="", help="path to gn; defaults to Skia bin/gn or PATH")
    parser.add_argument("--ninja", default="", help="path to ninja; defaults to PATH")
    parser.add_argument("--gn-arg", action="append", default=[], help="extra raw GN arg line")
    parser.add_argument("--configure-only", action="store_true")
    args = parser.parse_args()
    args.target_arch = normalize_target_arch(args.target_arch)

    root = Path(args.root).resolve()
    source = Path(args.source).resolve()
    build_dir = Path(args.build_dir).resolve()
    install_prefix = Path(args.install_prefix).resolve()
    sysroot = Path(args.sysroot).resolve()
    gn_exe_suffix = ".exe" if args.target_os == "windows" or os.name == "nt" else ""
    gn = args.gn or str(source / "bin" / ("gn" + gn_exe_suffix))
    if not Path(gn).exists():
        # Bare "gn" (no .exe) never resolves via Path.exists() on Windows
        # even when bin/gn.exe genuinely exists -- confirmed for real: this
        # silently fell through to `shutil.which("gn")` (which also never
        # finds it, since bin/ is never on PATH) and then to the literal
        # string "gn" itself, only failing much later with a confusing
        # FileNotFoundError from CreateProcess. Bootstraps a fresh gn the
        # same way Skia's own real developers do -- bin/fetch-gn downloads
        # a prebuilt binary pinned to a specific git_revision hardcoded in
        # that script itself, so this needs no pinning of its own here.
        gn = shutil.which("gn") or gn
        if not Path(gn).exists():
            fetch_gn = source / "bin" / "fetch-gn"
            if fetch_gn.exists():
                run([sys.executable, str(fetch_gn)], cwd=source)
                gn = str(source / "bin" / ("gn" + gn_exe_suffix))
    ninja = args.ninja or shutil.which("ninja") or "ninja"

    env = os.environ.copy()
    env["CRT_SYSROOT"] = str(sysroot)
    env["CRT_TARGET_OS"] = args.target_os
    pin_gn_script_executable(source, sys.executable)
    if args.target_os == "windows":
        # See pin_gcc_toolchain_python()'s own comment for the full story --
        # a real, separate "python3" gap from script_executable's own,
        # only ever hit once actual compilation starts (not during `gn
        # gen`), so patch it here too rather than assuming the fix above
        # already covers it.
        pin_gcc_toolchain_python(source, sys.executable)
        # crt-ar.cmd needs the same interpreter CMake used to launch this
        # driver; relying on the optional py launcher would make a configured
        # Python installation look like a missing archiver.
        env["CRT_HOST_PYTHON"] = sys.executable
        # crt-cc.cmd/crt-c++.cmd (see default_gn_args()'s own comment on
        # why GN needs these .cmd wrappers rather than the bare scripts)
        # need CRT_MKSH_EXE set to the *rootfs* copy of mksh.exe, not the
        # sysroot's -- only the rootfs has toybox's applet aliases
        # actually installed (system/bin/printf and friends), which
        # tools/crt-cc/tools/crt-c++ themselves call. Exactly matches
        # tools/crt-libcxx-build.py's own common_cmake_args() handling.
        if not args.rootfs:
            raise SystemExit("--rootfs is required on Windows (its mksh.exe launches crt-cc.cmd/crt-c++.cmd)")
        mksh = Path(args.rootfs).resolve() / "system" / "bin" / "mksh.exe"
        if not mksh.is_file():
            raise SystemExit(f"CRT mksh is missing from the rootfs: {mksh} (build the \"rootfs\" target first)")
        env["CRT_MKSH_EXE"] = str(mksh)
        # Pre-seed CRT_ROOTFS (and the matching POSIX-shaped PATH) before
        # mksh.exe ever starts, exactly matching tools/crt-libcxx-build.py's
        # own common_cmake_args() handling. This is not just about PATH
        # resolution: libc/src/env.c's own __crt_rootfs_bootstrap() runs
        # unconditionally at process startup and, whenever CRT_ROOTFS is
        # *not already set*, auto-detects it from argv[0] (mksh.exe lives
        # under .../rootfs/system/bin/) and then unconditionally
        # `chdir("/")`s -- discarding whatever real cwd ninja launched it
        # with. Confirmed for real (2026-08-22) by isolating to a two-line
        # repro run directly via mksh.exe: `pwd` reported "/" even though
        # mksh.exe was launched with cwd already set to the Skia GN output
        # directory, and every ninja-driven compile failed with "no such
        # file or directory" on its own GN-relative source path (e.g.
        # "../../modules/skcms/src/skcms_TransformHsw.cc") as a direct
        # result. Setting CRT_ROOTFS here ahead of time makes
        # __crt_rootfs_bootstrap() return immediately (its own first check
        # is `if (getenv("CRT_ROOTFS") != 0) return;`), so mksh.exe keeps
        # the real, correct cwd ninja gave it.
        env["CRT_ROOTFS"] = str(Path(args.rootfs).resolve())
        # A flat POSIX-style replacement, matching tools/crt-libcxx-build.
        # py's own common_cmake_args() exactly. This project's own
        # Windows mksh build keeps MKSH_PATHSEPC as ':' (a deliberate
        # choice -- see shell/toybox/PATCHES.md's own MKSH_CRT_WINPATH
        # writeup), so PATH has to stay pure POSIX/rootfs-relative for
        # mksh's own internal command search to work *at all*: a mixed
        # or real-Windows-style entry doesn't just fail to resolve, it
        # actively breaks mksh's ':'-delimited PATH parsing (a real,
        # backslash-form Windows directory contains its own ':' after
        # the drive letter, so mksh reads it as two garbage entries).
        # Confirmed for real (2026-08-22): even a real-Windows-format
        # entry pointing *directly* at the rootfs bin dir containing
        # printf.exe still failed "printf: inaccessible or not found"
        # from inside mksh.
        #
        # gn.exe's own separate, real-Windows-PATH-based "python3" lookup
        # (needed for the "gn gen" call below) no longer depends on PATH
        # content at all -- see pin_gn_script_executable()'s own comment,
        # called earlier in main() before this env is even built.
        env["PATH"] = "/system/bin:/bin:/usr/bin"
        # tools/crt-cc/tools/crt-c++ fall back to a bare "clang"/"clang++"
        # (resolved via mksh's own $PATH search) whenever CRT_HOST_CC/
        # CRT_HOST_CXX are unset -- but mksh's own PATH inside these
        # scripts is POSIX-rooted (/system/bin:/bin:/usr/bin, set inside
        # the scripts themselves for other reasons), which has no real
        # host compiler on it at all. Confirmed for real: `gn gen` failed
        # with `crt-cc: clang: inaccessible or not found` (exit 127) the
        # first time GN's own is_clang.py actually reached crt-cc.cmd at
        # all -- the exact same gap tools/test_libcxx_runtime.py's own
        # comment already documents and tools/crt-libcxx-build.py's own
        # common_cmake_args() already works around, resolved via
        # shutil.which() (using *this* process's own, unrestricted PATH)
        # and exported as a forward-slash path (mksh's own script-loading
        # and exec() path lookup both misread a bare backslash path as a
        # command name to search PATH for, not a file to open). That
        # forward-slash form alone is still not enough, though: mksh
        # cannot exec *any* path containing a space regardless of slash
        # direction (confirmed for real -- see windows_short_path()'s own
        # comment), and a stock Windows LLVM install always lands under
        # "C:\Program Files\LLVM\...". Convert to the 8.3 short-path form
        # first, exactly matching tools/crt-port-build.py's own
        # find_windows_host_tool()/native_windows_tool_command() handling
        # of this identical problem.
        host_cc = shutil.which("clang")
        host_cxx = shutil.which("clang++")
        if host_cc:
            env["CRT_HOST_CC"] = windows_short_path(host_cc).replace("\\", "/")
        if host_cxx:
            env["CRT_HOST_CXX"] = windows_short_path(host_cxx).replace("\\", "/")
        # tools/crt-ar's own fallback (shutil.which("llvm-ar") or
        # shutil.which("ar")) hits the identical PATH-format problem as
        # CRT_HOST_CC/CRT_HOST_CXX just above -- confirmed for real
        # (2026-08-22): `crt-ar: neither CRT_HOST_AR, llvm-ar, nor ar was
        # found`, the first time tool("alink")'s own `... && crt-ar.cmd
        # rcs ...` half of its compound command actually ran (the python3
        # half just before it was fixed separately, see
        # pin_gcc_toolchain_python()'s own comment). Unlike crt-cc.cmd/
        # crt-c++.cmd, crt-ar.cmd never routes through mksh.exe at all --
        # it is a plain native-Windows .cmd wrapper invoking `python
        # tools/crt-ar` directly (confirmed by reading it) -- so no
        # windows_short_path()/forward-slash treatment is needed here,
        # only a real value for CRT_HOST_AR itself, resolved the same way
        # (via *this* process's own, unrestricted PATH) before PATH gets
        # overwritten to its POSIX-only form just above.
        host_ar = shutil.which("llvm-ar")
        if host_ar:
            env["CRT_HOST_AR"] = host_ar
    if args.target_arch != "host":
        env["CRT_TARGET_ARCH"] = args.target_arch

    # The project-owned imported libc++ (CRT_USE_IMPORTED_LIBCXX) is a
    # hard requirement, matching libcrtgfx/CMakeLists.txt's own already-
    # documented policy ("never make ordinary CRT workflows depend on a
    # host libc++ as a hidden substitute") -- see this file's own
    # removed-detect_cxx_standard_include_dirs() comment above for the
    # real failure this replaces. Exactly matches tools/test_libcxx_
    # runtime.py's own already-verified setup, not reinvented here.
    libcxx_headers = sysroot / "include" / "c++" / "v1"
    if not libcxx_headers.is_dir():
        raise SystemExit(
            f"CRT_USE_IMPORTED_LIBCXX headers not found: {libcxx_headers}\n"
            "Build libc++ first (crt-libcxx-build, or reconfigure with "
            "-DCRT_USE_IMPORTED_LIBCXX=ON and build crt-libcxx-sysroot) -- "
            "Skia's own C++ code needs a real project-owned <vector>/"
            "<string>/... implementation, never a host libc++ substitute."
        )
    env["CRT_CXX_STANDARD_INCLUDE_FLAGS"] = f"-isystem{libcxx_headers}"

    gn_args = default_gn_args(root, sysroot, args.target_os, args.target_arch)
    args_gn = build_dir / "args.gn"
    write_args_file(args_gn, gn_args, args.gn_arg)
    run([gn, "gen", str(build_dir)], cwd=source, env=env)
    if args.configure_only:
        return

    run([ninja, "-C", str(build_dir), "skia"], cwd=source, env=env)
    install_artifacts(source, build_dir, install_prefix)
    print(f"Skia installed into {install_prefix}")


if __name__ == "__main__":
    main()
