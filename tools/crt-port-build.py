#!/usr/bin/env python3
import argparse
import ctypes
import json
import os
import platform
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def load_recipes(recipe_dir):
    recipes = {}
    for path in sorted(Path(recipe_dir).glob("*.json")):
        with open(path, "r", encoding="utf-8") as f:
            recipe = json.load(f)
        recipes[recipe["name"]] = recipe
    return recipes


def progress(message):
    print(f"[port] {message}", flush=True)


def detect_target_arch(target_arch_arg):
    """Resolves the target architecture used for GNU-triple substitution
    (the @CRT_MINGW_TRIPLE@ token recipes can use in configure_args/
    make_subdir -- see mingw_triple_for_arch()) to "aarch64" or "x86_64".
    Priority: explicit --target-arch CLI arg, then CRT_TARGET_ARCH env var
    (matching the same override tools/crt-cc respects), then
    platform.machine() -- a real Win32 API query via Python's own stdlib,
    reliable regardless of which uname/shell this script happens to run
    under (unlike a shell-based `uname` check: tools/crt-cc's own C-side
    equivalent has to combine `uname -m` and `uname -s` specifically
    because this project's toybox uname and MSYS/Git-Bash's uname report
    the architecture in different fields)."""
    arch = target_arch_arg or os.environ.get("CRT_TARGET_ARCH") or platform.machine()
    arch = arch.lower()
    if arch in ("aarch64", "arm64"):
        return "aarch64"
    if arch in ("x86_64", "amd64", "x64"):
        return "x86_64"
    raise SystemExit(
        f"crt-port-build.py: unrecognized target architecture {arch!r} "
        "(expected aarch64/arm64 or x86_64/amd64/x64; pass --target-arch or set CRT_TARGET_ARCH to override)"
    )


def mingw_triple_for_arch(target_arch):
    return f"{target_arch}-w64-mingw32"


def alias_unix_static_libs_for_windows_link(port_prefix, target_os):
    """Windows-only: `-lfoo` on this project's toolchain (clang -fuse-ld=lld,
    a MinGW-triple driver but a COFF/lld-link backend) resolves to searching
    for a file literally named `foo.lib`, matching lld-link's own MSVC-
    compatible argument convention -- it does not also try the Unix
    `libfoo.a` name the way a native GNU `ld` would. This project's own
    CMake-built libraries are already named the `foo.lib` way (see e.g.
    libc/CMakeLists.txt's `OUTPUT_NAME c`), but a third-party port built via
    its own autoconf/make (zlib, libpng, ...) instead installs the Unix-
    conventional `libfoo.a`, which a *later* port's `-lfoo` can then never
    find. Observed concretely: libpng's `configure` failing its own
    `AC_CHECK_LIB(z, zlibVersion)` probe with `lld-link: error: could not
    open 'z.lib'`, even though zlib's own install stamp -- and
    `PORT_PREFIX/lib/libz.a` -- were already present.

    Rather than patching every third-party port's own build system to use
    an unfamiliar output name, alias every `libfoo.a` this port just
    installed with a copy named `foo.lib` alongside it, so later ports'
    plain `-lfoo` references resolve the same way this project's own
    libraries already do. A copy, not a symlink: Windows symlinks need a
    privilege an ordinary dev environment may not have, and these archives
    are small. Skipped if `foo.lib` already exists (e.g. a port that
    already produces a native COFF archive on its own) to avoid clobbering
    something intentionally different."""
    if target_os != "windows":
        return
    lib_dir = port_prefix / "lib"
    if not lib_dir.is_dir():
        return
    for unix_lib in sorted(lib_dir.glob("lib*.a")):
        coff_name = unix_lib.name[len("lib"):-len(".a")] + ".lib"
        coff_lib = lib_dir / coff_name
        if coff_lib.exists():
            continue
        progress(f"alias {unix_lib.name} -> {coff_name} (for plain -l lookups on this Windows toolchain)")
        shutil.copy2(unix_lib, coff_lib)


def run(args, cwd, env, label=None):
    if label:
        progress(f"start {label}")
    print("+", " ".join(str(a) for a in args), flush=True)
    start = time.monotonic()
    subprocess.run(args, cwd=cwd, env=env, check=True)
    if label:
        elapsed = time.monotonic() - start
        progress(f"done {label} ({elapsed:.1f}s)")


def run_checked_output(args, cwd, env, expect_stdout=None, label=None):
    if label:
        progress(f"start {label}")
    print("+", " ".join(str(a) for a in args), flush=True)
    start = time.monotonic()
    # stdin=DEVNULL: found for real running a port's own test binary
    # (curl's http-roundtrip-shared, on Windows) -- without this, the
    # test binary inherits whatever stdin this script itself has, which
    # differs by how deep this script's own invocation chain runs
    # (cmake -E env -> cmd.exe /C -> python.exe, as CMake's own custom
    # target driver does, vs. a plain direct subprocess call) and can
    # end up being a real, live, never-EOF-signaling handle instead of
    # a closed/redirected one. A test binary should never be able to
    # block on stdin at all -- it isn't given any input on purpose --
    # so explicitly redirecting it from the null device removes the
    # ambiguity outright, regardless of which specific library call
    # ends up reading from fd 0.
    completed = subprocess.run(
        args, cwd=cwd, env=env, text=True, capture_output=True, check=False,
        stdin=subprocess.DEVNULL)
    if completed.stdout:
        print(completed.stdout, end="" if completed.stdout.endswith("\n") else "\n")
    if completed.stderr:
        print(completed.stderr, end="" if completed.stderr.endswith("\n") else "\n")
    if completed.returncode != 0:
        raise subprocess.CalledProcessError(completed.returncode, args, completed.stdout, completed.stderr)
    if expect_stdout and expect_stdout not in completed.stdout:
        raise SystemExit(f"{label or args[0]}: expected stdout fragment not found: {expect_stdout!r}")
    if label:
        elapsed = time.monotonic() - start
        progress(f"done {label} ({elapsed:.1f}s)")


def is_native_windows_configure(target_os):
    return target_os == "windows" and os.name == "nt"


def path_for_msys_shell(path):
    value = str(path).replace("\\", "/")
    if len(value) >= 2 and value[1] == ":":
        drive = value[0].lower()
        rest = value[2:]
        if rest.startswith("/"):
            rest = rest[1:]
        return f"/{drive}/{rest}"
    return value


def path_for_crt_shell(path):
    return str(path).replace("\\", "/")


def windows_short_path(path):
    if os.name != "nt":
        return str(path)
    value = str(path)
    GetShortPathNameW = ctypes.windll.kernel32.GetShortPathNameW
    needed = GetShortPathNameW(value, None, 0)
    if needed == 0:
        return value
    buffer = ctypes.create_unicode_buffer(needed)
    if GetShortPathNameW(value, buffer, needed) == 0:
        return value
    return buffer.value


def read_cmake_cache(cache_path):
    values = {}
    if not cache_path.exists():
        return values
    for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("//") or line.startswith("#") or "=" not in line:
            continue
        key_type, value = line.split("=", 1)
        key = key_type.split(":", 1)[0]
        values[key] = value
    return values


def find_windows_host_tool(names):
    for name in names:
        found = shutil.which(name)
        if found:
            return path_for_crt_shell(windows_short_path(found))
    candidates = []
    program_files = os.environ.get("ProgramFiles")
    program_files_x86 = os.environ.get("ProgramFiles(x86)")
    for root in (program_files, program_files_x86):
        if root:
            for name in names:
                candidates.append(Path(root) / "LLVM" / "bin" / name)
    for candidate in candidates:
        if candidate.exists():
            return path_for_crt_shell(windows_short_path(candidate))
    return None


def find_llvm_tool(name):
    """Resolve an llvm-* tool (llvm-ar/llvm-ranlib/llvm-strip) that a plain
    shutil.which(name) can genuinely miss even when a real LLVM install is
    right there on this host. Confirmed for real (2026-08-24, building
    libffi on a real Ubuntu/WSL host while preparing this project's own
    Wayland port): Debian/Ubuntu's llvm-<N> package installs every LLVM
    tool, unlinked, under /usr/lib/llvm-<N>/bin/ and exposes only clang/
    clang++/clang-cl/... on plain PATH (via update-alternatives-managed
    /usr/bin/clang) -- llvm-ar/llvm-ranlib/llvm-strip themselves are never
    symlinked onto PATH at all, so shutil.which("llvm-ranlib") returns
    None and this function's own caller used to fall back to the *host's*
    real GNU binutils ranlib instead. That matters for a real, non-
    cosmetic reason: GNU ranlib on a modern glibc host performs its own
    IFUNC-relink heuristic against archive members, and segfaulted for
    real relinking this project's own sysroot/lib/libm.so (a real ELF this
    project's own build produced, not the host's) while ranlib'ing
    libffi.a -- `ranlib: Relink '.../sysroot/lib/libm.so' with '/usr/lib/
    x86_64-linux-gnu/libm.so.6' for IFUNC symbol 'ceil'` immediately
    followed by `Segmentation fault`. LLVM's own llvm-ranlib has no such
    IFUNC-relink behavior at all (confirmed: switching to it, same host,
    same libffi.a, ranlib completes with no relink attempt and no crash).

    Resolved by walking backward from wherever the *compiler* this project
    actually uses is really found: shutil.which("clang") followed by
    os.path.realpath() (needed since /usr/bin/clang is itself normally an
    update-alternatives symlink, not the real file) lands in exactly the
    directory Debian/Ubuntu's own LLVM package installs every other LLVM
    tool into -- confirmed directly for real on this same host:
    `realpath $(which clang)` and llvm-ranlib/llvm-ar both resolve under
    the identical /usr/lib/llvm-21/bin/. A plain shutil.which(name) is
    still tried first (respects any host where llvm-ranlib genuinely is on
    PATH, e.g. Homebrew's own layout on macOS, or a from-source LLVM build
    someone put on PATH themselves), so this is purely an additional
    fallback, never a behavior change for a host where the simple case
    already worked."""
    found = shutil.which(name)
    if found:
        return found
    clang = shutil.which("clang")
    if not clang:
        return None
    candidate = Path(os.path.realpath(clang)).parent / name
    if candidate.is_file():
        return str(candidate)
    return None


