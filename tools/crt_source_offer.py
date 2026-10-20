#!/usr/bin/env python3
"""LGPL source offer and corresponding-source bundle (Tranche 10 release closure).

Two parts of the SDK are LGPL code that is compiled into redistributed binaries: FFmpeg (every stage that ships it,
04-gfx-media onwards) and WPE WebKit (06-web). Distributing binaries with them requires that the recipient can get
the corresponding source and replace the library. This module

* renders ``share/licenses/SOURCE-OFFER.md`` for a stage from the template
  ``third_party/crt-runtime/SOURCE-OFFER.template.md``, the release-time values in
  ``third_party/crt-runtime/release.json`` (issue tracker, offer period) and the build's own facts (version/tag,
  upstream archive URL and SHA-256 of each LGPL component, number of carried patches), so nothing about a release
  is typed by hand; and
* builds the **corresponding-source bundle** ``crt-<tag>-corresponding-source.tar.xz``: the tracked files of the
  release commit (the CRT sources, the carried patches, the PlatformCRT overlay, every recipe and build tool)
  plus ``CORRESPONDING-SOURCE.json`` naming the commit and each upstream archive with its SHA-256. The upstream
  archives themselves are not copied in (policy decision); the written offer in the generated text covers them.

Nothing is uploaded; ``tools/prepare_release_assets.py`` puts the bundle next to the SDK archives.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import lzma
import re
import subprocess
import sys
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RELEASE_CONFIG = Path("third_party/crt-runtime/release.json")
TEMPLATE = Path("third_party/crt-runtime/SOURCE-OFFER.template.md")
OFFER_RELATIVE = Path("share/licenses/SOURCE-OFFER.md")
STAGE_ORDER = ("01-c", "02-cxx", "03-gfx-simple", "04-gfx-media", "05-ui", "06-web")
UNRESOLVED = re.compile(r"\{\{|\}\}|PLACEHOLDER|TODO|TBD", re.IGNORECASE)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_config(root: Path = ROOT) -> dict:
    config = json.loads((root / RELEASE_CONFIG).read_text(encoding="utf-8"))
    for key in ("repository_url", "issue_tracker_url"):
        value = config.get(key, "")
        if not isinstance(value, str) or not value.startswith("https://"):
            raise ValueError(f"{RELEASE_CONFIG}: {key} must be an https URL")
    years = config.get("offer_years")
    if not isinstance(years, int) or isinstance(years, bool) or years < 3:
        raise ValueError(f"{RELEASE_CONFIG}: offer_years must be an integer of at least 3 (the LGPL minimum)")
    return config


def ffmpeg_component(recipe: dict) -> dict:
    source = recipe["source"]
    return {
        "name": "FFmpeg", "version": recipe["version"], "license": "LGPL-2.1-or-later",
        "license_note": "configured without --enable-gpl and --enable-nonfree",
        "license_file": "share/licenses/ffmpeg/COPYING.LGPLv2.1",
        "archive": {"url": source["url"], "sha256": source["sha256"], "size": source.get("size")},
        "patches": 0, "recipe": "porting/recipes/ffmpeg.json",
        "linkage": ("static archives lib/libavcodec.a, libavformat.a, libavutil.a, libswresample.a, and the same "
                    "code built into the shared libcrtmedia"),
        "replace": ("Static link: build a modified FFmpeg with the recipe `porting/recipes/ffmpeg.json` and link "
                    "your application with your `libav*.a`/`libswresample.a` in place of the SDK's (they are "
                    "separate from `libcrtmedia.a`). Shared `libcrtmedia`: rebuild it from the corresponding "
                    "source with your FFmpeg install and substitute it."),
    }


def webkit_component(recipe: dict, patch_manifest: dict) -> dict:
    source = recipe["source"]
    return {
        "name": "WPE WebKit", "version": recipe["version"], "license": "LGPL-2.0-or-later / LGPL-2.1-or-later",
        "license_note": "with BSD-licensed parts; see share/licenses/webkit/",
        "license_file": "share/licenses/webkit/Source/",
        "archive": {"url": source["archive_url"], "sha256": source["archive_sha256"], "size": source.get("archive_size")},
        "commit": recipe.get("product_source", {}).get("commit"),
        "patches": len(patch_manifest["patches"]),
        "recipe": "libcrtweb/third_party/webkit/recipe.json",
        "linkage": ("private shared library `libexec/crtweb/lib/libWebKit.*` with its helper processes "
                    "(`WebProcess`, `NetworkProcess`, `crtweb_host`); applications link only `libcrtweb`"),
        "replace": ("Rebuild the private WebKit closure with `tools/crt-stage-build.py` from the source recipe and "
                    "predecessor SDK named in `share/crt/web-source-correspondence.json`, then replace "
                    "`libexec/crtweb/` (library and matching helper processes together). Applications never link "
                    "WebKit, so nothing else has to be relinked."),
    }


def components_from_root(root: Path, stage: str, has_ffmpeg: bool) -> list:
    """The LGPL components of `stage`, read from the repository's recipes."""
    result = []
    if has_ffmpeg:
        result.append(ffmpeg_component(json.loads((root / "porting/recipes/ffmpeg.json").read_text(encoding="utf-8"))))
    if stage == "06-web":
        result.append(webkit_component(
            json.loads((root / "libcrtweb/third_party/webkit/recipe.json").read_text(encoding="utf-8")),
            json.loads((root / "libcrtweb/patches/manifest.json").read_text(encoding="utf-8"))))
    return result


