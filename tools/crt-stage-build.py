#!/usr/bin/env python3
"""Fetch a pinned CRT source-stage asset and build it with a predecessor SDK."""

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tarfile
import urllib.parse
import urllib.request
from pathlib import Path, PurePosixPath


def load_recipe(location: str) -> tuple[dict, str]:
    parsed = urllib.parse.urlparse(location)
    if parsed.scheme in ("http", "https", "file"):
        with urllib.request.urlopen(location) as response:
            return json.load(response), location
    path = Path(location).resolve()
    return json.loads(path.read_text(encoding="utf-8")), path.as_uri()


def validate_recipe(recipe: dict) -> None:
    required = ("schema_version", "input_stage", "output_stage", "source", "build")
    if (not isinstance(recipe, dict) or any(key not in recipe for key in required) or
            recipe["schema_version"] != 1 or
            not isinstance(recipe["input_stage"], str) or
            not re.fullmatch(r"[0-9]{2}-[a-z0-9-]+", recipe["input_stage"]) or
            not isinstance(recipe["output_stage"], str) or
            not re.fullmatch(r"[0-9]{2}-[a-z0-9-]+", recipe["output_stage"]) or
            not isinstance(recipe["source"], dict) or
            not isinstance(recipe["build"], dict)):
        raise SystemExit("invalid or unsupported CRT stage recipe")
    source = recipe["source"]
    archive = source.get("archive", "")
    archive_path = PurePosixPath(archive) if isinstance(archive, str) else None
    if (not isinstance(source.get("size"), int) or source["size"] <= 0 or
            not re.fullmatch(r"[0-9a-f]{64}", source.get("sha256", "")) or
            not re.fullmatch(r"[0-9a-f]{40}", source.get("source_commit", "")) or
            not source.get("url") or
            archive_path is None or archive_path.name != archive or
            "\\" in archive or not archive.endswith(".tar.xz")):
        raise SystemExit("stage recipe source identity is incomplete")
    entrypoint = recipe["build"].get("entrypoint", "")
    entrypoint_path = PurePosixPath(entrypoint) if isinstance(entrypoint, str) else None
    if (entrypoint_path is None or not entrypoint.endswith(".py") or
            entrypoint_path.is_absolute() or ".." in entrypoint_path.parts or
            "\\" in entrypoint):
        raise SystemExit("stage recipe build entrypoint is missing")


def fetch(url: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = destination.with_suffix(destination.suffix + ".part")
    with urllib.request.urlopen(url) as response, partial.open("wb") as output:
        shutil.copyfileobj(response, output)
    partial.replace(destination)


def verify(path: Path, source: dict) -> None:
    if path.stat().st_size != source["size"]:
        raise SystemExit(f"stage source size mismatch: expected {source['size']}, got {path.stat().st_size}")
    digest_builder = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest_builder.update(chunk)
    digest = digest_builder.hexdigest()
    if digest != source["sha256"]:
        raise SystemExit(f"stage source SHA-256 mismatch: expected {source['sha256']}, got {digest}")


def safe_extract(archive: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    root = destination.resolve()
    with tarfile.open(archive, "r:xz") as tar:
        for member in tar.getmembers():
            target = (destination / member.name).resolve()
            try:
                target.relative_to(root)
            except ValueError as exc:
                raise SystemExit(f"unsafe stage archive member: {member.name}") from exc
            if not (member.isdir() or member.isfile()):
                raise SystemExit(f"stage archive member type is not accepted: {member.name}")
        tar.extractall(destination)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--recipe", required=True)
    parser.add_argument("--work-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--cache", type=Path)
    parser.add_argument("--asset", type=Path, help="local asset override; still checked against the recipe")
    args = parser.parse_args()

    sdk_root = args.sdk_root.resolve()
    manifest_path = sdk_root / "manifest.json"
    if not manifest_path.is_file():
        raise SystemExit(f"predecessor SDK manifest is missing: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    recipe, recipe_location = load_recipe(args.recipe)
    validate_recipe(recipe)
    if manifest.get("stage") != recipe["input_stage"]:
        raise SystemExit(f"recipe requires {recipe['input_stage']}, SDK is {manifest.get('stage')}")

    work_root = args.work_root.resolve()
    cache = (args.cache.resolve() if args.cache else work_root / "downloads")
    archive = args.asset.resolve() if args.asset else cache / recipe["source"]["archive"]
    if not archive.is_file():
        fetch(recipe["source"]["url"], archive)
    verify(archive, recipe["source"])

    source_dir = work_root / "source" / recipe["output_stage"]
    if source_dir.exists():
        shutil.rmtree(source_dir)
    safe_extract(archive, source_dir)
    stage_metadata = json.loads((source_dir / "stage.json").read_text(encoding="utf-8"))
    if (stage_metadata.get("stage") != recipe["output_stage"] or
            stage_metadata.get("source_commit") != recipe["source"]["source_commit"]):
        raise SystemExit("stage archive metadata does not match its recipe")
    entrypoint = (source_dir / recipe["build"]["entrypoint"]).resolve()
    try:
        entrypoint.relative_to(source_dir.resolve())
    except ValueError as exc:
        raise SystemExit("stage entrypoint escapes the extracted source") from exc
    if not entrypoint.is_file():
        raise SystemExit(f"stage entrypoint is missing: {entrypoint}")
    subprocess.run([
        sys.executable, str(entrypoint),
        "--sdk-root", str(sdk_root),
        "--asset-root", str(source_dir),
        "--work-root", str(work_root / "build" / recipe["output_stage"]),
        "--output", str(args.output.resolve()),
        "--recipe-location", recipe_location,
        "--source-sha256", recipe["source"]["sha256"],
    ], check=True)


if __name__ == "__main__":
    main()
