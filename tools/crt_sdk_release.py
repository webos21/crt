#!/usr/bin/env python3
"""Assemble local Development or Full SDK release assets. Never upload."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile

import crt_dev_profile as dev
import prepare_release_assets as release
from crt_web_stage import run

ROOT = Path(__file__).resolve().parent.parent


def check_pair(development, full, version):
    profile = json.loads((development / dev.SIDECAR).read_text())
    manifest = json.loads((development / "manifest.json").read_text())
    full_manifest = json.loads((full / "manifest.json").read_text())
    problems = dev.check(development, manifest)
    if problems:
        raise release.ReleaseError("; ".join(problems))
    full_commit = full_manifest.get("source_commit")
    if full_manifest.get("stage") == "06-web":
        # web-source-correspondence.source_commit is WebKit's product pin,
        # NOT the CRT producer commit. Bind the overlay recipe to the inherited
        # CRT runtime identity instead of comparing unrelated repositories.
        recipe = json.loads((full / "stages/recipes/06-web.json").read_text())
        source = recipe.get("source", {})
        if (source.get("source_commit") != full_commit
                or source.get("sha256") != full_manifest.get("built_from", {}).get("source_sha256")):
            raise release.ReleaseError("Full SDK Web overlay recipe does not match its CRT source identity")
    if (not full_commit or profile.get("producer_source_commit") != full_commit
            or profile.get("base_source_commit") != full_commit or profile.get("producer_working_tree_dirty") is not False
            or profile.get("compile_provenance") != {"source_commit": full_commit, "working_tree_dirty": False}):
        raise release.ReleaseError("Development/Full SDK need the same known clean source commit; rebuild stale predecessors")
    if manifest.get("target") != full_manifest.get("target"):
        raise release.ReleaseError("Development/Full target mismatch")
    if any((sdk / "VERSION").read_text().strip() != version for sdk in (development, full)):
        raise release.ReleaseError("Development/Full VERSION must equal the release tag")
    if dev.runtime_digests(development) != dev.runtime_digests(full):
        raise release.ReleaseError("Development/Full inherited runtime bytes differ")


def check_full_source(full, source_assets):
    """New Full collectors require known clean Web source-asset provenance.

    Existing assets remain usable for local SDK replay. Missing legacy state
    is unknown, never upgraded just because the collector's checkout is clean.
    """
    recipe = json.loads((full / "stages/recipes/06-web.json").read_text())
    source = recipe["source"]
    archive = (source_assets / source["archive"]).resolve()
    if (archive.parent != source_assets.resolve() or archive.stat().st_size != source["size"]
            or dev.digest(archive) != source["sha256"]):
        raise release.ReleaseError("Full SDK source asset does not match its pinned recipe")
    with tarfile.open(archive, "r:xz") as stream:
        member = stream.getmember("stage.json")
        if not member.isfile() or member.size > 1048576:
            raise release.ReleaseError("invalid Full SDK source metadata")
        metadata = json.loads(stream.extractfile(member).read())
    if metadata.get("source_commit") != source["source_commit"] or metadata.get("working_tree_dirty") is not False:
        raise release.ReleaseError("Full SDK source asset needs known clean creation provenance; regenerate legacy/dirty assets")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", required=True, choices=("dev", "full"))
    parser.add_argument("--version", required=True)
    parser.add_argument("--development-sdk", required=True, type=Path)
    parser.add_argument("--full-sdk", required=True, type=Path)
    parser.add_argument("--source-assets", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args(argv)
    release.validate_version(args.version)  # reject the local 'development' tag first
    dirty = subprocess.check_output(["git", "-C", str(ROOT), "status", "--porcelain"], text=True).strip()
    if dirty:
        raise release.ReleaseError("release requires a clean producer tree")
    development, full = args.development_sdk.resolve(), args.full_sdk.resolve()
    check_pair(development, full, args.version)
    commit = subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
    if json.loads((development / dev.SIDECAR).read_text())["producer_source_commit"] != commit:
        raise release.ReleaseError("release collector checkout must match the SDK producer commit")
    output = args.output_dir.resolve()
    if output.exists():
        raise release.ReleaseError("release output already exists; choose a fresh output root instead of replacing an identity")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".crt-release-", dir=output.parent) as scratch:
        args.output_dir = Path(scratch) / "assets"
        assemble(args, development, full)
        args.output_dir.rename(output)
    print("Release assets prepared locally; nothing uploaded")
    return 0


def assemble(args, development, full):
    run([sys.executable, ROOT / "tools/verify_dist.py", "--dist", development, "--stage", "02-cxx", "--profile", "dev"])
    if args.profile == "full":
        if json.loads((full / "manifest.json").read_text()).get("stage") != "06-web":
            raise release.ReleaseError("Full release requires 06-web")
        check_full_source(full, args.source_assets)
        # Canonical extraction name without modifying a replay SDK.
        with tempfile.TemporaryDirectory(prefix="crt full release ") as temp:
            canonical = Path(temp) / "06-web"
            shutil.copytree(full, canonical, symlinks=True)
            release.prepare(args.source_assets.parent / "dist", args.version, args.output_dir,
                            ("06-web",), {"06-web": canonical}, args.source_assets, False, False)
    else:
        # Reuse the ordinary recipe/asset gate without pretending dev is a
        # cumulative stage or weakening the Full 02-cxx/06-web release gates.
        _, sources = release.check_sdk(development, "02-cxx", args.version, args.source_assets)
        target = json.loads((development / "manifest.json").read_text())["target"]
        args.output_dir.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="crt dev release ") as temp:
            canonical = Path(temp) / "dev"
            shutil.copytree(development, canonical, symlinks=True)
            archive = release.archive_directory(canonical, "dev", args.version, target["os"], target["arch"], args.output_dir)
        with tempfile.TemporaryDirectory(prefix="crt dev extracted ") as temp:
            shutil.unpack_archive(str(archive), temp)
            run([sys.executable, ROOT / "tools/verify_dist.py", "--dist", Path(temp) / "dev",
                 "--stage", "02-cxx", "--profile", "dev"])
        files = [archive]
        for source in sources:
            destination = args.output_dir / source.name
            shutil.copy2(source, destination)
            files.append(destination)
        assets = [{"name": p.name, "size": p.stat().st_size, "sha256": dev.digest(p)} for p in files]
        suffix = target["os"] + "-" + target["arch"]
        (args.output_dir / ("SHA256SUMS-dev-" + suffix)).write_text(release.format_sha256sums(
            [(entry["name"], entry["sha256"]) for entry in assets]), encoding="utf-8")
        (args.output_dir / ("release-manifest-dev-" + suffix + ".json")).write_text(json.dumps({
            "profile": "dev", "version": args.version, "target": target,
            "source_commit": json.loads((development / dev.SIDECAR).read_text())["producer_source_commit"],
            "assets": assets}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (release.ReleaseError, OSError, ValueError, KeyError, tarfile.TarError) as error:
        print(f"crt-sdk-release: {error}", file=sys.stderr)
        sys.exit(1)
