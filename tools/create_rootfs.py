#!/usr/bin/env python3
import argparse
import filecmp
import os
import shutil
from pathlib import Path


TOYBOX_APPLETS = [
    "[",
    "basename",
    "cat",
    "chmod",
    "cksum",
    "cp",
    "crc32",
    "date",
    "dirname",
    "echo",
    "egrep",
    "expr",
    "false",
    "fgrep",
    "grep",
    "head",
    "id",
    "ln",
    "ls",
    "mkdir",
    "mktemp",
    "mv",
    "pwd",
    "printf",
    "readlink",
    "rm",
    "rmdir",
    "sed",
    "sleep",
    "sort",
    "stat",
    "tail",
    "tee",
    "test",
    "touch",
    "tr",
    "true",
    "tsort",
    "tty",
    "uname",
    "uniq",
    "unlink",
    "uuencode",
    "wc",
    "which",
    "xargs",
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


def progress(message):
    print(f"[rootfs] {message}", flush=True)


def copy_file(src: Path, dst: Path, label: str, quiet=False):
    if dst.exists() and filecmp.cmp(src, dst, shallow=False):
        if not quiet:
            progress(f"skip unchanged {label}: {dst}")
        return
    if not quiet:
        progress(f"install {label}: {dst}")
    try:
        shutil.copy2(src, dst)
    except PermissionError as exc:
        if os.name == "nt":
            raise SystemExit(
                f"[rootfs] failed to install {label}: {dst}\n"
                f"[rootfs] Windows reports the destination is in use. Close any "
                f"running rootfs shell/tool process such as mksh.exe, sh.exe, "
                f"toybox.exe, or applet .exe under this rootfs, then rerun the "
                f"CMake target.\n"
                f"[rootfs] source: {src}\n"
                f"[rootfs] error: {exc}"
            ) from exc
        raise


def install_alias(target: Path, link: Path, target_os: str, quiet=False):
    if target_os == "windows":
        copy_file(target, link, "alias", quiet)
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
    parser.add_argument("--awk")
    args = parser.parse_args()

    rootfs = Path(args.dest).resolve()
    progress(f"create {rootfs}")
    for entry in ROOTFS_DIRS:
        (rootfs / entry).mkdir(parents=True, exist_ok=True)

    progress("write README.txt")
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
            copy_file(shell_source, rootfs / shell_dir / tiny_name, tiny_name)
        if args.target_os == "windows":
            for shell_dir in ("system/bin", "bin", "usr/bin"):
                copy_file(shell_source, rootfs / shell_dir / "tiny-sh", "tiny-sh")
    if args.mksh:
        mksh_source = Path(args.mksh).resolve()
        mksh_name = "mksh.exe" if args.target_os == "windows" else "mksh"
        mksh_dest = rootfs / "system" / "bin" / mksh_name
        copy_file(mksh_source, mksh_dest, mksh_name)
        shell_name = "sh.exe" if args.target_os == "windows" else "sh"
        for shell_dir in ("system/bin", "bin", "usr/bin"):
            install_alias(mksh_dest, rootfs / shell_dir / shell_name, args.target_os)
        if args.target_os == "windows":
            copy_file(mksh_source, rootfs / "system" / "bin" / "mksh", "mksh")
            for shell_dir in ("system/bin", "bin", "usr/bin"):
                copy_file(mksh_source, rootfs / shell_dir / "sh", "sh")
    if args.mkshrc:
        copy_file(Path(args.mkshrc).resolve(), rootfs / "system" / "etc" / "mkshrc", "mkshrc")
    if args.toybox:
        toybox_source = Path(args.toybox).resolve()
        toybox_name = "toybox.exe" if args.target_os == "windows" else "toybox"
        toybox_dest = rootfs / "system" / "bin" / toybox_name
        copy_file(toybox_source, toybox_dest, toybox_name)
        if args.target_os == "windows":
            copy_file(toybox_source, rootfs / "system" / "bin" / "toybox", "toybox")
        progress(f"install toybox applet aliases: {len(TOYBOX_APPLETS)} applets")
        for applet in TOYBOX_APPLETS:
            applet_name = f"{applet}.exe" if args.target_os == "windows" else applet
            for applet_dir in ("system/bin", "bin", "usr/bin"):
                install_alias(toybox_dest, rootfs / applet_dir / applet_name, args.target_os, quiet=True)
                if args.target_os == "windows":
                    copy_file(toybox_source, rootfs / applet_dir / applet, applet, quiet=True)
    if args.awk:
        awk_source = Path(args.awk).resolve()
        awk_name = "awk.exe" if args.target_os == "windows" else "awk"
        awk_dest = rootfs / "system" / "bin" / awk_name
        copy_file(awk_source, awk_dest, awk_name)
        for awk_dir in ("bin", "usr/bin"):
            install_alias(awk_dest, rootfs / awk_dir / awk_name, args.target_os, quiet=True)
            if args.target_os == "windows":
                copy_file(awk_source, rootfs / awk_dir / "awk", "awk", quiet=True)
    progress(f"done {rootfs}")
    print(f"CRT_ROOTFS={rootfs}", flush=True)


if __name__ == "__main__":
    main()
