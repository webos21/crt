#!/usr/bin/env python3
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


def normalize_ref(version, ref):
    if ref:
        return ref
    if version.startswith("refs/"):
        return version
    if version.startswith("chrome/"):
        return f"refs/heads/{version}"
    if version.startswith("m") and version[1:].isdigit():
        return f"refs/heads/chrome/{version}"
    return version


def clone_ref(repo, ref, dest):
    if ref.startswith("refs/heads/"):
        branch = ref.removeprefix("refs/heads/")
        run(["git", "clone", "--depth", "1", "--branch", branch, repo, str(dest)])
        return

    run(["git", "clone", "--depth", "1", repo, str(dest)])
    run(["git", "fetch", "--depth", "1", "origin", ref], cwd=dest)
    run(["git", "checkout", "--detach", "FETCH_HEAD"], cwd=dest)


def sync_deps(dest):
    script = dest / "tools" / "git-sync-deps"
    if not script.exists():
        raise SystemExit(f"Skia dependency sync script not found: {script}")
    run(["python3", str(script)], cwd=dest)


def main():
    parser = argparse.ArgumentParser(description="Fetch a Skia milestone/ref for libcrtgfx.")
    parser.add_argument("--dest", required=True, help="destination Skia source directory")
    parser.add_argument("--repo", default="https://skia.googlesource.com/skia.git")
    parser.add_argument("--version", default="m148", help="Skia milestone shorthand, such as m148")
    parser.add_argument("--ref", default="", help="explicit git ref or commit; overrides --version")
    parser.add_argument("--expected-commit", default="", help="optional full commit hash to verify")
    parser.add_argument("--sync-deps", action="store_true", help="run Skia tools/git-sync-deps after clone")
    parser.add_argument("--force", action="store_true", help="replace an existing destination")
    args = parser.parse_args()

    dest = Path(args.dest).resolve()
    ref = normalize_ref(args.version, args.ref)
    manifest_path = dest / ".crt-skia-fetch.json"

    if dest.exists():
        if args.force:
            shutil.rmtree(dest)
        elif manifest_path.exists():
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            if manifest.get("repo") == args.repo and manifest.get("requested_ref") == ref:
                commit = checked_output(["git", "rev-parse", "HEAD"], cwd=dest)
                if args.expected_commit and commit != args.expected_commit:
                    raise SystemExit(
                        f"Skia commit mismatch in existing checkout\n"
                        f"expected {args.expected_commit}\n"
                        f"actual   {commit}"
                    )
                print(f"Skia already fetched: {dest} ({commit})")
                return
            raise SystemExit(
                f"{dest} already contains a different Skia fetch. "
                "Pass --force to replace it."
            )
        else:
            raise SystemExit(f"{dest} already exists and is not managed by fetch_skia.py")

    dest.parent.mkdir(parents=True, exist_ok=True)
    clone_ref(args.repo, ref, dest)
    commit = checked_output(["git", "rev-parse", "HEAD"], cwd=dest)
    if args.expected_commit and commit != args.expected_commit:
        raise SystemExit(
            f"Skia commit mismatch\nexpected {args.expected_commit}\nactual   {commit}"
        )
    if args.sync_deps:
        sync_deps(dest)

    manifest = {
        "repo": args.repo,
        "version": args.version,
        "requested_ref": ref,
        "commit": commit,
        "deps_synced": bool(args.sync_deps),
    }
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Skia fetched: {dest} ({commit})")


if __name__ == "__main__":
    main()
