#!/usr/bin/env python3
"""Create a reproducible CRT source-stage asset and its pinned recipe."""

import argparse
import hashlib
import io
import json
import lzma
import stat
import subprocess
import tarfile
from pathlib import Path, PurePosixPath


STAGES = {
    "02-cxx": {
        "input_stage": "01-c",
        "entrypoint": "tools/build_stage_02_cxx.py",
        "project_paths": (
            "LICENSE.md",
            "tools/build_stage_02_cxx.py",
            "tools/crt-libcxx-build.py",
            "tools/install_libcxx_runtimes.py",
            "tools/test_libcxx_runtime.py",
            "tools/verify_dist.py",
            "tools/crt-cc",
            "tools/crt-c++",
            "tools/crt-cc.cmd",
            "tools/crt-c++.cmd",
            "libstdc++/third_party",
            "libstdc++/tests/imported_libcxx_test.cc",
            "libstdc++/tests/imported_libcxx_string_sdk.cc",
            "libstdc++/tests/imported_libcxx_string_abi_test.cc",
        ),
        "source_paths": ("libcxx", "libcxxabi", "libunwind", "cmake", "runtimes", "libc"),
    },
}


def git(root: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", *args], cwd=root, text=True, capture_output=True, check=True
    )
    return result.stdout.strip()


def add_path(tar: tarfile.TarFile, source: Path, archive: PurePosixPath) -> None:
    """Add one tree with stable ordering and metadata."""
    paths = [source]
    if source.is_dir():
        paths.extend(sorted(source.rglob("*"), key=lambda p: p.as_posix()))
    for path in paths:
        relative = path.relative_to(source)
        name = archive if relative == Path(".") else archive / PurePosixPath(relative.as_posix())
        info = tarfile.TarInfo(name.as_posix())
        info.uid = info.gid = 0
        info.uname = info.gname = ""
        info.mtime = 0
        if path.is_symlink():
            raise SystemExit(f"stage source packages do not accept symlinks: {path}")
        if path.is_dir():
            info.type = tarfile.DIRTYPE
            info.mode = 0o755
            tar.addfile(info)
        elif path.is_file():
            info.size = path.stat().st_size
            executable = bool(path.stat().st_mode & stat.S_IXUSR) or path.suffix in (".sh",)
            info.mode = 0o755 if executable else 0o644
            with path.open("rb") as stream:
                tar.addfile(info, stream)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--stage", required=True, choices=sorted(STAGES))
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--release-tag", required=True)
    parser.add_argument("--repository-url", default="https://github.com/webos21/crt")
    parser.add_argument("--allow-dirty", action="store_true")
    args = parser.parse_args()

    root = args.root.resolve()
    source_root = args.source_root.resolve()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    spec = STAGES[args.stage]
    commit = git(root, "rev-parse", "HEAD")
    if not args.allow_dirty and git(root, "status", "--porcelain"):
        raise SystemExit("refusing to create a release source asset from a dirty worktree; use --allow-dirty only for local testing")

    missing = [path for path in spec["project_paths"] if not (root / path).exists()]
    missing += [f"sources/{path}" for path in spec["source_paths"] if not (source_root / path).exists()]
    if missing:
        raise SystemExit("stage source inputs are missing: " + ", ".join(missing))

    asset_name = f"crt-{args.release_tag}-{args.stage}-source.tar.xz"
    asset = output_dir / asset_name
    metadata = {
        "format": 1,
        "stage": args.stage,
        "input_stage": spec["input_stage"],
        "source_commit": commit,
        "entrypoint": spec["entrypoint"],
    }
    with lzma.open(asset, "wb", preset=9) as compressed:
        with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as tar:
            payload = (json.dumps(metadata, indent=2, sort_keys=True) + "\n").encode("utf-8")
            info = tarfile.TarInfo("stage.json")
            info.size = len(payload)
            info.mode = 0o644
            info.mtime = 0
            tar.addfile(info, io.BytesIO(payload))
            for relative in spec["project_paths"]:
                add_path(tar, root / relative, PurePosixPath(relative))
            for relative in spec["source_paths"]:
                add_path(tar, source_root / relative, PurePosixPath("sources") / relative)

    digest = hashlib.sha256(asset.read_bytes()).hexdigest()
    recipe = {
        "schema_version": 1,
        "input_stage": spec["input_stage"],
        "output_stage": args.stage,
        "source": {
            "url": f"{args.repository_url.rstrip('/')}/releases/download/{args.release_tag}/{asset_name}",
            "archive": asset_name,
            "size": asset.stat().st_size,
            "sha256": digest,
            "source_commit": commit,
        },
        "build": {"entrypoint": spec["entrypoint"]},
    }
    recipe_path = output_dir / f"{args.stage}.json"
    recipe_path.write_text(json.dumps(recipe, indent=2) + "\n", encoding="utf-8")
    print(f"CRT stage source asset: {asset}")
    print(f"CRT stage recipe: {recipe_path}")
    print(f"SHA-256: {digest}")


if __name__ == "__main__":
    main()