def find_host_make(target_os):
    names = ("make", "gmake")
    if target_os == "windows":
        names = ("make.exe", "mingw32-make.exe", "make")
    for name in names:
        found = shutil.which(name)
        if found:
            return path_for_crt_shell(windows_short_path(found)) if target_os == "windows" else found
    return None


def native_windows_tool_command(root, root_env, shell, target_os, path):
    """Wrap a real, native Windows host tool (llvm-ar.exe, ld.lld.exe, ...)
    as "<mksh> tools/crt-native-tool <tool-path>" rather than the literal
    string "CRT_SPAWN_NATIVE_WINDOWS=1 <tool-path>" this used to be: the
    latter relies on the *calling* shell recognizing that leading "VAR=val"
    as an environment-assignment prefix whenever $AR/$RANLIB/$STRIP/$LD is
    expanded unquoted, which POSIX shells only do for literal, parsed-at-
    parse-time source text -- never for a variable's word-split expansion
    at runtime (e.g. libtool's own `` `$LD -v` `` "is this GNU ld" probe
    inside `configure`, which silently misdetected `with_gnu_ld=no` this
    way). See tools/crt-native-tool's own header comment for the full
    story. Mirrors exactly how CC/CXX are already set two cases above,
    since that "<mksh> <script>" pattern is already proven reliable."""
    tool_arg = path_for_crt_shell(windows_short_path(path))
    if is_native_windows_configure(target_os):
        return f"/system/bin/mksh {root_env}/tools/crt-native-tool {tool_arg}"
    return f"{shell} {root / 'tools' / 'crt-native-tool'} {tool_arg}"


def rootfs_mksh_path(preset_build_dir, target_os):
    name = "mksh.exe" if target_os == "windows" and os.name == "nt" else "mksh"
    return preset_build_dir / "rootfs" / "system" / "bin" / name


def find_posix_shell(env):
    for name in ("CRT_PORT_SHELL", "CONFIG_SHELL"):
        value = env.get(name)
        if value:
            return value

    for name in ("bash", "sh"):
        value = shutil.which(name)
        if value:
            return value

    raise SystemExit(
        "configure recipes on native Windows require a POSIX shell. "
        "Install MSYS2 or Git Bash, put bash.exe/sh.exe in PATH, or set "
        "CRT_PORT_SHELL to the shell executable path."
    )


def join_flags(*parts):
    return " ".join(part for part in parts if part).strip()


def find_source(source_root, source_dir):
    exact = source_root / source_dir
    if exact.exists():
        return exact

    pattern = source_dir.split("-", 1)[0] + "-*"
    candidates = sorted(source_root.glob(pattern), reverse=True)
    if not candidates:
        raise SystemExit(f"source not found: {exact}")
    return candidates[0]


def remove_tree(path):
    def retry_with_write_permission(func, failing_path, _exc):
        try:
            os.chmod(failing_path, 0o700)
        except OSError:
            pass
        func(failing_path)

    # shutil.rmtree()'s onexc= callback (func, path, exc_instance) was
    # only added in Python 3.12; older interpreters (this project's
    # Windows CI/dev hosts have run both 3.11 and 3.14 at different
    # times -- see HISTORY.md) only support the older onerror= callback
    # (func, path, exc_info triple). Both callback shapes are
    # source-compatible with retry_with_write_permission's own body
    # (it only uses failing_path), so just pick whichever kwarg this
    # interpreter's shutil.rmtree() actually accepts.
    use_onexc = sys.version_info >= (3, 12)

    for attempt in range(5):
        try:
            if use_onexc:
                shutil.rmtree(path, onexc=retry_with_write_permission)
            else:
                shutil.rmtree(path, onerror=retry_with_write_permission)
            return
        except OSError:
            if attempt == 4 or not path.exists():
                raise
            time.sleep(0.1 * (attempt + 1))


def copy_source(src, dst):
    if dst.exists():
        remove_tree(dst)
    ignore = shutil.ignore_patterns(".git", "autom4te.cache", "*.o", "*.a", "*.so", "*.dylib", "*.dll")
    shutil.copytree(src, dst, ignore=ignore)


