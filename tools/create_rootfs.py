#!/usr/bin/env python3
import argparse
import os
import shutil
from pathlib import Path


TOYBOX_APPLETS = [
    "[",
    "basename",
    "cat",
    "chmod",
    "cp",
    "date",
    "dirname",
    "echo",
    "expr",
    "false",
    "grep",
    "head",
    "ln",
    "ls",
    "mkdir",
    "mktemp",
    "mv",
    "pwd",
    "printf",
    "rm",
    "rmdir",
    "sed",
    "sleep",
    "sort",
    "tail",
    "tee",
    "test",
    "touch",
    "tr",
    "true",
    "uname",
    "uniq",
    "wc",
]

ROOTFS_DIRS = [
    "system/bin",
    "system/etc",
    "system/lib",
    "bin",
    "usr/bin",
    "usr/lib",
    "tmp",
    "dev",
    "proc",
    "proc/self",
    "proc/self/fd",
    "data",
    "data/local",
    "data/local/tmp",
    "home",
]


def install_alias(target: Path, link: Path, target_os: str):
    if target_os == "windows":
        shutil.copy2(target, link)
        return
    if link.exists() or link.is_symlink():
        link.unlink()
    link.symlink_to(os.path.relpath(target, link.parent))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dest", required=True)
    parser.add_argument("--target-os", required=True)
    parser.add_argument("--shell")
    parser.add_argument("--mksh")
    parser.add_argument("--mkshrc")
    parser.add_argument("--toybox")
    args = parser.parse_args()

    rootfs = Path(args.dest).resolve()
    for entry in ROOTFS_DIRS:
        (rootfs / entry).mkdir(parents=True, exist_ok=True)

    (rootfs / "README.txt").write_text(
        "CRT Android-like rootfs\n"
        f"target-os={args.target_os}\n"
        "\n"
        "This tree is a runtime namespace for CRT-hosted POSIX/Bionic-style\n"
        "programs. It is intentionally separate from the compiler sysroot.\n"
        "Early virtual paths include /tmp, /dev/null, /dev/tty,\n"
        "/proc/self/exe, and /proc/self/fd.\n",
        encoding="utf-8",
    )
    if args.shell:
        shell_source = Path(args.shell).resolve()
        shell_name = "sh.exe" if args.target_os == "windows" else "sh"
        tiny_name = "tiny-sh.exe" if args.target_os == "windows" else "tiny-sh"
        for shell_dir in ("system/bin", "bin", "usr/bin"):
            shutil.copy2(shell_source, rootfs / shell_dir / tiny_name)
        if args.target_os == "windows":
            for shell_dir in ("system/bin", "bin", "usr/bin"):
                shutil.copy2(shell_source, rootfs / shell_dir / "tiny-sh")
    if args.mksh:
        mksh_source = Path(args.mksh).resolve()
        mksh_name = "mksh.exe" if args.target_os == "windows" else "mksh"
        mksh_dest = rootfs / "system" / "bin" / mksh_name
        shutil.copy2(mksh_source, mksh_dest)
        shell_name = "sh.exe" if args.target_os == "windows" else "sh"
        for shell_dir in ("system/bin", "bin", "usr/bin"):
            install_alias(mksh_dest, rootfs / shell_dir / shell_name, args.target_os)
        if args.target_os == "windows":
            shutil.copy2(mksh_source, rootfs / "system" / "bin" / "mksh")
            for shell_dir in ("system/bin", "bin", "usr/bin"):
                shutil.copy2(mksh_source, rootfs / shell_dir / "sh")
    if args.mkshrc:
        shutil.copy2(Path(args.mkshrc).resolve(), rootfs / "system" / "etc" / "mkshrc")
    if args.toybox:
        toybox_source = Path(args.toybox).resolve()
        toybox_name = "toybox.exe" if args.target_os == "windows" else "toybox"
        toybox_dest = rootfs / "system" / "bin" / toybox_name
        shutil.copy2(toybox_source, toybox_dest)
        if args.target_os == "windows":
            shutil.copy2(toybox_source, rootfs / "system" / "bin" / "toybox")
        for applet in TOYBOX_APPLETS:
            applet_name = f"{applet}.exe" if args.target_os == "windows" else applet
            for applet_dir in ("system/bin", "bin", "usr/bin"):
                install_alias(toybox_dest, rootfs / applet_dir / applet_name, args.target_os)
                if args.target_os == "windows":
                    shutil.copy2(toybox_source, rootfs / applet_dir / applet)
    print(f"CRT_ROOTFS={rootfs}")


if __name__ == "__main__":
    main()
