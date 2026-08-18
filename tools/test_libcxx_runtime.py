#!/usr/bin/env python3
"""Compile and run the imported Android libc++ smoke against a CRT sysroot."""

import argparse
import os
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--sysroot", required=True, type=Path)
    parser.add_argument("--target-os", required=True, choices=["linux", "macos", "windows"])
    parser.add_argument("--output", required=True, type=Path)
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
    if args.target_os == "windows":
        base_env["PATH"] = str(sysroot / "bin") + os.pathsep + base_env.get("PATH", "")

    wrapper = root / "tools" / "crt-c++"
    command = [str(wrapper)]
    if args.target_os == "windows":
        # Native Windows cannot execute a shebang script directly. The CRT
        # mksh installed by `sysroot` is itself a normal PE executable and is
        # the canonical launcher for the same wrapper used by porting tests.
        mksh = sysroot / "system" / "bin" / "mksh.exe"
        if not mksh.is_file():
            raise SystemExit(f"CRT mksh is missing: {mksh}")
        command = [str(mksh), str(wrapper)]

    for linkage in ("static", "shared"):
        env = base_env.copy()
        env["CRT_CXX_RUNTIME_LINKAGE"] = linkage
        binary = output if linkage == "shared" else output.with_name(
            f"{output.stem}_static{output.suffix}"
        )
        link_command = command + [
            "-fno-typed-cxx-new-delete",
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