def make_env(root, preset_build_dir, work_build_dir, sysroot, port_prefix, target_os, mingw_triple, use_crt_shell=False):
    env = os.environ.copy()
    if target_os == "windows":
        # tools/crt-cc/tools/crt-c++ read $CRT_TARGET_ARCH to pick
        # --target=<arch>-w64-mingw32; if unset, they fall back to
        # auto-detecting the *host's* arch via `uname`. That fallback is
        # only correct when the host and requested target arch happen to
        # match -- it silently builds the wrong architecture during a
        # same-OS cross-arch build (e.g. this project's own
        # -DCRT_TARGET_ARCH=x86_64 CMake preset, or --target-arch x86_64
        # passed straight to this script, from an aarch64 host). mingw_triple
        # is already resolved once from the same --target-arch/
        # CRT_TARGET_ARCH/platform.machine() priority chain (see
        # detect_target_arch()/mingw_triple_for_arch()), so forward that
        # same answer here instead of letting crt-cc re-derive its own,
        # potentially different one via `uname`.
        env["CRT_TARGET_ARCH"] = mingw_triple.removesuffix("-w64-mingw32")
    use_msys_paths = is_native_windows_configure(target_os) and not use_crt_shell
    path_for_shell = path_for_crt_shell if (is_native_windows_configure(target_os) and use_crt_shell) else path_for_msys_shell
    root_env = path_for_shell(root) if is_native_windows_configure(target_os) else str(root)
    build_dir_env = path_for_shell(work_build_dir) if is_native_windows_configure(target_os) else str(work_build_dir)
    sysroot_env = path_for_shell(sysroot) if is_native_windows_configure(target_os) else str(sysroot)
    port_prefix_env = path_for_shell(port_prefix) if is_native_windows_configure(target_os) else str(port_prefix)
    tools_dir_env = path_for_shell(root / "tools") if is_native_windows_configure(target_os) else str(root / "tools")

    env["CRT_SYSROOT"] = sysroot_env
    env["CRT_TARGET_OS"] = target_os
    if use_crt_shell:
        rootfs = (preset_build_dir / "rootfs").resolve()
        shell = rootfs_mksh_path(preset_build_dir, target_os)
        if is_native_windows_configure(target_os):
            env["CRT_ROOTFS"] = str(rootfs)
            env["CC"] = f"/system/bin/mksh {root_env}/tools/crt-cc"
            env["CXX"] = f"/system/bin/mksh {root_env}/tools/crt-c++"
            env["PATH"] = "/system/bin:/bin:/usr/bin"
            # TMPDIR: a real bug, confirmed for real (2026-08-31, first
            # ffmpeg.json Windows/mksh configure attempt) -- os.environ's
            # own TEMP/TMP (Windows-style, backslash-separated, e.g.
            # "C:\Users\Lee\AppData\Local\Temp") is otherwise inherited
            # into this mksh-run environment untouched by the os.environ.
            # copy() above, and FFmpeg's own configure (like any POSIX
            # shell script) reads $TMPDIR unquoted/uninterpreted -- mksh's
            # ordinary backslash-escapes-next-char word-expansion silently
            # eats every backslash, producing a mangled, nonexistent path
            # ("C:UsersLeeAppDataLocalTemp") that configure's own sanity
            # test then fails to create files under ("Unable to create and
            # execute files in ...", "Sanity test failed"). Every other
            # path already forwarded into this same env (CRT_SYSROOT,
            # root_env, build_dir_env, ...) already goes through path_for_
            # crt_shell()'s backslash-to-forward-slash conversion for
            # exactly this reason; TMPDIR was simply never one of them.
            env["TMPDIR"] = path_for_crt_shell(
                Path(os.environ.get("TEMP") or os.environ.get("TMP") or tempfile.gettempdir()))
        else:
            rootfs_path = os.pathsep.join(
                str(rootfs / entry) for entry in ("system/bin", "bin", "usr/bin"))
            env["CRT_ROOTFS"] = str(rootfs)
            env["CC"] = f"{shell} {root / 'tools' / 'crt-cc'}"
            env["CXX"] = f"{shell} {root / 'tools' / 'crt-c++'}"
            env["PATH"] = f"{rootfs_path}{os.pathsep}{env.get('PATH', '')}"
        cache = read_cmake_cache(preset_build_dir / "CMakeCache.txt")
        kernel32 = cache.get("CRT_WINDOWS_KERNEL32_LIB")
        if kernel32:
            env["CRT_WINDOWS_SDK_LIBPATH"] = path_for_crt_shell(windows_short_path(Path(kernel32).parent))
        if target_os == "windows":
            env["CRT_HOST_CC"] = env.get("CRT_HOST_CC") or find_windows_host_tool(("clang.exe", "clang"))
            env["CRT_HOST_CXX"] = env.get("CRT_HOST_CXX") or find_windows_host_tool(("clang++.exe", "clang++"))
            # crt-cc links via `-fuse-ld=lld` (see tools/crt-cc), so ld.lld is
            # already this project's real linker backend -- just not under a
            # name/location any autoconf-generated configure script would
            # find on its own. Native-Windows configure runs with PATH
            # hard-restricted to this project's own rootfs (unlike macOS/
            # Linux, which append the host PATH -- see the `else` branch
            # above -- and so already find a real system `ld` there), so
            # without this, libtool's AC_PROG_LD ("checking for non-GNU
            # ld") search comes up empty and configure aborts outright
            # ("no acceptable ld found in $PATH"). Pre-setting $LD skips
            # that PATH search entirely (autoconf/libtool only search PATH
            # when $LD isn't already set).
            found_ld = env.get("LD") or find_windows_host_tool(("ld.lld.exe", "ld.lld"))
            if found_ld:
                env["LD"] = found_ld
            # Same reasoning as $LD above, for Libtool's MinGW/Cygwin
            # shared-library path specifically: it needs dlltool (builds
            # the .dll.a import library from a DEF file) and objdump
            # (several of its own internal probes shell out to it, e.g.
            # deplibs_check_method) under those literal GNU-binutils names.
            # This project's LLVM install ships equivalents under the
            # llvm- prefix instead (llvm-dlltool/llvm-objdump); point
            # DLLTOOL/OBJDUMP at those so libtool's own `checking for
            # dlltool`/`checking for objdump` probes (which, like ld,
            # only search PATH when the var isn't already set) succeed.
            found_dlltool = env.get("DLLTOOL") or find_windows_host_tool(("llvm-dlltool.exe", "llvm-dlltool"))
            if found_dlltool:
                env["DLLTOOL"] = found_dlltool
            found_objdump = env.get("OBJDUMP") or find_windows_host_tool(("llvm-objdump.exe", "llvm-objdump"))
            if found_objdump:
                env["OBJDUMP"] = found_objdump
            # Same reasoning as $LD/$DLLTOOL/$OBJDUMP above, for nm: this
            # project never wraps/aliases a bare "nm" into the rootfs
            # (only AR/RANLIB/STRIP/LD/DLLTOOL/OBJDUMP are), so without
            # this, libtool's own AC_PATH_TOOL-style NM search comes up
            # empty and silently falls back to the literal, unresolved
            # string "nm" -- confirmed for real via config.log
            # (`NM='nm'`). That alone doesn't fail configure outright
            # (unlike $LD), but it does silently break libtool's own
            # "checking command to parse nm output" self-test (its
            # `$NM conftest.o | $lt_cv_sys_global_symbol_pipe` probe
            # never runs a real nm, so `lt_cv_sys_global_symbol_pipe`
            # stays empty) -- which resurfaces much later, at actual
            # link time, for any library whose cygwin*/mingw*
            # `export_symbols_cmds` needs a real symbol list (`$NM
            # $libobjs $convenience | $global_symbol_pipe | $SED ...`):
            # an empty $global_symbol_pipe leaves two pipe characters
            # back to back with nothing between them, a bare shell
            # syntax error ("unexpected '|'"), hit for real building
            # libpng's own libpng16.la. Pre-setting $NM skips the whole
            # broken PATH search the same way $LD already does.
            found_nm = env.get("NM") or find_windows_host_tool(("llvm-nm.exe", "llvm-nm"))
            if found_nm:
                env["NM"] = found_nm
            # Same reasoning as $LD/$DLLTOOL/$OBJDUMP/$NM above, for the
            # Windows resource compiler: a mingw32-hosted `configure` (any
            # recipe building a Windows DLL with a version-info .rc file,
            # e.g. xz/liblzma's liblzma_w32res.rc) leaves $RC unset, since
            # this project never wraps/aliases anything under that name
            # either. Unlike $LD, an empty/unset $RC doesn't fail configure
            # itself, but it does leave libtool's generated Makefile rule
            # trying to run its own --mode=compile machinery with no real
            # tool behind it, which garbles the invocation into something
            # libtool's own resource-file argument parser rejects outright
            # ("unrecognised option: '-DHAVE_CONFIG_H'") -- confirmed for
            # real building xz/liblzma's shared library. This project's
            # LLVM install ships a real, RC.EXE-compatible resource
            # compiler under llvm-rc; point $RC at it the same way
            # $NM/$DLLTOOL/$OBJDUMP already point at their own llvm-
            # prefixed equivalents.
            found_rc = env.get("RC") or find_windows_host_tool(("llvm-rc.exe", "llvm-rc"))
            if found_rc:
                env["RC"] = found_rc
            # Generalized from the libpng-era investigation (see
            # porting/recipes/libpng.json's own notes): libtool's
            # deplibs_check_method for cygwin*/mingw* hosts is
            # `file_magic file format (pei*-i386(.*architecture: i386)?
            # |pe-arm-wince|pe-x86-64|pe-aarch64)`, matched against
            # `$OBJDUMP -f`'s own output -- but LLVM's llvm-objdump
            # reports `file format coff-x86-64`/`coff-aarch64` for this
            # project's own real .dll/.so files, never GNU objdump's
            # `pe-x86-64` spelling the hardcoded regex expects. This is
            # a fixed, permanent fact about this toolchain (not
            # something a working `file`/objdump install on this host
            # could ever satisfy), true for every Windows configure-
            # based recipe that happens to go through GNU Libtool, not
            # just the one that first exposed it -- so it belongs here,
            # not repeated per-recipe. Autoconf's `${VAR+set}` cache-
            # variable idiom (confirmed by reading a real generated
            # `configure`'s own logic) honors this pre-set environment
            # variable and skips the broken detection entirely, the same
            # mechanism $LD/$DLLTOOL/$OBJDUMP already rely on above.
            # Harmless no-op for any recipe whose configure script
            # doesn't use GNU Libtool at all.
            env.setdefault("lt_cv_deplibs_check_method", "pass_all")
            # libtool's build-to-host file/path name conversion is a
            # *separate* axis from the $host mingw* classification above
            # -- it does not affect whether shared libraries/import libs/
            # -DDLL_EXPORT get built at all, only how build-side path
            # strings get rewritten before landing in a `.libs/lt-*.c`
            # cwrapper's LIB_PATH_VALUE/EXE_PATH_VALUE (and a few other
            # wrapper-generation spots). configure's own logic:
            #   case $host in *-*-mingw* )
            #     case $build in *-*-mingw* | *-*-windows* ) # actually msys
            #       lt_cv_to_host_file_cmd=func_convert_file_msys_to_w32
            # i.e. autoconf's authors used "$build *also* looks like
            # mingw/windows" purely as a historical proxy for "configure
            # is running inside a real MSYS2 shell" -- true for every
            # toolchain they anticipated, but not for this project's own
            # from-scratch mksh/toybox PAL, which genuinely is a native
            # $build with no MSYS/Cygwin userland underneath it at all.
            # func_convert_file_msys_to_w32's actual implementation
            # shells out to a literal `cmd //c echo ...` to exploit real
            # MSYS's automatic POSIX-argv-to-Windows-path translation --
            # a real Windows cmd.exe this project's own rootfs $PATH
            # (deliberately scoped to just its own sysroot bin dirs)
            # cannot reach, so the conversion always fails ("Could not
            # determine host file/path name"). libtool then falls back to
            # its own documented "deliberately simplistic" recovery: a
            # blind `s/:/;/g` on the *original*, already-host-native
            # string -- which corrupts every path this project uses
            # (`C:/Users/...`), since the drive-letter colon isn't a path-
            # list separator the way a real POSIX build's colons would
            # be. Confirmed directly: a real generated `.libs/lt-*.c`
            # wrapper's LIB_PATH_VALUE ended up as `"C;/Users/..."`.
            # Fixed the same standard way as lt_cv_deplibs_check_method
            # just above: both lt_cv_to_host_file_cmd and its sibling
            # lt_cv_to_tool_file_cmd are autoconf cache variables (the
            # `${VAR+y}` idiom skips detection when pre-set), and
            # func_convert_file_noop -- libtool's own built-in "paths are
            # already in host format, nothing to convert" case (the exact
            # value real non-mingw/cygwin hosts already get in the "else"
            # branch of that same case statement) -- is precisely correct
            # here, since this project's paths never were MSYS/POSIX-
            # style to begin with. (to_host_path_cmd has no cache variable
            # of its own; it's derived at runtime from to_host_file_cmd by
            # libtool's func_init_to_host_path_cmd, so fixing the file
            # variant fixes the path variant too.) Harmless no-op for any
            # recipe whose configure script doesn't use GNU Libtool.
            env.setdefault("lt_cv_to_host_file_cmd", "func_convert_file_noop")
            env.setdefault("lt_cv_to_tool_file_cmd", "func_convert_file_noop")
        make_suffix = ".exe" if target_os == "windows" else ""
        port_make = port_prefix / "bin" / f"make{make_suffix}"
        if port_make.exists():
            env["MAKE"] = path_for_crt_shell(windows_short_path(port_make)) if target_os == "windows" else str(port_make)
        else:
            env["MAKE"] = env.get("MAKE") or find_host_make(target_os) or "make"
    else:
        env["CC"] = f"{root_env}/tools/crt-cc" if use_msys_paths else str(root / "tools" / "crt-cc")
        env["CXX"] = f"{root_env}/tools/crt-c++" if use_msys_paths else str(root / "tools" / "crt-c++")
        env["PATH"] = f"{tools_dir_env}{os.pathsep}{env.get('PATH', '')}"
    env["AR"] = env.get("AR") or find_llvm_tool("llvm-ar") or shutil.which("ar") or "ar"
    env["RANLIB"] = env.get("RANLIB") or find_llvm_tool("llvm-ranlib") or shutil.which("ranlib") or "ranlib"
    env["STRIP"] = env.get("STRIP") or find_llvm_tool("llvm-strip") or shutil.which("strip") or "strip"
    if target_os == "windows" and use_crt_shell:
        for tool_var in ("AR", "RANLIB", "STRIP", "LD", "DLLTOOL", "OBJDUMP", "NM", "RC"):
            if not env.get(tool_var):
                continue
            tool_path = Path(env[tool_var])
            if tool_path.is_absolute():
                env[tool_var] = native_windows_tool_command(root, root_env, shell, target_os, tool_path)
    env["PKG_CONFIG_LIBDIR"] = f"{port_prefix_env}/lib/pkgconfig"
    env["PKG_CONFIG_PATH"] = env["PKG_CONFIG_LIBDIR"]
    include_flags = f"-I{port_prefix_env}/include"
    lib_flags = f"-L{port_prefix_env}/lib"
    env["CPPFLAGS"] = join_flags(include_flags, env.get("CRT_EXTRA_CPPFLAGS", ""))
    env["CFLAGS"] = join_flags(env.get("CRT_PORT_CFLAGS", "-O2"), env.get("CRT_EXTRA_CFLAGS", ""))
    env["CXXFLAGS"] = join_flags(env.get("CRT_PORT_CXXFLAGS", "-O2"), env.get("CRT_EXTRA_CXXFLAGS", ""))
    # -rpath here (macOS/Linux only -- PE/COFF has no rpath concept, and
    # lld-link in MSVC-compatible mode doesn't understand the flag at
    # all) is what lets one port's shared library find *another port's*
    # shared library at runtime -- e.g. libpng.so depending on libz.so.
    # tools/crt-cc/tools/crt-c++'s own -Wl,-rpath addition (see their
    # Linux shared_mode comment) only covers this project's own sysroot
    # (libc.so/libm.so/...), which is a different directory entirely from
    # PORT_PREFIX/lib where third-party ports install their own shared
    # libraries -- without this, a port's shared library falls back to
    # searching the *host's* standard library path for the same bare
    # SONAME, which can genuinely resolve to a different, real,
    # legitimately-working library of the same name/version already
    # installed on the host (e.g. Ubuntu's own libz.so.1 package),
    # silently linking against the wrong build. Confirmed for real: `ldd`
    # on a real Linux aarch64 machine showed libpng16.so's libz.so.1
    # dependency resolving to /lib/aarch64-linux-gnu/libz.so.1 (the
    # system's own zlib package) instead of this project's own,
    # freshly-built one sitting right next to libpng16.so in the same
    # PORT_PREFIX/lib directory.
    rpath_flag = f"-Wl,-rpath,{port_prefix_env}/lib" if target_os in ("macos", "linux") else ""
    env["LDFLAGS"] = join_flags(lib_flags, rpath_flag, env.get("CRT_EXTRA_LDFLAGS", ""))
    env["LIBS"] = env.get("CRT_EXTRA_LIBS", "")
    env["DESTDIR"] = ""
    env["CRT_PORT_BUILD_DIR"] = build_dir_env
    # DYLD_LIBRARY_PATH/LD_LIBRARY_PATH: a runtime-loader fallback search
    # path pointing at this port's shared-library install dir, alongside
    # the -Wl,-rpath flag above -- needed for real, not just belt-and-
    # suspenders: found while porting curl on macOS, where its own
    # configure runs a real "checking runtime libs availability" probe
    # (compiles AND EXECUTES a tiny test program linked against
    # -lmbedtls/-lmbedx509/-lmbedcrypto/-lz) as part of detecting the
    # mbedTLS backend. That probe failed outright ("one or more libs
    # available at link-time are not available runtime") because
    # mbedtls's own hand-written library/Makefile builds its .dylib
    # files with no explicit -install_name at all (unlike every other
    # shared-library recipe here, which drives real GNU Libtool and
    # gets a correct one automatically) -- ld64's default records a
    # bare "libmbedcrypto.dylib" with no @rpath/absolute-path prefix at
    # all, which dyld can never resolve via LC_RPATH (macOS's rpath
    # mechanism only helps references already prefixed "@rpath/...";
    # unlike Linux's DT_RPATH/DT_RUNPATH, it does nothing for a bare
    # name) regardless of the -Wl,-rpath flag already in LDFLAGS above.
    # This project's own run_port_tests()/port_test_env() already set
    # this same env var for *running* a port's own test binary
    # afterward (which is why mbedtls's own shared-library test already
    # passed on macOS despite this) -- but configure's own internal
    # runtime probes, run as part of the *build* step (not the test
    # step), never inherited it. Setting it here, once, generically,
    # covers both without needing an mbedtls-specific workaround.
    if target_os in ("linux", "macos"):
        runtime_lib_var = "LD_LIBRARY_PATH" if target_os == "linux" else "DYLD_LIBRARY_PATH"
        existing = env.get(runtime_lib_var, "")
        env[runtime_lib_var] = f"{port_prefix_env}/lib{os.pathsep}{existing}" if existing else f"{port_prefix_env}/lib"
    return env


