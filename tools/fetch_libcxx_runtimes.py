#!/usr/bin/env python3
"""Fetch Android's LLVM C++ runtime source family without vendoring it."""

import argparse
import subprocess
from pathlib import Path


RUNTIMES = (
    ("libcxx", "https://android.googlesource.com/platform/external/libcxx"),
    ("libcxxabi", "https://android.googlesource.com/platform/external/libcxxabi"),
    ("libunwind", "https://android.googlesource.com/platform/external/libunwind"),
)


def run(args, cwd=None):
    print("+", " ".join(str(arg) for arg in args), flush=True)
    subprocess.run(args, cwd=cwd, check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--ref", default="refs/heads/main")
    parser.add_argument("--rebuild", action="store_true")
    args = parser.parse_args()

    args.root.mkdir(parents=True, exist_ok=True)
    # Android documents refs as refs/heads/<name>, while `git clone --branch`
    # expects the short branch name for a shallow branch checkout.
    clone_ref = args.ref.removeprefix("refs/heads/")
    for name, repository in RUNTIMES:
        checkout = args.root / name
        if not checkout.exists():
            run(["git", "clone", "--depth", "1", "--branch", clone_ref, repository, str(checkout)])
        elif args.rebuild:
            run(["git", "fetch", "--depth", "1", "origin", args.ref], checkout)
            run(["git", "checkout", "--detach", "FETCH_HEAD"], checkout)
        else:
            print(f"Android {name} already fetched: {checkout}")


if __name__ == "__main__":
    main()