def components_from_sdk(sdk: Path, stage: str) -> list:
    """The same, read from an installed SDK (the isolated 06-web stage has no repository)."""
    result = []
    ffmpeg = sdk / "share/crt/dependencies/ffmpeg/recipe.json"
    if ffmpeg.is_file():
        result.append(ffmpeg_component(json.loads(ffmpeg.read_text(encoding="utf-8"))))
    webkit = sdk / "share/crt/dependencies/webkit/recipe.json"
    if stage == "06-web" and webkit.is_file():
        result.append(webkit_component(json.loads(webkit.read_text(encoding="utf-8")),
                                       json.loads((sdk / "share/crt/dependencies/webkit/patches/manifest.json")
                                                  .read_text(encoding="utf-8"))))
    return result


def release_url(config: dict, version: str) -> str:
    return f"{config['repository_url']}/releases/tag/{version}"


def render_offer(root: Path, config: dict, stage: str, version: str, components: list,
                 commit: str | None = None) -> str:
    """The generated text; raises if the template or the inputs leave anything unresolved."""
    if not components:
        raise ValueError(f"stage {stage} has no LGPL component; no source offer is generated")
    development = version == "development"
    rows = ["| Component | Version | License | In this SDK as |", "| --- | --- | --- | --- |"]
    for item in components:
        rows.append(f"| {item['name']} | {item['version']} | {item['license']} ({item['license_note']}) "
                    f"| {item['linkage']} |")
    if development:
        where = (f"This is a development build, not a release: the corresponding source is the CRT repository "
                 f"{config['repository_url']}" + (f" at commit `{commit}`." if commit else "."))
    else:
        where = (f"The corresponding-source bundle `crt-{version}-corresponding-source.tar.xz` is published with "
                 f"this SDK on its release page, {release_url(config, version)}"
                 + (f" (release commit `{commit}`)." if commit else "."))
    parts = []
    for item in components:
        patches = (f"; the {item['patches']} CRT patches and their manifest (`share/crt/dependencies/"
                   f"{'webkit/patches' if item['name'] == 'WPE WebKit' else 'ffmpeg'}`), the CRT-owned port overlay "
                   f"(`share/crt/dependencies/webkit/port`)" if item["patches"] else "; no CRT patches")
        parts.append(f"* **{item['name']} {item['version']}**: the unmodified upstream archive named below{patches}, "
                     f"and the build recipe `{item['recipe']}` with the build tools, all in the bundle.")
    upstream = [f"* {item['name']} {item['version']}: <{item['archive']['url']}>, SHA-256 "
                f"`{item['archive']['sha256']}`" + (f", {item['archive']['size']} bytes" if item['archive'].get("size") else "")
                for item in components]
    template = (root / TEMPLATE).read_text(encoding="utf-8")
    values = {
        "version": version, "stage": stage,
        "component_names": " and ".join(f"{item['name']} {item['version']}" for item in components),
        "component_table": "\n".join(rows), "where": where, "source_parts": "\n".join(parts),
        "issue_tracker": config["issue_tracker_url"], "offer_years": str(config["offer_years"]),
        "upstream_list": "\n".join(upstream), "replace": "\n\n".join(f"**{item['name']}.** {item['replace']}" for item in components),
    }
    text = template
    for key, value in values.items():
        text = text.replace("{{" + key + "}}", value)
    problems = unresolved_problems(text)
    if problems:
        raise ValueError("the rendered source offer has unresolved text: " + "; ".join(problems))
    return text


def unresolved_problems(text: str) -> list:
    return [f"line {number}: {line.strip()[:80]}" for number, line in enumerate(text.splitlines(), 1)
            if UNRESOLVED.search(line)]