def make_command_for_shell(make_args, target_os, use_crt_shell):
    return shlex.join([str(arg) for arg in make_args])


def apply_source_patches(work, recipe):
    """Applies recipe["build"]["patches"] -- a list of {"file", "find",
    "replace"} objects -- as plain, exact-text substring replacements
    against the freshly copy_source()'d work tree. Deliberately a simple
    find/replace, not a unified-diff engine: this project's stated policy
    (docs/porting_status.md) is to keep upstream source unchanged wherever
    possible and treat any patch as a documented policy exception, so the
    expectation is a handful of small, one-line, easy-to-audit-in-the-
    recipe-JSON-itself edits, not a general patch-management system.
    Re-applied on every build_port() call (copy_source() always re-copies
    the work tree fresh first, --rebuild or not), so there is no separate
    "already patched" state to track. Fails loudly if the exact text to
    find is missing -- silently no-op'ing a patch that no longer matches
    (e.g. after an upstream version bump) would be worse than a build
    failure that points straight at why."""
    port_name = recipe["name"]
    for patch in recipe["build"].get("patches", []):
        target = work / patch["file"]
        text = target.read_text()
        find = patch["find"]
        if find not in text:
            raise SystemExit(
                f"{port_name}: patch target text not found in {patch['file']} "
                f"(upstream source changed? patch: {patch.get('reason', '(no reason given)')})"
            )
        target.write_text(text.replace(find, patch["replace"], 1))
        progress(f"{port_name}: patched {patch['file']}")


# Space-separated flag accumulator variables: a shared base value (e.g.
# CFLAGS=-O2) and a per-OS addition (e.g. Windows-only -U_WIN32 undefs) are
# both meant to survive in the final value, so target_overrides.<os>.env
# appends to these rather than replacing them -- matching how
# target_overrides.<os>.cflags/configure_args (plain lists) already extend
# rather than replace the base list. Anything else (RANLIB, CC, a probe
# variable like gcc_cv_as_cfi_pseudo_op, ...) is a single value, not a flag
# list, so a target override there means "use this value instead", and gets
# replaced -- notably RANLIB, which already has a real, non-empty default
# from make_env() before any recipe env is applied, so accumulating it would
# wrongly append after that default instead of overriding it.
_ENV_FLAG_ACCUMULATOR_VARS = {"CFLAGS", "CPPFLAGS", "CXXFLAGS", "LDFLAGS", "LIBS"}


def apply_recipe_env(env, recipe, target_os, root, preset_build_dir=None, work_build_dir=None, sysroot=None, port_prefix=None):
    build = recipe["build"]
    # @ROOT@/@PORT_PREFIX@/etc. substitution (same tokens
    # configure_args/make_args/install_args already get via
    # substitute_recipe_value) applied to env values too -- added so a
    # recipe's env block can reference a repo-checked-in file by an
    # absolute, host-independent path (e.g. a linker response file:
    # see mbedtls.json's own Windows LDFLAGS, which points
    # --exclude-symbols's several-hundred-entry list at
    # @ROOT@/porting/recipes/mbedtls-windows-exclude-symbols.rsp via
    # -Wl,@... instead of spelling every --exclude-symbols=NAME flag
    # out on the command line -- the fully-spelled-out form blew past
    # this project's own rootfs mksh's argv length limit ("Argument
    # list too long") well before hitting any real Windows
    # CreateProcess limit). preset_build_dir/work_build_dir/sysroot/
    # port_prefix are optional (default None, substituted as empty
    # tokens) purely so any future caller that doesn't need path
    # substitution isn't forced to pass them.
    def subst(value):
        if preset_build_dir is None:
            return str(value)
        return substitute_recipe_value(value, root, preset_build_dir, work_build_dir, sysroot, port_prefix, target_os)

    for name, value in build.get("env", {}).items():
        env[name] = subst(value)
    for name, value in build.get("target_overrides", {}).get(target_os, {}).get("env", {}).items():
        value = subst(value)
        if name in _ENV_FLAG_ACCUMULATOR_VARS and env.get(name):
            env[name] = f"{env[name]} {value}"
        else:
            env[name] = value
    # include_dirs: paths relative to this repo's root, turned into -I
    # flags added to CPPFLAGS (an accumulator var, so this composes with
    # everything else already there). Recipe JSON has no path-templating
    # of its own, so this is the escape hatch for "add a project-owned
    # header directory to a port's include path" -- e.g. a small,
    # project-owned Win32-API-shim header satisfying a narrow, specific
    # #include <windows.h> a port's source needs, the same
    # __declspec(dllimport)-declaration style already used throughout
    # libc/src/arch/windows/, without inventing a whole real <windows.h>.
    include_dirs = build.get("include_dirs", []) + build.get("target_overrides", {}).get(target_os, {}).get(
        "include_dirs", []
    )
    if include_dirs:
        flags = " ".join(f"-I{path_for_crt_shell(root / d)}" for d in include_dirs)
        env["CPPFLAGS"] = f"{flags} {env['CPPFLAGS']}" if env.get("CPPFLAGS") else flags
    # force_include: same path-templating as include_dirs, but for a file
    # unconditionally prepended to every translation unit via -include
    # (GCC/Clang), rather than merely added to the include search path.
    # Needed when the thing that must see a compatibility shim first
    # doesn't itself #include anything this project names or controls --
    # e.g. GNU Libtool's own generated `.libs/lt-*.c` "uninstalled
    # execution" wrapper source (ltmain.sh's own template, not the port's
    # source at all), which #include <unistd.h>/<stdio.h>/... by name, so
    # there's no upstream #include for a like-named shim (the
    # include_dirs/porting/shims/win32/windows.h trick) to intercept.
    #
    # Deliberately folded into CFLAGS, not CPPFLAGS. Automake's implicit
    # compile rule reads both ($(CC) ... $(AM_CPPFLAGS) $(CPPFLAGS)
    # $(AM_CFLAGS) $(CFLAGS)), but its LINK rule -- $(CCLD) $(AM_CFLAGS)
    # $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@ -- never references
    # CPPFLAGS at all. That LINK line is exactly where a `.libs/lt-*.c`
    # wrapper actually gets compiled: libtool generates and compiles it
    # in one step as part of linking an executable against an
    # uninstalled shared library, so it only ever sees the LINK line's
    # flags. Confirmed directly against a real libpng build log: the
    # pngtest.o *compile* line carried -include correctly (via
    # CPPFLAGS), but the pngtest.exe *link* line -- where lt-pngtest.c
    # is actually compiled -- carried no -include at all, leaving
    # _getcwd/_stat/_chmod/_putenv/_setmode/_spawnv undeclared even
    # though the shim itself was already verified correct in isolation.
    # CFLAGS is the one accumulator var common to both rules.
    force_include = build.get("force_include", []) + build.get("target_overrides", {}).get(target_os, {}).get(
        "force_include", []
    )
    if force_include:
        flags = " ".join(f"-include {path_for_crt_shell(root / f)}" for f in force_include)
        env["CFLAGS"] = f"{flags} {env['CFLAGS']}" if env.get("CFLAGS") else flags


