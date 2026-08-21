#!/usr/bin/env python3
"""Compile and run the imported Android libc++ smoke against a CRT sysroot."""

import argparse
import os
import shutil
import subprocess
from pathlib import Path


def compiler_supports_flag(compiler, flag):
    if compiler is None:
        return False
    try:
        probe = subprocess.run(
            [str(compiler), "-x", "c++", "-", "-fsyntax-only", flag],
            input="int main(){return 0;}\n",
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError:
        return False
    return probe.returncode == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--sysroot", required=True, type=Path)
    parser.add_argument("--target-os", required=True, choices=["linux", "macos", "windows"])
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--rootfs", type=Path, help="Windows only: the Android-like rootfs (toybox applets, mksh)")
    parser.add_argument("--windows-sdk-libpath", type=Path, help="Windows only: dir containing kernel32.lib")
    args = parser.parse_args()

    root = args.root.resolve()
    sysroot = args.sysroot.resolve()
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    base_env = os.environ.copy()
    base_env.update(
        CRT_SYSROOT=str(sysroot),
        CRT_TARGET_OS=args.target_os,
        CRT_CXX_ENABLE_EXCEPTIONS="1",
        CRT_CXX_ENABLE_RTTI="1",
        CRT_CXX_STANDARD_INCLUDE_FLAGS=f"-isystem{sysroot / 'include' / 'c++' / 'v1'}",
    )

    wrapper = root / "tools" / "crt-c++"
    command = [str(wrapper)]
    if args.target_os == "windows":
        # Native Windows cannot execute a shebang script directly, so this
        # needs mksh.exe as a launcher -- but NOT the bare copy `sysroot`
        # stages (that copy carries mksh.exe alone, no toybox alongside
        # it). tools/crt-c++ itself shells out to toybox applets for
        # argument handling (e.g. printf) and to detect the host
        # architecture (uname -m/-s); confirmed for real, twice: first
        # "printf: inaccessible or not found" (already diagnosed in this
        # project's top-level CMakeLists.txt, near CRT_LIBCXX_PLATFORM_
        # ARGUMENTS' own --rootfs comment), then -- after routing PATH
        # through the rootfs's own system/bin -- a second, silent failure:
        # `uname -m` returning 127 under `set -eu`, which propagates and
        # kills the whole script with the exact same exit code, no error
        # text at all (mksh's own "command not found" diagnostic goes to
        # stderr, but by the time errexit fires the subshell running the
        # command substitution has already unwound). The *rootfs* --
        # ${CMAKE_BINARY_DIR}/rootfs, matching CRT_LIBCXX_PLATFORM_
        # ARGUMENTS' own --rootfs value for tools/crt-libcxx-build.py --
        # is the one tree that actually has toybox's applet aliases
        # installed (system/bin/printf, system/bin/uname, ...).
        if not args.rootfs:
            raise SystemExit("--rootfs is required when --target-os windows")
        rootfs = args.rootfs.resolve()
        mksh = rootfs / "system" / "bin" / "mksh.exe"
        if not mksh.is_file():
            raise SystemExit(f"CRT mksh is missing: {mksh}")
        # mksh's own script-loading applies the same forward-slash-only
        # path recognition as its exec() path lookup (see tools/crt-libcxx-
        # build.py's find_windows_host_tool() for the full story) -- a bare
        # backslash argv[1] like "C:\...\tools\crt-c++" (exactly what
        # str(Path) produces on Windows) gets misread as a bare command
        # name to search PATH for, not a script to open, and mksh reports
        # it "inaccessible or not found" (subprocess exit 127) even though
        # the file exists.
        command = [mksh.as_posix(), wrapper.as_posix()]
        base_env["CRT_ROOTFS"] = str(rootfs)
        # Matches tools/crt-libcxx-build.py's own common_cmake_args():
        # restrict PATH to the POSIX-rooted names mksh/toybox actually
        # resolve (real Windows PATH entries are semicolon-separated,
        # backslash-quoted -- mksh's own $PATH search expects
        # colon-separated POSIX entries and cannot parse a native Windows
        # PATH string at all, so leaving the inherited PATH in place does
        # not "additionally" help; it is simply never consulted correctly
        # either way).
        base_env["PATH"] = "/system/bin:/bin:/usr/bin"
        if args.windows_sdk_libpath:
            base_env["CRT_WINDOWS_SDK_LIBPATH"] = str(args.windows_sdk_libpath.resolve())

    host_cxx = os.environ.get("CRT_HOST_CXX") or shutil.which("clang++") or shutil.which("clang++-18") or shutil.which("c++")
    if host_cxx and args.target_os == "windows":
        # tools/crt-c++ itself falls back to a bare "clang++" (resolved via
        # mksh's own $PATH search) whenever CRT_HOST_CXX is unset -- and
        # PATH is now the restricted POSIX-rooted string set just above,
        # which has no real compiler on it at all. Resolving host_cxx via
        # shutil.which() up front (already done above, for the
        # -fno-typed-cxx-new-delete probe, using the *original* inherited
        # PATH before the restriction) and exporting it here too is the
        # same fix tools/crt-libcxx-build.py's own common_cmake_args()
        # already applies for exactly this reason. .as_posix(): same
        # forward-slash requirement as the mksh script path above --
        # shutil.which() returns a native (backslash) path on Windows, and
        # mksh would misread that exactly the same way once tools/crt-c++
        # tries to exec it.
        base_env["CRT_HOST_CXX"] = Path(host_cxx).as_posix()
    typed_new_delete_flag = []
    if compiler_supports_flag(host_cxx, "-fno-typed-cxx-new-delete"):
        typed_new_delete_flag = ["-fno-typed-cxx-new-delete"]

    for linkage in ("static", "shared"):
        env = base_env.copy()
        env["CRT_CXX_RUNTIME_LINKAGE"] = linkage
        binary = output if linkage == "shared" else output.with_name(
            f"{output.stem}_static{output.suffix}"
        )
        link_command = command + typed_new_delete_flag + [
            "-std=c++17",
            str(root / "tests" / "imported_libcxx_test.cc"),
            "-o",
            str(binary),
        ]
        print("+", " ".join(link_command), flush=True)
        subprocess.run(link_command, env=env, check=True)
        print("+", binary, flush=True)
        subprocess.run([str(binary)], env=env, check=True)


if __name__ == "__main__":
    main()
