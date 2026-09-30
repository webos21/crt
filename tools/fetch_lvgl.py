#!/usr/bin/env python3
"""Fetch the pinned LVGL release for crtui and extract only what the build needs.

The pin lives in libcrtui/third_party/lvgl/recipe.json (tag, commit, archive
SHA-256 and size). This tool downloads the GitHub tag archive (or reuses one
from --cache), verifies size and SHA-256, verifies that the commit recorded in
the archive's pax header equals expected_commit, and extracts only src/,
include/, LICENCE.txt and lv_conf_template.h into --dest/<source_dir>. The full
archive is ~111 MB because it carries tests/docs/examples/demos; none of that is
extracted. Nothing in the extracted tree is ever patched.
"""

import argparse
import hashlib
import json
import shutil
import sys
import tarfile
import time
import urllib.request
from pathlib import Path

KEEP_TOP_LEVEL = ("src", "include")
KEEP_FILES = ("LICENCE.txt", "lv_conf_template.h", "lvgl.h", "lvgl_private.h", "lv_version.h")


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download(url: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = destination.with_suffix(destination.suffix + ".part")
    last_error = None
    for attempt in range(1, 6):
        try:
            print(f"fetch {url} (attempt {attempt}/5)", flush=True)
            with urllib.request.urlopen(url, timeout=120) as response, partial.open("wb") as output:
                shutil.copyfileobj(response, output)
            partial.replace(destination)
            return
        except OSError as error:
            last_error = error
            print(f"  {error}; retrying", flush=True)
            time.sleep(5 * attempt)
    raise SystemExit(f"could not download {url}: {last_error}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", required=True, type=Path)
    parser.add_argument("--dest", required=True, type=Path)
    parser.add_argument("--cache", type=Path, help="directory holding/receiving the downloaded archive")
    args = parser.parse_args()

    recipe = json.loads(args.recipe.read_text(encoding="utf-8"))
    source = recipe["source"]
    source_dir = source["source_dir"]
    target = args.dest / source_dir
    stamp = target / ".crt-lvgl-source.json"
    wanted = {"archive_sha256": source["archive_sha256"], "expected_commit": source["expected_commit"],
              "keep": [*KEEP_TOP_LEVEL, *KEEP_FILES]}
    if stamp.is_file() and json.loads(stamp.read_text(encoding="utf-8")) == wanted and \
            (target / "include" / "lvgl" / "lvgl.h").is_file():
        print(f"LVGL {recipe['version']} already extracted at {target}")
        return

    cache = args.cache or (args.dest / ".downloads")
    archive = cache / f"lvgl-{recipe['version']}.tar.gz"
    if not (archive.is_file() and archive.stat().st_size == source["archive_size"]
            and sha256_of(archive) == source["archive_sha256"]):
        download(source["archive_url"], archive)
    if archive.stat().st_size != source["archive_size"]:
        raise SystemExit(f"LVGL archive size mismatch: expected {source['archive_size']}, "
                         f"got {archive.stat().st_size}")
    actual = sha256_of(archive)
    if actual != source["archive_sha256"]:
        raise SystemExit(f"LVGL archive SHA-256 mismatch: expected {source['archive_sha256']}, got {actual}")

    if target.exists():
        shutil.rmtree(target)
    args.dest.mkdir(parents=True, exist_ok=True)
    extracted = 0
    with tarfile.open(archive, "r:gz") as tar:
        commit = tar.pax_headers.get("comment")
        if commit != source["expected_commit"]:
            raise SystemExit(f"LVGL archive commit mismatch: expected {source['expected_commit']}, got {commit}")
        root = Path(args.dest).resolve()
        for member in tar:
            parts = Path(member.name).parts
            if len(parts) < 2 or parts[0] != source_dir:
                continue
            relative = parts[1:]
            if relative[0] not in KEEP_TOP_LEVEL and not (len(relative) == 1 and relative[0] in KEEP_FILES):
                continue
            destination = (args.dest / member.name).resolve()
            try:
                destination.relative_to(root)
            except ValueError as error:
                raise SystemExit(f"unsafe archive member: {member.name}") from error
            if member.isdir():
                destination.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                destination.parent.mkdir(parents=True, exist_ok=True)
                with tar.extractfile(member) as source_file, destination.open("wb") as output:
                    shutil.copyfileobj(source_file, output)
                extracted += 1
            else:
                raise SystemExit(f"unsupported archive member type: {member.name}")
    if not (target / "include" / "lvgl" / "lvgl.h").is_file() or not (target / "LICENCE.txt").is_file():
        raise SystemExit("extracted LVGL tree is incomplete")
    stamp.write_text(json.dumps(wanted, indent=2) + "\n", encoding="utf-8")
    print(f"LVGL {recipe['version']}: extracted {extracted} files to {target}")


if __name__ == "__main__":
    sys.exit(main())