def substitute_recipe_value(value, root, preset_build_dir, work_build_dir, sysroot, port_prefix, target_os):
    replacements = {
        "@ROOT@": root,
        "@BUILD_DIR@": preset_build_dir,
        "@WORK_ROOT@": work_build_dir,
        "@SYSROOT@": sysroot,
        "@PORT_PREFIX@": port_prefix,
    }
    result = str(value)
    for token, path in replacements.items():
        text = path_for_crt_shell(path) if target_os == "windows" else str(path)
        result = result.replace(token, text)
    return result


def substitute_recipe_values(values, root, preset_build_dir, work_build_dir, sysroot, port_prefix, target_os):
    return [
        substitute_recipe_value(value, root, preset_build_dir, work_build_dir, sysroot, port_prefix, target_os)
        for value in values
    ]


def command_value_argv(value, preset_build_dir, target_os):
    argv = shlex.split(value)
    if target_os == "windows" and argv and argv[0] == "/system/bin/mksh":
        argv[0] = str(rootfs_mksh_path(preset_build_dir, target_os))
    return argv


def build_configure_port(root, preset_build_dir, work, port_prefix, recipe, env, target_os, mingw_triple, use_crt_shell=False, configure_only=False, jobs=None):
    port_name = recipe["name"]
    build = recipe["build"]
    shell = rootfs_mksh_path(preset_build_dir, target_os)
    # @PORT_PREFIX@ substitution, computed once up front so both
    # configure_args (below) and make_args/install_args (further down)
    # can use it: needed whenever a recipe's own configure step must
    # reference ANOTHER already-installed port's location directly (e.g.
    # curl's --with-mbedtls=@PORT_PREFIX@/--with-zlib=@PORT_PREFIX@,
    # since every port in this project shares one install prefix). A
    # first attempt at curl's own recipe found this substitution was
    # simply never applied to configure_args at all (only make_args/
    # install_args did it) -- confirmed by a real build attempt showing
    # the literal, unsubstituted string "@PORT_PREFIX@" in configure's
    # own --with-mbedtls argument and resulting CPPFLAGS.
    port_prefix_text = path_for_crt_shell(port_prefix) if target_os == "windows" else str(port_prefix)
    # skip_configure: some upstream sources (mbedtls's 3.x LTS series among
    # them) ship a plain, hand-written top-level Makefile with no
    # ./configure step at all -- there's nothing to run, and no --prefix
    # convention to append to. Everything else in this function (patches,
    # env/CFLAGS overrides already applied by apply_recipe_env before this
    # function is even called, make_args/install_args, Windows shell
    # wrapping, parallel -jN jobs) is identical to the configure-based
    # flow, so this only skips the one step that doesn't apply, rather
    # than duplicating the whole function for a second "system" type.
    #
    # pre_configure_copy / configure_cwd: freetype is the first recipe in
    # this queue whose real top-level ./configure is not a flat autoconf
    # script but a thin, non-autoconf wrapper (see porting/recipes/
    # freetype.json's own notes) that -- for an in-tree build, which every
    # recipe here is -- does nothing but (1) `cp builds/unix/unix.mk
    # config.mk` and (2) `cd builds/unix && ./configure $CFG` (the real,
    # autoconf-generated configure FreeType ships one directory down).
    # Upstream reaches that same pair of steps via `$MAKE setup unix`, a
    # *recursive* GNU Make invocation launched from inside the wrapper
    # script itself. That recursive invocation works fine as-is on Linux/
    # macOS (a real bash + coreutils + GNU make, exactly what upstream's
    # own wrapper assumes -- verified for real, 2026-08-24), but was tried
    # for real on Windows the same day and found genuinely broken on this
    # PAL there: the nested make.exe (a real, non-mksh-aware native
    # Windows GNU Make) cannot resolve real toybox applets (cat/cp)
    # through this project's own rootfs-relative $PATH on its own
    # (`make.exe: cat: No such file or directory`), and forcing every
    # recipe command through this project's own mksh via a
    # `SHELL=/system/bin/mksh` override (the same technique make_args/
    # install_args below already use successfully for every
    # *non-recursive* make invocation in this file) instead produced a
    # silent, hard crash (exit 139, no output at all, not even GNU Make's
    # own $(info) banner text) -- a real, reproduced instability in nested
    # mksh-launched-from-make process spawning on this PAL, matching the
    # class of problem already flagged elsewhere in this file (the -jN
    # jobserver "Bad file descriptor" crash, see the `jobs` comment further
    # down). Rather than debug that nested-shell crash further, this
    # recipe-level pair of fields (read from target_overrides.<os> the same
    # way configure_args/make_args already are, so Linux/macOS can keep
    # using upstream's own unmodified wrapper while only Windows opts into
    # the workaround) replicates the wrapper's own two real steps directly
    # -- pre_configure_copy (a list of {"src","dst"} pairs, both relative
    # to the port's work directory, copied verbatim before configure runs)
    # and configure_cwd (a work-relative subdirectory to run ./configure
    # from, instead of the work root) -- with no recursive make involved
    # at all. Both default to "no-op"/"." so every existing flat-
    # ./configure recipe (i.e. every recipe but this one) is unaffected.
    os_overrides = build.get("target_overrides", {}).get(target_os, {})
    for copy_entry in os_overrides.get("pre_configure_copy", build.get("pre_configure_copy", [])):
        copy_src = work / copy_entry["src"]
        copy_dst = work / copy_entry["dst"]
        progress(f"{port_name}: pre-configure copy {copy_entry['src']} -> {copy_entry['dst']}")
        shutil.copy2(copy_src, copy_dst)
    configure_cwd_rel = os_overrides.get("configure_cwd", build.get("configure_cwd"))
    configure_cwd = work / configure_cwd_rel if configure_cwd_rel else work
    if not build.get("skip_configure", False):
        configure = ["./configure"]
        configure.extend(build["configure_args"])
        configure.extend(build.get("target_overrides", {}).get(target_os, {}).get("configure_args", []))
        # @CRT_MINGW_TRIPLE@: e.g. libpng/libffi's --build=@CRT_MINGW_TRIPLE@
        # (config.guess can't recognize plain Windows `uname` output, so
        # configure needs an explicit --build= triple). Substituted here
        # rather than hardcoded per-recipe so the same recipe JSON works on
        # both Windows aarch64 and x86_64 -- mingw_triple is resolved once in
        # main() via detect_target_arch()/mingw_triple_for_arch().
        configure = [arg.replace("@CRT_MINGW_TRIPLE@", mingw_triple) for arg in configure]
        # @ROOT@: the same gap this function's own @PORT_PREFIX@ comment
        # above already documents finding once (curl's --with-X= flags) --
        # confirmed for real again (2026-08-31, ffmpeg.json's own
        # --cc=@ROOT@/tools/crt-cc): configure_args never substituted
        # @ROOT@ at all, only @CRT_MINGW_TRIPLE@/@PORT_PREFIX@, so FFmpeg's
        # own non-autoconf configure script (which needs --cc= passed
        # explicitly -- unlike every other recipe here, it does not read
        # $CC from the environment) received the literal, unsubstituted
        # string "@ROOT@/tools/crt-cc" as its C compiler and failed its
        # own compiler smoke test outright. Needed here for the same
        # reason substitute_recipe_value()/apply_recipe_env() already
        # support @ROOT@ for cflags/env/make_args/install_args/test
        # fields -- configure_args was simply never wired to it.
        configure = [arg.replace("@ROOT@", str(root)) for arg in configure]
        configure = [arg.replace("@PORT_PREFIX@", port_prefix_text) for arg in configure]
        if is_native_windows_configure(target_os):
            prefix = path_for_crt_shell(port_prefix) if use_crt_shell else path_for_msys_shell(port_prefix)
        else:
            prefix = str(port_prefix)
        configure.append(f"--prefix={prefix}")
        if use_crt_shell:
            shell = str(shell)
            env["CONFIG_SHELL"] = "/system/bin/mksh"
            if not is_native_windows_configure(target_os):
                env["CONFIG_SHELL"] = shell
            # TEMPORARY diagnostic: set CRT_PORT_SHELL_XTRACE=1 to run configure
            # under `mksh -x` for tracing a silent (no-output) configure failure.
            # Remove once the Windows aarch64 zlib configure investigation is
            # resolved.
            shell_argv = [shell, "-x"] if os.environ.get("CRT_PORT_SHELL_XTRACE") else [shell]
            run(shell_argv + configure, configure_cwd, env, f"{port_name}: configure")
        elif is_native_windows_configure(target_os):
            shell = find_posix_shell(env)
            env["CONFIG_SHELL"] = path_for_msys_shell(shell)
            configure = [shell] + configure
            run(configure, configure_cwd, env, f"{port_name}: configure")
        else:
            run(configure, configure_cwd, env, f"{port_name}: configure")
        # post_configure_patch (read from target_overrides.<os> the same way
        # pre_configure_copy/configure_cwd above are): a list of {"file",
        # "find", "replace"} literal (non-regex) string substitutions applied
        # to a real ./configure *output* file, once, right after configure
        # finishes. freetype's own generated builds/unix/unix-def.mk hits a
        # genuine Windows-only GNU Make syntax collision (2026-08-24, found
        # building this recipe for real): its line `TOP_DIR := $(shell cd
        # $(TOP_DIR); pwd)` (standard autoconf-generated behavior, true on
        # every real Unix host too, and harmless there since a real POSIX
        # absolute path never contains a colon) unconditionally forces
        # TOP_DIR -- and, transitively, OBJ_DIR/OBJ_BUILD/PLATFORM_DIR,
        # every one of them derived from it -- absolute via a real `pwd`
        # call. On this project's own native Windows PAL, `pwd` legitimately
        # returns a real drive-letter path (`C:/Users/...`), and this
        # project's own ported GNU Make (porting/recipes/make.json) builds
        # deliberately for its generic POSIX code path on Windows too, NOT
        # GNU Make's own mainline HAVE_DOS_PATHS-guarded drive-letter
        # awareness -- an explicit, already-recorded project decision (see
        # make.json's own notes: "Windows intentionally uses the POSIX-like
        # musl config path rather than the upstream Win32 make path so
        # failures expose CRT/PAL gaps"), not something this recipe should
        # work around by changing make itself. FreeType's own (unusually,
        # for this porting queue) hand-rolled, pre-automake build system has
        # at least 11 separate rule-declaration lines across builds/*.mk
        # that each combine TWO such TOP_DIR/OBJ_DIR-derived expansions on
        # one line (confirmed via a real recursive grep across the whole
        # builds/ tree) -- each one, once absolute, becomes real GNU Make
        # static-pattern-rule syntax (`targets : target-pattern :
        # prereq-patterns`, triggered by any two colons on one rule-
        # declaration line), so Make aborts parsing the *whole file*
        # (`target pattern contains no '%'`) on whichever one it reaches
        # first -- not just freetype-config-style convenience rules, real
        # object-file build rules too (freetype.mk's own $(FTSYS_OBJ)/
        # $(FTINIT_OBJ)/etc.), so patching each broken rule line
        # individually would be real whack-a-mole across many files for no
        # good reason. Patching this ONE root-cause line instead (a no-op
        # self-assignment, `TOP_DIR := $(TOP_DIR)`) keeps TOP_DIR at the top
        # Makefile's own original, already-relative default (`TOP_DIR ?=
        # .`) for this project's always-in-tree build (confirmed safe: no
        # `$(MAKE) -C`/`cd ...; $(MAKE)` recursive sub-make anywhere in this
        # tree that would need an absolute TOP_DIR to survive a directory
        # change), which makes every one of those 11+ rule-declaration
        # lines relative-vs-relative (a single real colon) automatically,
        # with no other file needing a matching patch.
        for patch_entry in os_overrides.get("post_configure_patch", build.get("post_configure_patch", [])):
            patch_path = work / patch_entry["file"]
            progress(f"{port_name}: post-configure patch {patch_entry['file']}")
            patch_text = patch_path.read_text(encoding="utf-8")
            if patch_entry["find"] not in patch_text:
                raise SystemExit(
                    f"{port_name}: post_configure_patch text not found in {patch_path}\n"
                    f"looking for: {patch_entry['find']!r}"
                )
            patch_path.write_text(patch_text.replace(patch_entry["find"], patch_entry["replace"]), encoding="utf-8")
    if configure_only:
        progress(f"{port_name}: configure-only stop")
        return
    make = env.get("MAKE", "make")
    if jobs is None:
        # Same default on every OS, Windows included: os.cpu_count() (or a
        # conservative fallback of 2 if that's unavailable). Windows used to
        # default to serial (`jobs = 1`) here because GNU Make's jobserver
        # crashed outright under real `-jN` (N>1) concurrency on this PAL
        # (`make.exe: /system/bin/mksh: Bad file descriptor`) -- root-caused
        # and fixed, then stress-tested against a real libpng build (much
        # larger and more concurrent than zlib, real GNU Libtool, real
        # dependency graph) with real parallelism before removing the
        # special case; see HISTORY.md's 2026-08-11 entries. Pass --jobs
        # explicitly (this script's own CLI flag) to override this default
        # for any single invocation.
        jobs = os.cpu_count() or 2
    # target_overrides.<os>.make_args extends the base make_args the same
    # way configure_args/cflags already do (see apply_recipe_env/
    # build_configure_port's own configure_args handling above) -- e.g.
    # a host-scoped `make VAR=value` override to replace a value an
    # upstream Makefile bakes in unconditionally (AM_LTLDFLAGS in
    # libffi's own Makefile.am, which always adds a real, GNU-Libtool-
    # generated `-bindir "$(bindir)"`; libtool's own func_normal_abspath
    # only recognizes a leading '/' as marking an absolute path, so a
    # real Windows drive-letter path handed to -bindir sends it into a
    # genuine infinite loop trying to ascend to a root it can never
    # reach -- see porting/recipes/libffi.json's own notes).
    # @PORT_PREFIX@ substitution for make_args/install_args (port_prefix_text
    # itself is computed once, up top of this function -- see that comment):
    # needed for skip_configure recipes (like mbedtls) whose Makefile uses a
    # `make`-variable install convention (DESTDIR=) instead of a ./configure
    # --prefix= one, so the port's real install path has to reach
    # make_args/install_args as a `make` variable rather than a configure
    # flag.
    override_make_args = [
        arg.replace("@PORT_PREFIX@", port_prefix_text)
        for arg in build.get("target_overrides", {}).get(target_os, {}).get("make_args", [])
    ]
    # build_make_args (new: target_overrides.<os>.build_make_args, distinct
    # from target_overrides.<os>.make_args above): a `make` variable that
    # must reach the BUILD step only, never `make install` -- unlike every
    # existing use of the (build+install)-shared make_args override, which
    # all only ever change *values* consumed identically by both steps.
    # mbedtls's own top-level Makefile needs this: WINDOWS=1 selects the
    # correctly-named .dll build (see the recipe's own notes for the
    # -lbcrypt/-lws2_32/__udivti3/AESNI fixes that made this build shape
    # possible), but the SAME top-level Makefile also wraps its *entire*
    # `install:`/`uninstall:` block in `ifndef WINDOWS` -- passing
    # WINDOWS=1 to `make install` too doesn't just change what install:
    # does, it makes install: NOT EXIST AT ALL (confirmed directly: GNU
    # Make's own -p database dump showed install: surviving only as an
    # empty .PHONY entry with no recipe once WINDOWS=1 reached it, so
    # `make install WINDOWS=1` silently no-ops -- exit 0, nothing copied,
    # no error -- rather than failing loudly).
    build_only_make_args = [
        arg.replace("@PORT_PREFIX@", port_prefix_text)
        for arg in build.get("target_overrides", {}).get(target_os, {}).get("build_make_args", [])
    ]
    make_args = [make, "-j", str(jobs)] + build["make_args"] + override_make_args + build_only_make_args
    # install_args gets the same VAR=value overrides as make_args (but not
    # the base build["make_args"]/-jN, which are build-target-specific
    # flags like parallelism that don't apply to `make install`): a
    # recipe-level `make` variable override almost always needs to reach
    # both steps, not just the first -- e.g. xz/liblzma's Windows build
    # overriding away liblzma_w32res.rc's automake-generated variables
    # (see porting/recipes/xz.json's own notes) skipped the file during
    # `make` but `make install`'s own dependency chain re-triggered
    # building it anyway, since install_args previously never saw the
    # override at all.
    #
    # install_args (base recipe level, distinct from target_overrides.
    # <os>.make_args above): extra arguments specific to the install step
    # only, not the build step -- e.g. mbedtls's DESTDIR=@PORT_PREFIX@,
    # which would be harmless but meaningless during `make lib`.
    base_install_args = [
        arg.replace("@PORT_PREFIX@", port_prefix_text) for arg in build.get("install_args", [])
    ]
    install_args = [make, "install"] + base_install_args + override_make_args
    if target_os == "windows" and use_crt_shell:
        make_args.append("SHELL=/system/bin/mksh")
        install_args.append("SHELL=/system/bin/mksh")
    # Some GCC-family autotools trees (libffi among them) configure a
    # top-level "multilib dispatcher" Makefile -- generated by piping the
    # real, per-host Makefile through a bundled sed script (makefile.sed)
    # that strips it down to a thin recursive-make wrapper -- alongside the
    # real, complete Makefile in a $build-triple-named subdirectory (e.g.
    # aarch64-w64-mingw32/). That sed script assumes Unix paths throughout
    # and mishandles a Windows drive-letter colon (e.g. "MAKE=C:/Users/...")
    # as if it were a Makefile "target:" separator, silently truncating the
    # substituted value (observed: "MAKE=C:/Users/.../make.exe" became just
    # "MAKE=C:") and breaking the dispatcher outright ("recipe commences
    # before first target"). The real subdirectory Makefile is unaffected
    # (ordinary Automake substitution, not this ad-hoc sed hack) and was
    # itself configured with the same --prefix, so make_subdir lets a
    # recipe route straight to it and skip the broken dispatcher entirely.
    make_subdir = build.get("target_overrides", {}).get(target_os, {}).get("make_subdir")
    if make_subdir:
        make_subdir = make_subdir.replace("@CRT_MINGW_TRIPLE@", mingw_triple)
    make_work = work / make_subdir if make_subdir else work
    if use_crt_shell:
        run([str(shell), "-c", make_command_for_shell(make_args, target_os, use_crt_shell)], make_work, env, f"{port_name}: make")
        run([str(shell), "-c", make_command_for_shell(install_args, target_os, use_crt_shell)], make_work, env, f"{port_name}: make install")
    else:
        run(make_args, make_work, env, f"{port_name}: make")
        run(install_args, make_work, env, f"{port_name}: make install")

    # Some generated public headers defer ABI layout decisions to host
    # compiler macros that the CRT wrapper intentionally hides from normal C
    # consumers. Apply narrowly-scoped transformations to installed artifacts
    # so their ABI follows the stable CRT target macros instead. Upstream
    # source remains untouched, and an exact find match makes version drift
    # fail loudly rather than silently producing a mismatched public header.
    for patch_entry in os_overrides.get("post_install_patch", build.get("post_install_patch", [])):
        patch_path = port_prefix / patch_entry["file"]
        progress(f"{port_name}: post-install patch {patch_entry['file']}")
        patch_text = patch_path.read_text(encoding="utf-8")
        if patch_entry["find"] not in patch_text:
            raise SystemExit(
                f"{port_name}: post_install_patch text not found in {patch_path}\n"
                f"looking for: {patch_entry['find']!r}"
            )
        patch_path.write_text(
            patch_text.replace(patch_entry["find"], patch_entry["replace"]),
            encoding="utf-8",
        )


