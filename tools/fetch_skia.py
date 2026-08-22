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


def clone_ref(repo, ref, dest, sparse_paths):
    if sparse_paths:
        # A real partial clone (--filter=blob:none, defers file *content*
        # until checkout) plus cone-mode sparse-checkout, matching the
        # same mechanism libstdc++/third_party/{libcxx,libcxxabi,
        # libunwind}/recipe.json's own tools/crt-libcxx-build.py already
        # uses -- see that file's own recipe-schema comment for the full
        # story on why this needs to fetch a ref *without* --branch (a
        # pinned ref is normally a raw commit SHA, and `git clone
        # --branch <sha>` does not work against Skia's own git host,
        # skia.googlesource.com, the same Gerrit/JGit backend as AOSP's
        # android.googlesource.com -- confirmed for real, identical
        # failure: "Remote branch <sha> not found in upstream origin").
        # Confirmed for real (2026-08-21) that this project's own default
        # CPU-raster-only GN config (tools/build_skia.py's own
        # default_gn_args()) needs no third_party/externals content at
        # all once skia_use_wuffs is also disabled (see that file's own
        # comment) -- `git-sync-deps` was tried once for real and found
        # to unconditionally download unrelated multi-gigabyte content
        # (an entire Emscripten/WASM toolchain, 8.6GB and counting before
        # it was killed) regardless of which GN features are actually
        # enabled; it is deliberately never invoked by this sparse path.
        #
        # --depth 1 on this initial clone matters and must stay: a real
        # mistake, caught the hard way (2026-08-21), first dropped it here
        # by mistake (an interactive test that validated this exact
        # command sequence had actually kept --depth 1; the flag was lost
        # transcribing that test into this file). Without it, this clones
        # Skia's ENTIRE commit/tree history (still blobless, but every
        # commit and tree object all the way back) before the real ref
        # fetch below narrows anything -- confirmed for real: a `crtgfx-
        # skia-fetch` run without --depth 1 produced a 189MB .git
        # (464,512 packed objects) versus ~11MB with it. --filter=
        # blob:none alone only defers file *content*, never trims the
        # *commit graph* itself -- the identical lesson already learned
        # once for libstdc++/third_party/*/recipe.json's own fetch (see
        # tools/crt-libcxx-build.py's matching comment).
        run(
            ["git", "clone", "--filter=blob:none", "--no-checkout", "--depth", "1", repo, str(dest)],
            cwd=None,
        )
        run(["git", "sparse-checkout", "init", "--cone"], cwd=dest)
        run(["git", "sparse-checkout", "set"] + sparse_paths, cwd=dest)
        run(["git", "fetch", "--depth", "1", "origin", ref], cwd=dest)
        run(["git", "checkout", "--detach", "FETCH_HEAD"], cwd=dest)
        return

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
    parser.add_argument(
        "--sparse-path", action="append", default=[],
        help="repo-relative directory to check out (cone mode); repeatable. "
             "Omit for a full (non-sparse) checkout.",
    )
    parser.add_argument("--sync-deps", action="store_true", help="run Skia tools/git-sync-deps after clone")
    parser.add_argument("--force", action="store_true", help="replace an existing destination")
    args = parser.parse_args()

    dest = Path(args.dest).resolve()
    ref = normalize_ref(args.version, args.ref)
    sparse_paths = sorted(args.sparse_path)
    if args.sync_deps and sparse_paths and "tools" not in sparse_paths:
        # tools/git-sync-deps is the script --sync-deps itself needs to
        # run -- confirmed for real (2026-08-21) that combining a sparse
        # checkout with --sync-deps otherwise fails with a confusing
        # "Skia dependency sync script not found" (sync_deps() below
        # raises SystemExit on a plain missing-file check, giving no hint
        # that the real cause is the sparse-checkout's own directory
        # list, not a broken clone). CRTGFX_SKIA_SPARSE_PATHS' own default
        # (libcrtgfx/CMakeLists.txt) deliberately omits "tools" -- it is
        # sized for the common case, CRTGFX_SKIA_SYNC_DEPS=OFF -- so this
        # widens the *actual* sparse-checkout automatically the moment
        # --sync-deps is requested (whether via a CRTGFX_SKIA_SYNC_DEPS=ON
        # override or a direct fetch_skia.py --sync-deps invocation)
        # instead of requiring the caller to separately remember to add
        # "tools" to CRTGFX_SKIA_SPARSE_PATHS too. Sorted again so the
        # manifest's own sparse_paths comparison below (an unrelated
        # ordering-only check) still behaves deterministically.
        sparse_paths = sorted(sparse_paths + ["tools"])
    manifest_path = dest / ".crt-skia-fetch.json"

    if dest.exists():
        if args.force:
            shutil.rmtree(dest)
        elif manifest_path.exists():
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            if (
                manifest.get("repo") == args.repo
                and manifest.get("requested_ref") == ref
                and manifest.get("sparse_paths", []) == sparse_paths
            ):
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
                f"{dest} already contains a different Skia fetch "
                "(repo/ref/sparse_paths changed). Pass --force to replace it."
            )
        else:
            raise SystemExit(f"{dest} already exists and is not managed by fetch_skia.py")

    dest.parent.mkdir(parents=True, exist_ok=True)
    clone_ref(args.repo, ref, dest, sparse_paths)
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
        "sparse_paths": sparse_paths,
        "deps_synced": bool(args.sync_deps),
    }
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Skia fetched: {dest} ({commit})")


if __name__ == "__main__":
    main()
