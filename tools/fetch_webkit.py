#!/usr/bin/env python3
"""Download and verify the pinned WPE WebKit release tarball for CRT's 06-web stage.

The pin lives in libcrtweb/third_party/webkit/recipe.json (version, archive URL, size and
SHA-256, plus the signed tag and commit it was cut from). This tool downloads the
archive into --cache (or reuses one that already matches), verifies size and
SHA-256, and prints the digest it computed. It deliberately extracts nothing:
Web Tranche 0 only freezes and verifies the pin; extraction of the paths each
tranche needs arrives with Tranche 1 (JavaScriptCore).

Exit status is non-zero on any mismatch, so a changed or truncated archive can
never be mistaken for the pinned one. Nothing downloaded is ever patched.
"""

import argparse
import hashlib
import json
import shutil
import sys
import time
import urllib.request
from pathlib import Path

REQUIRED_SOURCE_FIELDS = (
    "archive_url", "archive_name", "archive_size", "archive_sha256",
    "tag", "expected_commit",
)


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_recipe(recipe: dict) -> dict:
    """Return recipe["source"] after checking the fields this tool relies on."""
    if recipe.get("schema_version") != 1 or recipe.get("name") != "wpewebkit":
        raise SystemExit("not a wpewebkit pin recipe (schema_version 1)")
    source = recipe.get("source")
    if not isinstance(source, dict):
        raise SystemExit("recipe has no source object")
    missing = [field for field in REQUIRED_SOURCE_FIELDS if field not in source]
    if missing:
        raise SystemExit("recipe source is missing: " + ", ".join(missing))
    digest = source["archive_sha256"]
    if not (isinstance(digest, str) and len(digest) == 64 and
            all(c in "0123456789abcdef" for c in digest)):
        raise SystemExit("archive_sha256 must be 64 lowercase hex characters")
    commit = source["expected_commit"]
    if not (isinstance(commit, str) and len(commit) == 40 and
            all(c in "0123456789abcdef" for c in commit)):
        raise SystemExit("expected_commit must be a 40-character commit id")
    if not (isinstance(source["archive_size"], int) and source["archive_size"] > 0):
        raise SystemExit("archive_size must be a positive integer")
    name = source["archive_name"]
    if "/" in name or "\\" in name or not name.endswith(".tar.xz"):
        raise SystemExit("archive_name must be a plain .tar.xz file name")
    if recipe.get("version") and recipe["version"] not in name:
        raise SystemExit("archive_name does not contain the recipe version")
    return source


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


def verify(archive: Path, source: dict) -> str:
    """Check size then SHA-256; return the computed digest."""
    size = archive.stat().st_size
    if size != source["archive_size"]:
        raise SystemExit(
            f"WPE WebKit archive size mismatch: expected {source['archive_size']}, got {size}")
    actual = sha256_of(archive)
    if actual != source["archive_sha256"]:
        raise SystemExit(
            f"WPE WebKit archive SHA-256 mismatch: expected {source['archive_sha256']}, got {actual}")
    return actual


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", required=True, type=Path)
    parser.add_argument("--cache", required=True, type=Path,
                        help="directory holding/receiving the downloaded archive")
    parser.add_argument("--no-download", action="store_true",
                        help="verify an archive already in --cache; never touch the network")
    args = parser.parse_args()

    recipe = json.loads(args.recipe.read_text(encoding="utf-8"))
    source = validate_recipe(recipe)
    archive = args.cache / source["archive_name"]

    if not archive.is_file():
        if args.no_download:
            raise SystemExit(f"{archive} is not present and --no-download was given")
        download(source["archive_url"], archive)
    # A cached file that does not match is evidence of a changed or truncated
    # download; it is reported, never silently fetched over.
    digest = verify(archive, source)
    print(f"WPE WebKit {recipe['version']}: {archive} verified")
    print(f"  size    {archive.stat().st_size}")
    print(f"  sha256  {digest}")
    print(f"  commit  {source['expected_commit']} (tag {source['tag']})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