def amalgamation_shared_base_name(port_name, archive_name):
    """Derive the shared-library base name ("sqlite3") from the recipe's
    static archive filename ("libsqlite3.a") rather than adding a second,
    redundant recipe field that could drift out of sync with it."""
    if not (archive_name.startswith("lib") and archive_name.endswith(".a")):
        raise SystemExit(
            f"{port_name}: cannot derive a shared-library name from archive "
            f"'{archive_name}' (expected 'lib<name>.a')"
        )
    return archive_name[len("lib"):-len(".a")]


def build_amalgamation_shared_library(port_name, recipe, port_prefix, env, cc_argv, pic_objects, target_os, work):
    """Links the PIC objects build_amalgamation_port() already compiled
    into a real shared library, named and versioned per OS the same way
    zlib's own hand-written Makefile does it (see that recipe/tools/crt-cc
    for the established convention this mirrors): a real, versioned file
    plus SONAME-style symlink aliases on macOS/Linux, a plain unversioned
    .dll on Windows (matching how upstream SQLite itself ships its own
    precompiled Windows binary as a bare "sqlite3.dll", no "lib" prefix,
    no version suffix -- Windows has no equivalent SONAME-symlink
    convention). No .lib import library is generated on Windows, matching
    zlib's own precedent this session: lld-link can link a consumer
    directly against the built .dll by its exact filename (verified
    working for zlib's examplesh/minigzipsh test binaries), so nothing
    else in this project's port-build flow has needed one either.
    tools/crt-cc/tools/crt-c++'s own -shared/-dynamiclib handling already
    covers everything OS/arch-specific this needs (shared vs. static CRT
    linking, dllcrt.o + /DLL on Windows, -rpath on macOS/Linux) -- this
    function only adds the handful of flags that are specific to *this*
    library's own name/version (-soname/-install_name), not to the
    target platform in general."""
    build = recipe["build"]
    base_name = amalgamation_shared_base_name(port_name, build["archive"])
    version = recipe.get("version", "0.0.0")
    major = version.split(".")[0]
    lib_dir = port_prefix / "lib"
    ldflags = shlex.split(env.get("LDFLAGS", ""))
    link_cmd = list(cc_argv)

    if target_os == "windows":
        dll_name = f"{base_name}.dll"
        dll_path = lib_dir / dll_name
        link_cmd += ["-shared", "-O2"] + [str(o) for o in pic_objects] + ldflags + ["-o", str(dll_path)]
        run(link_cmd, work, env, f"{port_name}: link {dll_name}")
        return

    real_name = None
    soname = None
    dev_name = f"lib{base_name}.{'so' if target_os == 'linux' else 'dylib'}"
    if target_os == "linux":
        real_name = f"lib{base_name}.so.{version}"
        soname = f"lib{base_name}.so.{major}"
        real_path = lib_dir / real_name
        link_cmd += ["-shared", "-O2", f"-Wl,-soname,{soname}"] + [str(o) for o in pic_objects] + ldflags + [
            "-o", str(real_path)
        ]
    elif target_os == "macos":
        real_name = f"lib{base_name}.{version}.dylib"
        soname = f"lib{base_name}.{major}.dylib"
        real_path = lib_dir / real_name
        install_name = f"{lib_dir}/{soname}"
        link_cmd += [
            "-dynamiclib", "-O2",
            "-install_name", install_name,
            "-compatibility_version", f"{major}.0.0",
            "-current_version", version,
        ] + [str(o) for o in pic_objects] + ldflags + ["-o", str(real_path)]
    else:
        return

    run(link_cmd, work, env, f"{port_name}: link {real_name}")
    for alias_name in (soname, dev_name):
        alias_path = lib_dir / alias_name
        if alias_path.is_symlink() or alias_path.exists():
            alias_path.unlink()
        os.symlink(real_name, alias_path)
        progress(f"{port_name}: alias {alias_name} -> {real_name}")


