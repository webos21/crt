#!/usr/bin/env python3
"""Fetch a pinned Wayland core commit for libcrtgfx's own external Meson
build (tools/build_wayland.py). Mirrors tools/fetch_skia.py's own shape
(clone at a pinned ref, verify the resulting commit, write a manifest so a
repeat invocation with the same repo/ref is a cheap no-op) trimmed to what
Wayland's own, much smaller repository actually needs: no cone-mode sparse
checkout (libcrtgfx/third_party/wayland/recipe.json's own notes confirm the
whole repo is only a few MB, unlike Skia's own 260MB+ untrimmed tree) and no
third-party dependency-sync step (Wayland has no DEPS-style mechanism at
all, unlike Skia's git-sync-deps).
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
    # A pinned ref is a raw commit SHA (see recipe.json's own notes on why:
    # never a floating branch/tag), and `git clone --branch <sha>` does not
    # work against most git hosts, gitlab.freedesktop.org included --
    # matching exactly the reasoning tools/fetch_skia.py's own clone_ref()
    # already documents for skia.googlesource.com's Gerrit/JGit backend.
    # Clone the branch tip shallowly first (gives git *a* history to graft
    # onto), then fetch the specific pinned commit and detach onto it.
    run(["git", "clone", "--depth", "1", repo, str(dest)])
    run(["git", "fetch", "--depth", "1", "origin", ref], cwd=dest)
    run(["git", "checkout", "--detach", "FETCH_HEAD"], cwd=dest)


def main():
    parser = argparse.ArgumentParser(description="Fetch a pinned Wayland core commit for libcrtgfx.")
    parser.add_argument("--dest", required=True, help="destination Wayland source directory")
    parser.add_argument("--repo", default="https://gitlab.freedesktop.org/wayland/wayland.git")
    parser.add_argument("--version", default="1.26.0", help="human-readable version label (informational only)")
    parser.add_argument("--ref", required=True, help="explicit git ref or commit to fetch")
    parser.add_argument("--expected-commit", default="", help="optional full commit hash to verify")
    parser.add_argument("--force", action="store_true", help="replace an existing destination")
    args = parser.parse_args()

    dest = Path(args.dest).resolve()
    manifest_path = dest / ".crt-wayland-fetch.json"

    if dest.exists():
        if args.force:
            shutil.rmtree(dest)
        elif manifest_path.exists():
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            if manifest.get("repo") == args.repo and manifest.get("requested_ref") == args.ref:
                commit = checked_output(["git", "rev-parse", "HEAD"], cwd=dest)
                if args.expected_commit and commit != args.expected_commit:
                    raise SystemExit(
                        f"Wayland commit mismatch in existing checkout\n"
                        f"expected {args.expected_commit}\n"
                        f"actual   {commit}"
                    )
                print(f"Wayland already fetched: {dest} ({commit})")
                return
            raise SystemExit(
                f"{dest} already contains a different Wayland fetch "
                "(repo/ref changed). Pass --force to replace it."
            )
        else:
            raise SystemExit(f"{dest} already exists and is not managed by fetch_wayland.py")

    dest.parent.mkdir(parents=True, exist_ok=True)
    clone_ref(args.repo, args.ref, dest)
    commit = checked_output(["git", "rev-parse", "HEAD"], cwd=dest)
    if args.expected_commit and commit != args.expected_commit:
        raise SystemExit(
            f"Wayland commit mismatch\nexpected {args.expected_commit}\nactual   {commit}"
        )

    manifest = {
        "repo": args.repo,
        "version": args.version,
        "requested_ref": args.ref,
        "commit": commit,
    }
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Wayland fetched: {dest} ({commit})")


if __name__ == "__main__":
    main()
