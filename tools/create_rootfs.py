#!/usr/bin/env python3
import argparse
import shutil
from pathlib import Path


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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dest", required=True)
    parser.add_argument("--target-os", required=True)
    parser.add_argument("--shell")
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
        shell_dest = rootfs / "system" / "bin" / ("sh.exe" if args.target_os == "windows" else "sh")
        shutil.copy2(shell_source, shell_dest)
    print(f"CRT_ROOTFS={rootfs}")


if __name__ == "__main__":
    main()
