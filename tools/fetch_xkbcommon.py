#!/usr/bin/env python3
"""Fetch a pinned libxkbcommon commit for libcrtgfx's own external direct-
compile build (tools/build_xkbcommon.py). Mirrors tools/fetch_wayland.py's
own shape exactly (clone at a pinned ref, verify the resulting commit,
write a manifest so a repeat invocation with the same repo/ref is a cheap
no-op) -- see that file's own docstring for the reasoning this one shares.
libxkbcommon's own repo is small (no cone-mode sparse checkout needed,
matching Wayland's own) and has no DEPS-style dependency-sync step either.
"""

import argparse
import json
import shutil
import subprocess
from pathlib import Path


def run(args, cwd=None):
    print("+", " ".join(str(arg) for arg in args), flush=True)
    subprocess.run(args, cwd=cwd, check=True)


def checked_output(args, cwd=None):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def clone_ref(repo, ref, dest):
    # See tools/fetch_wayland.py's own clone_ref() for why a pinned commit
    # SHA needs this two-step shallow-clone-then-fetch shape rather than a
    # plain `git clone --branch <sha>` (most git hosts, GitHub included,
    # reject a raw commit SHA as a --branch value).
    run(["git", "clone", "--depth", "1", repo, str(dest)])
    run(["git", "fetch", "--depth", "1", "origin", ref], cwd=dest)
    run(["git", "checkout", "--detach", "FETCH_HEAD"], cwd=dest)


def main():
    parser = argparse.ArgumentParser(description="Fetch a pinned libxkbcommon commit for libcrtgfx.")
    parser.add_argument("--dest", required=True, help="destination libxkbcommon source directory")
    parser.add_argument("--repo", default="https://github.com/xkbcommon/libxkbcommon.git")
    parser.add_argument("--version", default="1.9.2", help="human-readable version label (informational only)")
    parser.add_argument("--ref", required=True, help="explicit git ref or commit to fetch")
    parser.add_argument("--expected-commit", default="", help="optional full commit hash to verify")
    parser.add_argument("--force", action="store_true", help="replace an existing destination")
    args = parser.parse_args()

    dest = Path(args.dest).resolve()
    manifest_path = dest / ".crt-xkbcommon-fetch.json"

    if dest.exists():
        if args.force:
            shutil.rmtree(dest)
        elif manifest_path.exists():
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            if manifest.get("repo") == args.repo and manifest.get("requested_ref") == args.ref:
                commit = checked_output(["git", "rev-parse", "HEAD"], cwd=dest)
                if args.expected_commit and commit != args.expected_commit:
                    raise SystemExit(
                        f"libxkbcommon commit mismatch in existing checkout\n"
                        f"expected {args.expected_commit}\n"
                        f"actual   {commit}"
                    )
                print(f"libxkbcommon already fetched: {dest} ({commit})")
                return
            raise SystemExit(
                f"{dest} already contains a different libxkbcommon fetch "
                "(repo/ref changed). Pass --force to replace it."
            )
        else:
            raise SystemExit(f"{dest} already exists and is not managed by fetch_xkbcommon.py")

    dest.parent.mkdir(parents=True, exist_ok=True)
    clone_ref(args.repo, args.ref, dest)
    commit = checked_output(["git", "rev-parse", "HEAD"], cwd=dest)
    if args.expected_commit and commit != args.expected_commit:
        raise SystemExit(
            f"libxkbcommon commit mismatch\nexpected {args.expected_commit}\nactual   {commit}"
        )

    manifest = {
        "repo": args.repo,
        "version": args.version,
        "requested_ref": args.ref,
        "commit": commit,
    }
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"libxkbcommon fetched: {dest} ({commit})")


if __name__ == "__main__":
    main()
