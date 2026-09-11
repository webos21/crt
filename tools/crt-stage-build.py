#!/usr/bin/env python3
"""Fetch a pinned CRT source-stage asset and build it with a predecessor SDK."""

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tarfile
import urllib.parse
import urllib.request
from pathlib import Path

from crt_stage_recipe import validate_recipe


def load_recipe(location: str) -> tuple[dict, str]:
    parsed = urllib.parse.urlparse(location)
    if parsed.scheme in ("http", "https", "file"):
        with urllib.request.urlopen(location) as response:
            return json.load(response), location
    path = Path(location).resolve()
    return json.loads(path.read_text(encoding="utf-8")), path.as_uri()


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
    # Forwarded only if given -- not every stage entrypoint understands it
    # (currently just tools/build_stage_04_gfx_media.py; see its own
    # --reuse-work-root help text for what it does and why it exists).
    parser.add_argument("--reuse-work-root", action="store_true",
                        help="Pass through to the stage entrypoint, if it "
                             "supports keeping --work-root between runs.")
    args = parser.parse_args()

    sdk_root = args.sdk_root.resolve()
    manifest_path = sdk_root / "manifest.json"
    if not manifest_path.is_file():
        raise SystemExit(f"predecessor SDK manifest is missing: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    recipe, recipe_location = load_recipe(args.recipe)
    try:
        validate_recipe(recipe)
    except ValueError as exc:
        raise SystemExit(str(exc)) from exc
    if manifest.get("stage") != recipe["input_stage"]:
        raise SystemExit(f"recipe requires {recipe['input_stage']}, SDK is {manifest.get('stage')}")
    sdk_os = manifest.get("target", {}).get("os")
    recipe_os = recipe["target"]["os"]
    if sdk_os != recipe_os:
        raise SystemExit(f"recipe targets {recipe_os}, SDK targets {sdk_os}")

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
            stage_metadata.get("target") != recipe["target"] or
            stage_metadata.get("source_commit") != recipe["source"]["source_commit"]):
        raise SystemExit("stage archive metadata does not match its recipe")
    entrypoint = (source_dir / recipe["build"]["entrypoint"]).resolve()
    try:
        entrypoint.relative_to(source_dir.resolve())
    except ValueError as exc:
        raise SystemExit("stage entrypoint escapes the extracted source") from exc
    if not entrypoint.is_file():
        raise SystemExit(f"stage entrypoint is missing: {entrypoint}")
    command = [
        sys.executable, str(entrypoint),
        "--sdk-root", str(sdk_root),
        "--asset-root", str(source_dir),
        "--work-root", str(work_root / "build" / recipe["output_stage"]),
        "--output", str(args.output.resolve()),
        "--recipe-location", recipe_location,
        "--source-sha256", recipe["source"]["sha256"],
    ]
    if args.reuse_work_root:
        command.append("--reuse-work-root")
    subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