def build_amalgamation_port(preset_build_dir, work, port_prefix, recipe, env, target_os):
    port_name = recipe["name"]
    build = recipe["build"]
    objects = []
    pic_objects = []
    cflags = shlex.split(env.get("CPPFLAGS", "")) + shlex.split(env.get("CFLAGS", ""))
    cflags.extend(build.get("cflags", []))
    cflags.extend(build.get("target_overrides", {}).get(target_os, {}).get("cflags", []))

    # CC (and, defensively, AR/RANLIB) can be a multi-token string when
    # --use-crt-shell wraps the real tool in a mksh invocation (e.g.
    # "<mksh path> <crt-cc path>"), not just a single executable path --
    # matching how build_android_host_tool_port() already handles env["CC"]
    # via this same helper instead of passing it as one argv element.
    cc_argv = command_value_argv(env["CC"], preset_build_dir, target_os)
    ar_argv = command_value_argv(env["AR"], preset_build_dir, target_os)
    ranlib_argv = command_value_argv(env["RANLIB"], preset_build_dir, target_os)

    # "shared": true opts a recipe into also building a real shared
    # library (.so/.dylib/.dll) alongside the static archive this build
    # system has always produced -- off by default so a future
    # amalgamation-style recipe that genuinely shouldn't get one (e.g.
    # something not meant to be dynamically loaded) doesn't silently
    # acquire one just by using this same build system.
    build_shared = bool(build.get("shared", False))

    sources = list(build["sources"])
    for index, source in enumerate(sources, 1):
        progress(f"{port_name}: compile {index}/{len(sources)} {source}")
        src = work / source
        obj = work / (Path(source).name + ".o")
        run(cc_argv + cflags + ["-I", str(work), "-c", str(src), "-o", str(obj)], work, env)
        objects.append(obj)

    if build_shared:
        # A second, -fPIC compile pass, same reasoning as zlib's own
        # Makefile (and the non-PIC-static-archive-in-a-shared-object fix
        # tools/crt-cc/tools/crt-c++ needed this session): Linux's ld
        # rejects certain relocations from non-PIC code inside a shared
        # object outright, so the objects that go into the .so/.dylib/
        # .dll must be compiled -fPIC separately from the ones that go
        # into the static .a (which has no such requirement and gains
        # nothing from paying the -fPIC cost).
        pic_flags = cflags + ["-fPIC", "-DPIC"]
        for index, source in enumerate(sources, 1):
            progress(f"{port_name}: compile (PIC) {index}/{len(sources)} {source}")
            src = work / source
            obj = work / (Path(source).name + ".pic.o")
            run(cc_argv + pic_flags + ["-I", str(work), "-c", str(src), "-o", str(obj)], work, env)
            pic_objects.append(obj)

    lib_dir = port_prefix / "lib"
    include_dir = port_prefix / "include"
    lib_dir.mkdir(parents=True, exist_ok=True)
    include_dir.mkdir(parents=True, exist_ok=True)
    archive = lib_dir / build["archive"]
    run(ar_argv + ["rcs", str(archive)] + [str(obj) for obj in objects], work, env, f"{port_name}: archive")
    run(ranlib_argv + [str(archive)], work, env, f"{port_name}: ranlib")

    if build_shared:
        build_amalgamation_shared_library(port_name, recipe, port_prefix, env, cc_argv, pic_objects, target_os, work)

    for header in build.get("install_headers", []):
        progress(f"{port_name}: install header {header}")
        shutil.copy2(work / header, include_dir / Path(header).name)


