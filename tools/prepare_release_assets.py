#!/usr/bin/env python3
"""Collect, verify, and checksum the release assets for one CRT host.

The build system already bakes the release tag into everything that carries
one (``-DCRT_RELEASE_TAG=<tag>``: SDK ``VERSION`` files, archive names, and the
stage-source recipes' download URLs).  This tool does not change any bytes; it
gathers what was built, refuses anything that is not release-grade, and writes
the files a maintainer uploads:

* the four stage SDK archives ``crt-<tag>-<os>-<arch>-<stage>.{zip,tar.xz}``
  (found in ``--dist-root``; a stage can instead be given as an extracted
  directory with ``--sdk STAGE=DIR``, which is how the isolated option-ON
  ``04-gfx-media`` build -- it emits a directory, not an archive -- is added),
* every stage-source asset the packaged recipes point at, byte-identical (a
  recipe pins the asset's size and SHA-256),
* ``SHA256SUMS-<os>-<arch>`` and ``release-manifest-<os>-<arch>.json`` (named per
  host so the outputs of the Linux, Windows, and macOS runs can share one
  GitHub release without colliding).

Checks, each one release-blocking:

1. built from a clean git tree (``--allow-dirty`` only for rehearsals);
2. the extracted SDK's ``VERSION`` equals the tag and ``verify_dist.py`` passes
   on the *extracted archive* (the published bytes, not the build directory);
3. ``04-gfx-media`` is the isolated option-ON stage with FreeType, FFmpeg, and
   Skia -- the ordinary cumulative ``dist`` target shares that label but keeps
   them OFF, and would silently omit the headline capabilities;
4. every ``stages/recipes/*.json`` points at ``/releases/download/<tag>/`` and
   its asset exists here with the recorded size and SHA-256.

Nothing is uploaded or published.  Publishing is a separate, deliberate step.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]

#: Stages published in a developer preview.  ``05-js`` is skeleton-only and is
#: deliberately not offered.
RELEASE_STAGES = ("01-c", "02-cxx", "03-gfx-simple", "04-gfx-media")

#: Dependencies a genuine option-ON 04-gfx-media must redistribute.
OPTION_ON_DEPENDENCIES = ("freetype", "ffmpeg", "skia")

#: Files whose presence separates the option-ON payload from the default-OFF
#: cumulative ``04-gfx-media`` (mirrors verify_dist.py's isolated-stage check).
OPTION_ON_FILES = (
    "lib/libskia.a",
    "lib/libfreetype.a",
    "lib/libavformat.a",
    "lib/libavcodec.a",
    "lib/libswresample.a",
    "lib/libavutil.a",
    "include/crtgfx/skia.h",
)

VERSION_PATTERN = re.compile(r"^v\d+\.\d+\.\d+(-[0-9A-Za-z][0-9A-Za-z.]*)?$")


class ReleaseError(Exception):
    """A release-blocking condition, reported without a traceback."""


def validate_version(version: str) -> str:
    if not VERSION_PATTERN.match(version):
        raise ReleaseError(
            f"invalid release version {version!r}: expected vMAJOR.MINOR.PATCH "
            "with an optional -prerelease suffix, e.g. v0.4.0-preview.1")
    return version


def archive_base_name(version: str, target_os: str, arch: str, stage: str) -> str:
    return f"crt-{version}-{target_os}-{arch}-{stage}"


def archive_suffix(target_os: str) -> str:
    return ".zip" if target_os == "windows" else ".tar.xz"


def checksum_file_name(target_os: str, arch: str) -> str:
    return f"SHA256SUMS-{target_os}-{arch}"


def find_archive(dist_root: Path, version: str, stage: str) -> Path:
    """The release-tagged archive of ``stage`` built into ``dist_root``."""
    matches = sorted(
        path for path in dist_root.glob(f"crt-{version}-*-{stage}.*")
        if path.name.endswith((".zip", ".tar.xz")))
    if not matches:
        raise ReleaseError(
            f"{stage}: no crt-{version}-*-{stage} archive in {dist_root}; build it "
            f"with -DCRT_RELEASE_TAG={version} (or pass --sdk {stage}=<directory>)")
    if len(matches) > 1:
        raise ReleaseError(f"{stage}: several archives match: "
                           + ", ".join(path.name for path in matches))
    return matches[0]


def release_grade_problems(stage: str, manifest: dict, dist: Path) -> list[str]:
    """Return every reason ``dist`` must not be published as ``stage``."""
    problems: list[str] = []
    if manifest.get("stage") != stage:
        problems.append(
            f"manifest stage is {manifest.get('stage')!r}, expected {stage!r}")
    if manifest.get("compiler_bundled") is not False:
        problems.append("manifest does not declare compiler_bundled=false")
    target = manifest.get("target") or {}
    if not target.get("os") or not target.get("arch"):
        problems.append("manifest has no target os/arch")
    if stage == "04-gfx-media":
        built_from = (manifest.get("built_from") or {}).get("stage")
        if built_from != "03-gfx-simple":
            problems.append(
                "this 04-gfx-media was not produced by the isolated option-ON "
                "stage build (no built_from.stage == '03-gfx-simple'); the "
                "ordinary cumulative dist target keeps Skia and FFmpeg OFF "
                "(build it with tools/crt-stage-build.py, see docs/release_preview.md)")
        declared = {item.get("name") for item in
                    manifest.get("redistributed_dependencies", [])}
        for name in OPTION_ON_DEPENDENCIES:
            if name not in declared:
                problems.append(f"04-gfx-media does not declare its {name} dependency")
        for relative in OPTION_ON_FILES:
            if not (dist / relative).exists():
                problems.append(f"04-gfx-media is missing {relative}")
    return problems


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def recipe_problems(sdk: Path, version: str,
                    stage_source_dir: Path) -> tuple[list[str], list[Path]]:
    """Check the recipes packaged in ``sdk`` against ``version``.

    Returns (problems, stage-source assets to publish alongside the SDKs).
    """
    problems: list[str] = []
    assets: list[Path] = []
    marker = f"/releases/download/{version}/"
    for recipe_path in sorted((sdk / "stages" / "recipes").glob("*.json")):
        source = json.loads(recipe_path.read_text(encoding="utf-8")).get("source", {})
        url = source.get("url", "")
        if marker not in url:
            problems.append(
                f"{recipe_path.name}: source URL {url!r} does not point at release "
                f"{version} (rebuild with -DCRT_RELEASE_TAG={version})")
            continue
        asset = stage_source_dir / source.get("archive", "")
        if not asset.is_file():
            problems.append(f"{recipe_path.name}: stage-source asset {asset.name} "
                            f"is missing from {stage_source_dir}")
            continue
        if asset.stat().st_size != source.get("size"):
            problems.append(f"{asset.name}: size {asset.stat().st_size} != recipe "
                            f"{source.get('size')}")
        elif sha256_file(asset) != source.get("sha256"):
            problems.append(f"{asset.name}: SHA-256 does not match its recipe")
        else:
            assets.append(asset)
    return problems, assets


def format_sha256sums(entries: list[tuple[str, str]]) -> str:
    """``sha256sum -c`` compatible text: '<hex>  <name>' per asset, sorted."""
    return "".join(f"{digest}  {name}\n" for name, digest in sorted(entries))


def git_state(root: Path) -> tuple[str, bool]:
    def run(*args: str) -> str:
        return subprocess.run(["git", *args], cwd=root, check=True,
                              capture_output=True, text=True).stdout.strip()
    return run("rev-parse", "HEAD"), bool(run("status", "--porcelain"))


def run_verify(dist: Path, stage: str) -> None:
    result = subprocess.run(
        [sys.executable, str(REPO_ROOT / "tools" / "verify_dist.py"),
         "--dist", str(dist), "--stage", stage],
        capture_output=True, text=True)
    if result.returncode != 0:
        raise ReleaseError(
            f"verify_dist.py rejected {dist}:\n{result.stdout}{result.stderr}")


def check_sdk(sdk: Path, stage: str, version: str,
              stage_source_dir: Path) -> tuple[dict, list[Path]]:
    """Every release check on an extracted SDK tree; returns (manifest, assets)."""
    manifest_path = sdk / "manifest.json"
    if not manifest_path.is_file():
        raise ReleaseError(f"{stage}: {sdk} has no manifest.json")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    problems = release_grade_problems(stage, manifest, sdk)
    recorded = (sdk / "VERSION").read_text(encoding="utf-8").strip() \
        if (sdk / "VERSION").is_file() else ""
    if recorded != version:
        problems.append(f"VERSION is {recorded!r}, expected {version!r} "
                        f"(build with -DCRT_RELEASE_TAG={version})")
    recipe_issues, assets = recipe_problems(sdk, version, stage_source_dir)
    problems += recipe_issues
    if problems:
        raise ReleaseError(f"{stage} is not release-grade:\n  - " + "\n  - ".join(problems))
    run_verify(sdk, stage)
    return manifest, assets


def archive_directory(sdk: Path, stage: str, version: str, target_os: str,
                      arch: str, output_dir: Path) -> Path:
    """Archive an extracted SDK directory byte-for-byte under its release name."""
    base = output_dir / archive_base_name(version, target_os, arch, stage)
    fmt = "zip" if target_os == "windows" else "xztar"
    return Path(shutil.make_archive(str(base), fmt, root_dir=sdk.parent,
                                    base_dir=sdk.name))


def prepare(dist_root: Path, version: str, output_dir: Path,
            stages: tuple[str, ...], sdk_dirs: dict[str, Path],
            stage_source_dir: Path, allow_dirty: bool, dry_run: bool) -> dict:
    validate_version(version)
    commit, dirty = git_state(REPO_ROOT)
    if dirty and not allow_dirty:
        raise ReleaseError(
            "the working tree has uncommitted changes; release from a clean "
            "commit (or pass --allow-dirty for a rehearsal)")
    for stage in stages:
        if stage not in RELEASE_STAGES:
            raise ReleaseError(f"stage {stage!r} is not part of the preview "
                               f"(publishable: {', '.join(RELEASE_STAGES)})")

    if not dry_run:
        output_dir.mkdir(parents=True, exist_ok=True)
    assets: dict[str, dict] = {}
    hosts: set[tuple[str, str]] = set()
    stage_sources: dict[str, Path] = {}
    with tempfile.TemporaryDirectory(prefix="crt release ") as scratch:
        for stage in stages:
            if stage in sdk_dirs:
                sdk = sdk_dirs[stage]
                if sdk.name != stage:
                    raise ReleaseError(
                        f"--sdk {stage}: the directory must be named '{stage}' "
                        f"(got '{sdk.name}') so the archive extracts to '{stage}/'")
                archive = None
                print(f"[release] {stage}: checking directory {sdk}", flush=True)
            else:
                archive = find_archive(dist_root, version, stage)
                print(f"[release] {stage}: checking {archive.name}", flush=True)
                extract_root = Path(scratch) / stage
                shutil.unpack_archive(str(archive), str(extract_root))
                sdk = extract_root / stage
                if not sdk.is_dir():
                    raise ReleaseError(f"{archive.name} does not extract to '{stage}/'")
            manifest, sources = check_sdk(sdk, stage, version, stage_source_dir)
            target_os, arch = manifest["target"]["os"], manifest["target"]["arch"]
            hosts.add((target_os, arch))
            for asset in sources:
                stage_sources[asset.name] = asset
            if dry_run:
                continue
            if archive is None:
                archive = archive_directory(sdk, stage, version, target_os, arch,
                                            output_dir)
            else:
                shutil.copy2(archive, output_dir / archive.name)
                archive = output_dir / archive.name
            assets[archive.name] = {"name": archive.name, "kind": "sdk",
                                    "stage": stage, "os": target_os, "arch": arch}
    if len(hosts) > 1:
        raise ReleaseError(f"stages target different hosts: {sorted(hosts)}")

    if not dry_run:
        for name, source in sorted(stage_sources.items()):
            shutil.copy2(source, output_dir / name)
            assets[name] = {"name": name, "kind": "stage-source"}
        for entry in assets.values():
            path = output_dir / entry["name"]
            entry["size"] = path.stat().st_size
            entry["sha256"] = sha256_file(path)
    release = {
        "version": version, "commit": commit, "working_tree_dirty": dirty,
        "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "assets": sorted(assets.values(), key=lambda entry: entry["name"]),
    }
    if not dry_run:
        host_os, host_arch = next(iter(hosts))
        release["host"] = {"os": host_os, "arch": host_arch}
        (output_dir / checksum_file_name(host_os, host_arch)).write_text(
            format_sha256sums([(a["name"], a["sha256"]) for a in release["assets"]]),
            encoding="utf-8", newline="\n")
        (output_dir / f"release-manifest-{host_os}-{host_arch}.json").write_text(
            json.dumps(release, indent=2) + "\n", encoding="utf-8", newline="\n")
    return release


def parse_sdk_overrides(values: list[str]) -> dict[str, Path]:
    overrides: dict[str, Path] = {}
    for value in values:
        stage, separator, directory = value.partition("=")
        if not separator or stage not in RELEASE_STAGES:
            raise ReleaseError(f"--sdk expects STAGE=DIR with STAGE one of "
                               f"{', '.join(RELEASE_STAGES)}, got {value!r}")
        overrides[stage] = Path(directory).resolve()
    return overrides


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Collect, verify, and checksum CRT release assets for one host "
                    "(nothing is uploaded or published).")
    parser.add_argument("--dist-root", required=True, type=Path,
                        help="a built out/<preset>/dist directory holding the "
                             "release-tagged archives")
    parser.add_argument("--version", required=True,
                        help="release tag, e.g. v0.4.0-preview.1")
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--stage-source-dir", type=Path,
                        help="stage-source assets (default: <dist-root>/../stage-sources)")
    parser.add_argument("--stages", nargs="+", default=list(RELEASE_STAGES),
                        metavar="STAGE")
    parser.add_argument("--sdk", action="append", default=[], metavar="STAGE=DIR",
                        help="use an extracted SDK directory for a stage instead of "
                             "an archive (e.g. the isolated option-ON 04-gfx-media)")
    parser.add_argument("--allow-dirty", action="store_true",
                        help="permit a dirty git tree (rehearsals only)")
    parser.add_argument("--dry-run", action="store_true",
                        help="run every check but write nothing")
    args = parser.parse_args(argv)
    dist_root = args.dist_root.resolve()
    stage_source_dir = (args.stage_source_dir or dist_root.parent / "stage-sources").resolve()
    try:
        release = prepare(dist_root, args.version, args.output_dir.resolve(),
                          tuple(args.stages), parse_sdk_overrides(args.sdk),
                          stage_source_dir, args.allow_dirty, args.dry_run)
    except ReleaseError as error:
        print(f"prepare_release_assets: {error}", file=sys.stderr)
        return 1
    for asset in release["assets"]:
        print(f"[release] {asset['sha256']}  {asset['name']} "
              f"({asset['size'] / 1048576:.1f} MB)")
    print("[release] done" + (" (dry run)" if args.dry_run else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
