#!/usr/bin/env python3
import argparse
import ctypes
import json
import os
import platform
import shlex
import shutil
import subprocess
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


def copy_source(src, dst):
    if dst.exists():
        shutil.rmtree(dst)
    ignore = shutil.ignore_patterns(".git", "autom4te.cache", "*.o", "*.a", "*.so", "*.dylib", "*.dll")
    shutil.copytree(src, dst, ignore=ignore)


def make_env(root, preset_build_dir, work_build_dir, sysroot, port_prefix, target_os, use_crt_shell=False):
    env = os.environ.copy()
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
    env["AR"] = env.get("AR") or shutil.which("llvm-ar") or shutil.which("ar") or "ar"
    env["RANLIB"] = env.get("RANLIB") or shutil.which("llvm-ranlib") or shutil.which("ranlib") or "ranlib"
    env["STRIP"] = env.get("STRIP") or shutil.which("llvm-strip") or shutil.which("strip") or "strip"
    if target_os == "windows" and use_crt_shell:
        for tool_var in ("AR", "RANLIB", "STRIP", "LD", "DLLTOOL", "OBJDUMP"):
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
    env["LDFLAGS"] = join_flags(lib_flags, env.get("CRT_EXTRA_LDFLAGS", ""))
    env["LIBS"] = env.get("CRT_EXTRA_LIBS", "")
    env["DESTDIR"] = ""
    env["CRT_PORT_BUILD_DIR"] = build_dir_env
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


def apply_recipe_env(env, recipe, target_os, root):
    build = recipe["build"]
    for name, value in build.get("env", {}).items():
        env[name] = str(value)
    for name, value in build.get("target_overrides", {}).get(target_os, {}).get("env", {}).items():
        value = str(value)
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


def command_value_argv(value, preset_build_dir, target_os):
    argv = shlex.split(value)
    if target_os == "windows" and argv and argv[0] == "/system/bin/mksh":
        argv[0] = str(rootfs_mksh_path(preset_build_dir, target_os))
    return argv


def build_configure_port(root, preset_build_dir, work, port_prefix, recipe, env, target_os, mingw_triple, use_crt_shell=False, configure_only=False):
    port_name = recipe["name"]
    build = recipe["build"]
    shell = rootfs_mksh_path(preset_build_dir, target_os)
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
        run(shell_argv + configure, work, env, f"{port_name}: configure")
    elif is_native_windows_configure(target_os):
        shell = find_posix_shell(env)
        env["CONFIG_SHELL"] = path_for_msys_shell(shell)
        configure = [shell] + configure
        run(configure, work, env, f"{port_name}: configure")
    else:
        run(configure, work, env, f"{port_name}: configure")
    if configure_only:
        progress(f"{port_name}: configure-only stop")
        return
    make = env.get("MAKE", "make")
    jobs = 1 if target_os == "windows" and use_crt_shell else (os.cpu_count() or 2)
    make_args = [make, "-j", str(jobs)] + build["make_args"]
    install_args = [make, "install"]
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


def build_amalgamation_port(preset_build_dir, work, port_prefix, recipe, env, target_os):
    port_name = recipe["name"]
    build = recipe["build"]
    objects = []
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

    sources = list(build["sources"])
    for index, source in enumerate(sources, 1):
        progress(f"{port_name}: compile {index}/{len(sources)} {source}")
        src = work / source
        obj = work / (Path(source).name + ".o")
        run(cc_argv + cflags + ["-I", str(work), "-c", str(src), "-o", str(obj)], work, env)
        objects.append(obj)

    lib_dir = port_prefix / "lib"
    include_dir = port_prefix / "include"
    lib_dir.mkdir(parents=True, exist_ok=True)
    include_dir.mkdir(parents=True, exist_ok=True)
    archive = lib_dir / build["archive"]
    run(ar_argv + ["rcs", str(archive)] + [str(obj) for obj in objects], work, env, f"{port_name}: archive")
    run(ranlib_argv + [str(archive)], work, env, f"{port_name}: ranlib")
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


def build_port(root, preset_build_dir, work_build_dir, source_root, sysroot, port_prefix, recipes, port, target_os, mingw_triple, use_crt_shell=False, configure_only=False, built=None):
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
        build_port(root, preset_build_dir, work_build_dir, source_root, sysroot, port_prefix, recipes, "make", target_os, mingw_triple, use_crt_shell, configure_only, built)

    for dep in recipe["dependencies"]:
        build_port(root, preset_build_dir, work_build_dir, source_root, sysroot, port_prefix, recipes, dep, target_os, mingw_triple, use_crt_shell, configure_only, built)

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
    env = make_env(root, preset_build_dir, work_build_dir, sysroot, port_prefix, target_os, use_crt_shell)
    apply_recipe_env(env, recipe, target_os, root)
    if build["system"] == "configure":
        build_configure_port(root, preset_build_dir, work, port_prefix, recipe, env, target_os, mingw_triple, use_crt_shell, configure_only)
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
        build_port(root, build_dir, work_root, source_root, sysroot, port_prefix, recipes, port, target_os, mingw_triple, args.use_crt_shell, args.configure_only)

    progress(f"ports installed: {port_prefix}")


if __name__ == "__main__":
    main()