def build_android_host_tool_port(preset_build_dir, work, port_prefix, recipe, env, target_os):
    port_name = recipe["name"]
    build = recipe["build"]
    objects = []
    cflags = shlex.split(env.get("CPPFLAGS", "")) + shlex.split(env.get("CFLAGS", ""))
    cflags.extend(build.get("cflags", []))
    include_dirs = list(build.get("include_dirs", []))
    sources = list(build["sources"])

    target = build.get("target_overrides", {}).get(target_os, {})
    cflags.extend(target.get("cflags", []))
    include_dirs.extend(target.get("include_dirs", []))
    sources.extend(target.get("sources", []))
    config_undefs = target.get("config_undefs", [])
    config_redefs = target.get("config_redefs", {})
    if config_undefs or config_redefs:
        overlay = work / "crt_config_overlay"
        overlay.mkdir(parents=True, exist_ok=True)
        config = overlay / "config.h"
        lines = ["#include_next <config.h>"]
        lines.extend(f"#undef {name}" for name in config_undefs)
        for name, value in config_redefs.items():
            lines.append(f"#undef {name}")
            lines.append(f"#define {name} {value}")
        config.write_text("\n".join(lines) + "\n", encoding="utf-8")
        include_dirs.insert(0, str(overlay))

    for include_dir in include_dirs:
        include_path = Path(include_dir)
        if not include_path.is_absolute():
            include_path = work / include_path
        cflags.extend(["-I", str(include_path)])

    for index, source in enumerate(sources, 1):
        progress(f"{port_name}: compile {index}/{len(sources)} {source}")
        src = work / source
        obj = work / (source.replace("/", "_").replace("\\", "_") + ".o")
        run(command_value_argv(env["CC"], preset_build_dir, target_os) + cflags + ["-c", str(src), "-o", str(obj)], work, env)
        objects.append(obj)

    bin_dir = port_prefix / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)
    suffix = ".exe" if target_os == "windows" else ""
    binary = bin_dir / (build["binary"] + suffix)
    ldflags = shlex.split(env.get("LDFLAGS", ""))
    libs = shlex.split(env.get("LIBS", ""))
    run(command_value_argv(env["CC"], preset_build_dir, target_os) + [str(obj) for obj in objects] + ldflags + libs + ["-o", str(binary)], work, env, f"{port_name}: link {binary.name}")


def port_test_env(env, port_prefix, target_os):
    test_env = env.copy()
    lib_dir = port_prefix / "lib"
    if target_os == "linux":
        name = "LD_LIBRARY_PATH"
        search_dirs = [lib_dir]
    elif target_os == "macos":
        name = "DYLD_LIBRARY_PATH"
        search_dirs = [lib_dir]
    elif target_os == "windows":
        name = "PATH"
        search_dirs = [lib_dir, port_prefix / "bin"]
    else:
        return test_env
    current = test_env.get(name, "")
    sep = os.pathsep
    prefix = sep.join(str(path) for path in search_dirs)
    test_env[name] = f"{prefix}{sep}{current}" if current else prefix
    return test_env


def run_port_tests(root, preset_build_dir, work_build_dir, sysroot, port_prefix, recipe, target_os, mingw_triple, use_crt_shell=False):
    port_name = recipe["name"]
    tests = recipe.get("tests", [])
    if not tests:
        progress(f"{port_name}: no port tests declared")
        return

    env = make_env(root, preset_build_dir, work_build_dir, sysroot, port_prefix, target_os, mingw_triple, use_crt_shell)
    apply_recipe_env(env, recipe, target_os, root, preset_build_dir, work_build_dir, sysroot, port_prefix)
    cc_argv = command_value_argv(env["CC"], preset_build_dir, target_os)
    test_root = work_build_dir / "tests" / port_name
    test_root.mkdir(parents=True, exist_ok=True)
    suffix = ".exe" if target_os == "windows" else ""

    for test in tests:
        test_name = test["name"]
        test_type = test.get("type", "compile-run")
        if test_type != "compile-run":
            raise SystemExit(f"{port_name}: unsupported test type {test_type!r} in {test_name}")

        target = test.get("target_overrides", {}).get(target_os, {})
        source_value = target.get("source", test["source"])
        source = Path(substitute_recipe_value(source_value, root, preset_build_dir, work_build_dir, sysroot, port_prefix, target_os))
        if not source.is_absolute():
            source = root / source

        cflags = list(test.get("cflags", [])) + list(target.get("cflags", []))
        link_args = list(test.get("link_args", [])) + list(target.get("link_args", []))
        cflags = substitute_recipe_values(cflags, root, preset_build_dir, work_build_dir, sysroot, port_prefix, target_os)
        link_args = substitute_recipe_values(link_args, root, preset_build_dir, work_build_dir, sysroot, port_prefix, target_os)

        binary = test_root / f"{test_name}{suffix}"
        compile_cmd = cc_argv + cflags + [str(source)] + link_args + ["-o", str(binary)]
        run(compile_cmd, test_root, env, f"{port_name}: test build {test_name}")
        run_checked_output([str(binary)], test_root, port_test_env(env, port_prefix, target_os),
                           test.get("expect_stdout"), f"{port_name}: test run {test_name}")


def build_port(root, preset_build_dir, work_build_dir, source_root, sysroot, port_prefix, recipes, port, target_os, mingw_triple, use_crt_shell=False, configure_only=False, built=None, jobs=None):
    if built is None:
        built = set()
    if port in built:
        progress(f"{port}: already handled in this run")
        return
    if port not in recipes:
        raise SystemExit(f"recipe not found: {port}")

    recipe = recipes[port]
    build = recipe["build"]
    if build["system"] not in ("configure", "amalgamation", "android_host_tool"):
        raise SystemExit(f"{port}: build system '{build['system']}' is not supported by crt-port-build.py yet")

    if build["system"] == "configure" and port != "make" and not configure_only and "make" in recipes:
        build_port(root, preset_build_dir, work_build_dir, source_root, sysroot, port_prefix, recipes, "make", target_os, mingw_triple, use_crt_shell, configure_only, built, jobs)

    for dep in recipe["dependencies"]:
        build_port(root, preset_build_dir, work_build_dir, source_root, sysroot, port_prefix, recipes, dep, target_os, mingw_triple, use_crt_shell, configure_only, built, jobs)

    stamp = work_build_dir / "stamps" / f"{port}.installed"
    if stamp.exists():
        progress(f"{port}: installed stamp exists, skipping")
        built.add(port)
        return

    src = find_source(source_root, recipe["source"]["source_dir"])
    work = work_build_dir / "work" / port
    progress(f"{port}: prepare source {src} -> {work}")
    copy_source(src, work)
    apply_source_patches(work, recipe)

    progress(f"{port}: build system {build['system']}")
    env = make_env(root, preset_build_dir, work_build_dir, sysroot, port_prefix, target_os, mingw_triple, use_crt_shell)
    apply_recipe_env(env, recipe, target_os, root, preset_build_dir, work_build_dir, sysroot, port_prefix)
    if build["system"] == "configure":
        build_configure_port(root, preset_build_dir, work, port_prefix, recipe, env, target_os, mingw_triple, use_crt_shell, configure_only, jobs)
    elif build["system"] == "amalgamation":
        build_amalgamation_port(preset_build_dir, work, port_prefix, recipe, env, target_os)
    elif build["system"] == "android_host_tool":
        build_android_host_tool_port(preset_build_dir, work, port_prefix, recipe, env, target_os)
    if not configure_only:
        alias_unix_static_libs_for_windows_link(port_prefix, target_os)
    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text("ok\n")
    progress(f"{port}: wrote install stamp {stamp}")
    built.add(port)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preset", required=True)
    parser.add_argument("--target-os", default=None)
    parser.add_argument("--target-arch", default=None, help="aarch64/arm64 or x86_64/amd64/x64; auto-detected via CRT_TARGET_ARCH env var or platform.machine() if omitted")
    parser.add_argument("--source-root", default=None)
    parser.add_argument("--work-root", default=None)
    parser.add_argument("--install-prefix", default=None)
    parser.add_argument("--recipe-dir", default="porting/recipes")
    parser.add_argument("--port", action="append", required=True)
    parser.add_argument("--rebuild", action="store_true", help="remove port install stamps before building")
    parser.add_argument("--skip-sysroot-build", action="store_true", help="assume the sysroot target has already been built")
    parser.add_argument("--use-crt-shell", action="store_true", help="run configure recipes with the CRT rootfs mksh")
    parser.add_argument("--configure-only", action="store_true", help="stop configure recipes after ./configure")
    parser.add_argument("--test", action="store_true", help="run recipe-declared port tests after the port is installed")
    parser.add_argument("--jobs", type=int, default=None, help="override make -jN (default: 1 on Windows via --use-crt-shell, else CPU count); for reproducing/testing the Windows jobserver bug")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    recipe_dir = Path(args.recipe_dir)
    if not recipe_dir.is_absolute():
        recipe_dir = root / recipe_dir
    recipe_dir = recipe_dir.resolve()
    recipes = load_recipes(recipe_dir)
    build_dir = (root / "out" / args.preset).resolve()
    port_test_root = build_dir / "port-tests"
    source_root = Path(args.source_root) if args.source_root else port_test_root / "src"
    work_root = Path(args.work_root) if args.work_root else port_test_root / "build"
    sysroot = build_dir / "sysroot"
    port_prefix = Path(args.install_prefix) if args.install_prefix else port_test_root / "install"
    source_root = source_root.resolve()
    work_root = work_root.resolve()
    sysroot = sysroot.resolve()
    port_prefix = port_prefix.resolve()
    target_os = args.target_os or args.preset.split("-host-", 1)[0]
    mingw_triple = mingw_triple_for_arch(detect_target_arch(args.target_arch))

    if not args.skip_sysroot_build:
        target = "rootfs" if args.use_crt_shell else "sysroot"
        run(["cmake", "--build", "--preset", args.preset, "--target", target], root, os.environ.copy(), f"cmake target {target}")
    port_prefix.mkdir(parents=True, exist_ok=True)
    (port_prefix / "include").mkdir(parents=True, exist_ok=True)
    (port_prefix / "lib" / "pkgconfig").mkdir(parents=True, exist_ok=True)

    if args.rebuild:
        for port in args.port:
            stamp = work_root / "stamps" / f"{port}.installed"
            if stamp.exists():
                progress(f"{port}: remove install stamp for rebuild")
                stamp.unlink()

    for port in args.port:
        progress(f"{port}: requested")
        build_port(root, build_dir, work_root, source_root, sysroot, port_prefix, recipes, port, target_os, mingw_triple, args.use_crt_shell, args.configure_only, jobs=args.jobs)
        if args.test:
            run_port_tests(root, build_dir, work_root, sysroot, port_prefix, recipes[port], target_os, mingw_triple, args.use_crt_shell)

    progress(f"ports installed: {port_prefix}")


if __name__ == "__main__":
    main()