def install_offer(destination: Path, text: str) -> str:
    target = destination / OFFER_RELATIVE
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8", newline="\n")
    return OFFER_RELATIVE.as_posix()


def check_installed_offer(sdk: Path, stage: str, version: str, components: list) -> list:
    """Problems with the offer inside an installed SDK (empty list = fine). Used by verify_dist and the release tool."""
    path = sdk / OFFER_RELATIVE
    if not path.is_file():
        return [f"{OFFER_RELATIVE.as_posix()} is missing"]
    text = path.read_text(encoding="utf-8")
    problems = unresolved_problems(text)
    if f"({version}, stage {stage})" not in text:
        problems.append(f"it does not name version {version} and stage {stage}")
    for item in components:
        if item["archive"]["sha256"] not in text:
            problems.append(f"it does not carry the SHA-256 of the {item['name']} archive")
    return problems


def git(root: Path, *args: str) -> str:
    return subprocess.run(["git", "-C", str(root), *args], check=True, capture_output=True, text=True).stdout.strip()


def try_commit(root: Path) -> str | None:
    """The commit of `root`, or None when it is not a git checkout (a stage-source asset)."""
    try:
        return git(root, "rev-parse", "HEAD")
    except (subprocess.CalledProcessError, FileNotFoundError, OSError):
        return None


def bundle_name(version: str) -> str:
    return f"crt-{version}-corresponding-source.tar.xz"


def build_bundle(root: Path, config: dict, version: str, output_dir: Path, components: list,
                 allow_dirty: bool = False) -> Path:
    """The tracked files of HEAD plus CORRESPONDING-SOURCE.json, as <output_dir>/crt-<version>-corresponding-source.tar.xz."""
    if git(root, "status", "--porcelain") and not allow_dirty:
        raise ValueError("the working tree has uncommitted changes; the bundle would not match the commit")
    commit = git(root, "rev-parse", "HEAD")
    manifest = {
        "format": 1, "version": version, "repository": config["repository_url"], "commit": commit,
        "release": None if version == "development" else release_url(config, version),
        "offer": {"issue_tracker": config["issue_tracker_url"], "years": config["offer_years"]},
        "upstream_archives": [{"name": item["name"], "version": item["version"], **item["archive"],
                               "license": item["license"], "carried_patches": item["patches"],
                               "recipe": item["recipe"]} for item in components],
        "note": "The upstream archives are not included; each is named by address and SHA-256.",
    }
    prefix = f"crt-{version}"
    archive = subprocess.run(["git", "-C", str(root), "archive", "--format=tar", f"--prefix={prefix}/", "HEAD"],
                             check=True, capture_output=True).stdout
    output_dir.mkdir(parents=True, exist_ok=True)
    target = output_dir / bundle_name(version)
    data = (json.dumps(manifest, indent=2) + "\n").encode("utf-8")
    with lzma.open(target, "wb", preset=6) as stream, tarfile.open(fileobj=stream, mode="w") as out:
        with tarfile.open(fileobj=io.BytesIO(archive), mode="r") as source:
            for member in source:
                out.addfile(member, source.extractfile(member) if member.isfile() else None)
        info = tarfile.TarInfo(f"{prefix}/CORRESPONDING-SOURCE.json")
        info.size = len(data)
        info.mtime = 0
        out.addfile(info, io.BytesIO(data))
    return target


def main(argv: list | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    render = sub.add_parser("render", help="print the offer for a stage")
    render.add_argument("--stage", required=True, choices=STAGE_ORDER)
    render.add_argument("--version", default="development")
    render.add_argument("--no-ffmpeg", action="store_true", help="the stage's SDK does not ship FFmpeg")
    bundle = sub.add_parser("bundle", help="build the corresponding-source bundle")
    bundle.add_argument("--version", required=True)
    bundle.add_argument("--output-dir", required=True, type=Path)
    bundle.add_argument("--stage", default="06-web", choices=STAGE_ORDER)
    bundle.add_argument("--allow-dirty", action="store_true")
    args = parser.parse_args(argv)
    config = load_config()
    components = components_from_root(ROOT, args.stage, not getattr(args, "no_ffmpeg", False))
    if args.command == "render":
        print(render_offer(ROOT, config, args.stage, args.version, components, git(ROOT, "rev-parse", "HEAD")), end="")
    else:
        path = build_bundle(ROOT, config, args.version, args.output_dir, components, args.allow_dirty)
        print(f"{path} {path.stat().st_size} bytes sha256 {sha256_bytes(path.read_bytes())}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
