#!/usr/bin/env python3
"""Focused unit tests for tools/prepare_release_assets.py (no real SDK needed)."""

import hashlib
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))

import prepare_release_assets as release

TAG = "v0.4.0-preview.1"


def make_sdk(root: Path, stage: str, *, version=TAG, target_os="windows",
             arch="x86_64", built_from=None, dependencies=(), files=(),
             recipes=()) -> Path:
    sdk = root / stage
    sdk.mkdir(parents=True)
    manifest = {
        "stage": stage,
        "target": {"os": target_os, "arch": arch},
        "compiler_bundled": False,
        "redistributed_dependencies": [{"name": n} for n in dependencies],
    }
    if built_from:
        manifest["built_from"] = {"stage": built_from}
    (sdk / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
    (sdk / "VERSION").write_text(version + "\n", encoding="utf-8")
    for relative in files:
        path = sdk / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"x")
    if recipes:
        (sdk / "stages" / "recipes").mkdir(parents=True)
        for name, source in recipes:
            (sdk / "stages" / "recipes" / f"{name}.json").write_text(
                json.dumps({"source": source}), encoding="utf-8")
    return sdk


def make_asset(directory: Path, name: str, payload=b"stage source bytes",
               version=TAG) -> dict:
    directory.mkdir(parents=True, exist_ok=True)
    (directory / name).write_bytes(payload)
    return {
        "url": f"https://github.com/webos21/crt/releases/download/{version}/{name}",
        "archive": name, "size": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest(),
    }


class TempCase(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)


class VersionAndNames(unittest.TestCase):
    def test_accepts_release_and_prerelease_versions(self):
        for version in (TAG, "v1.0.0", "v0.4.0-rc1"):
            self.assertEqual(release.validate_version(version), version)

    def test_rejects_development_and_malformed_versions(self):
        for version in ("development", "0.4.0", "v0.4", "v0.4.0-", "v0.4.0 preview", ""):
            with self.assertRaises(release.ReleaseError, msg=version):
                release.validate_version(version)

    def test_archive_names_carry_version_os_arch_stage(self):
        self.assertEqual(
            release.archive_base_name(TAG, "linux", "aarch64", "04-gfx-media"),
            "crt-v0.4.0-preview.1-linux-aarch64-04-gfx-media")
        self.assertEqual(release.archive_suffix("windows"), ".zip")
        self.assertEqual(release.archive_suffix("linux"), ".tar.xz")
        self.assertEqual(release.archive_suffix("macos"), ".tar.xz")

    def test_sha256sums_is_sorted_and_sha256sum_compatible(self):
        self.assertEqual(release.format_sha256sums([("b.zip", "bb"), ("a.zip", "aa")]),
                         "aa  a.zip\nbb  b.zip\n")


class FindArchive(TempCase):
    def test_finds_the_tagged_archive(self):
        (self.tmp / f"crt-{TAG}-windows-x86_64-01-c.zip").write_bytes(b"z")
        (self.tmp / "crt-development-windows-x86_64-01-c.zip").write_bytes(b"z")
        self.assertEqual(release.find_archive(self.tmp, TAG, "01-c").name,
                         f"crt-{TAG}-windows-x86_64-01-c.zip")

    def test_development_archive_is_not_accepted_for_a_release(self):
        (self.tmp / "crt-development-windows-x86_64-01-c.zip").write_bytes(b"z")
        with self.assertRaisesRegex(release.ReleaseError, "CRT_RELEASE_TAG"):
            release.find_archive(self.tmp, TAG, "01-c")

    def test_ambiguous_match_is_rejected(self):
        for os_ in ("windows", "linux"):
            suffix = ".zip" if os_ == "windows" else ".tar.xz"
            (self.tmp / f"crt-{TAG}-{os_}-x86_64-01-c{suffix}").write_bytes(b"z")
        with self.assertRaisesRegex(release.ReleaseError, "several archives"):
            release.find_archive(self.tmp, TAG, "01-c")


class ReleaseGrade(TempCase):
    def test_plain_lower_stage_is_accepted(self):
        sdk = make_sdk(self.tmp, "01-c")
        manifest = json.loads((sdk / "manifest.json").read_text())
        self.assertEqual(release.release_grade_problems("01-c", manifest, sdk), [])

    def test_cumulative_default_off_04_is_rejected(self):
        sdk = make_sdk(self.tmp, "04-gfx-media")
        manifest = json.loads((sdk / "manifest.json").read_text())
        joined = "\n".join(release.release_grade_problems("04-gfx-media", manifest, sdk))
        self.assertIn("isolated option-ON", joined)
        for name in release.OPTION_ON_DEPENDENCIES:
            self.assertIn(f"{name} dependency", joined)
        self.assertIn("lib/libskia.a", joined)

    def test_isolated_option_on_04_is_accepted(self):
        sdk = make_sdk(self.tmp, "04-gfx-media", built_from="03-gfx-simple",
                       dependencies=release.OPTION_ON_DEPENDENCIES,
                       files=release.OPTION_ON_FILES)
        manifest = json.loads((sdk / "manifest.json").read_text())
        self.assertEqual(release.release_grade_problems("04-gfx-media", manifest, sdk), [])

    def test_isolated_04_missing_a_payload_file_is_rejected(self):
        files = [f for f in release.OPTION_ON_FILES if f != "lib/libavcodec.a"]
        sdk = make_sdk(self.tmp, "04-gfx-media", built_from="03-gfx-simple",
                       dependencies=release.OPTION_ON_DEPENDENCIES, files=files)
        manifest = json.loads((sdk / "manifest.json").read_text())
        self.assertEqual(release.release_grade_problems("04-gfx-media", manifest, sdk),
                         ["04-gfx-media is missing lib/libavcodec.a"])

    def test_stage_mismatch_and_bundled_compiler_are_rejected(self):
        sdk = make_sdk(self.tmp, "02-cxx")
        manifest = json.loads((sdk / "manifest.json").read_text())
        manifest["compiler_bundled"] = True
        problems = release.release_grade_problems("01-c", manifest, sdk)
        self.assertTrue(any("expected '01-c'" in p for p in problems))
        self.assertTrue(any("compiler_bundled" in p for p in problems))


class Recipes(TempCase):
    def test_matching_recipe_and_asset_are_collected(self):
        sources = self.tmp / "stage-sources"
        recipe = make_asset(sources, "crt-v0.4.0-preview.1-windows-02-cxx-source.tar.xz")
        sdk = make_sdk(self.tmp / "sdk", "01-c", recipes=[("02-cxx", recipe)])
        problems, assets = release.recipe_problems(sdk, TAG, sources)
        self.assertEqual(problems, [])
        self.assertEqual([a.name for a in assets], [recipe["archive"]])

    def test_recipe_pointing_at_the_development_release_is_rejected(self):
        sources = self.tmp / "stage-sources"
        recipe = make_asset(sources, "crt-development-windows-02-cxx-source.tar.xz",
                            version="development")
        sdk = make_sdk(self.tmp / "sdk", "01-c", recipes=[("02-cxx", recipe)])
        problems, assets = release.recipe_problems(sdk, TAG, sources)
        self.assertEqual(assets, [])
        self.assertIn("does not point at release", problems[0])

    def test_missing_wrong_size_and_wrong_digest_are_rejected(self):
        sources = self.tmp / "stage-sources"
        good = make_asset(sources, "a-source.tar.xz")
        missing = dict(good, archive="gone-source.tar.xz",
                       url=good["url"].replace("a-source", "gone-source"))
        bad_size = make_asset(sources, "b-source.tar.xz")
        bad_size["size"] += 1
        bad_hash = make_asset(sources, "c-source.tar.xz")
        bad_hash["sha256"] = "0" * 64
        sdk = make_sdk(self.tmp / "sdk", "01-c", recipes=[
            ("missing", missing), ("size", bad_size), ("hash", bad_hash)])
        problems, assets = release.recipe_problems(sdk, TAG, sources)
        self.assertEqual(assets, [])
        text = "\n".join(problems)
        self.assertIn("gone-source.tar.xz is missing", text)
        self.assertIn("b-source.tar.xz: size", text)
        self.assertIn("c-source.tar.xz: SHA-256", text)


class Prepare(TempCase):
    def _stage(self, stage, sources, **kw):
        recipe = make_asset(sources, f"crt-{TAG}-windows-{stage}-source.tar.xz")
        return make_sdk(self.tmp / "sdks", stage, recipes=[(stage, recipe)], **kw)

    def _run(self, sdks, *, dirty=False, allow_dirty=False, dry_run=False, out=None):
        out = out or self.tmp / "out"
        with mock.patch.object(release, "run_verify"), \
             mock.patch.object(release, "git_state", return_value=("abc123", dirty)):
            return release.prepare(
                self.tmp / "dist", TAG, out, tuple(sdks), sdks,
                self.tmp / "stage-sources", allow_dirty, dry_run), out

    def test_end_to_end_writes_archives_sources_and_checksums(self):
        sources = self.tmp / "stage-sources"
        sdks = {"01-c": self._stage("01-c", sources),
                "02-cxx": self._stage("02-cxx", sources)}
        result, out = self._run(sdks)
        names = sorted(a["name"] for a in result["assets"])
        self.assertEqual(names, sorted([
            f"crt-{TAG}-windows-x86_64-01-c.zip", f"crt-{TAG}-windows-x86_64-02-cxx.zip",
            f"crt-{TAG}-windows-01-c-source.tar.xz", f"crt-{TAG}-windows-02-cxx-source.tar.xz"]))
        # sha256sum -c compatible and matching the real bytes
        sums = out / release.checksum_file_name("windows", "x86_64")
        self.assertEqual(sums.name, "SHA256SUMS-windows-x86_64")
        for line in sums.read_text().splitlines():
            digest, name = line.split("  ")
            self.assertEqual(hashlib.sha256((out / name).read_bytes()).hexdigest(), digest)
        manifest = json.loads(
            (out / "release-manifest-windows-x86_64.json").read_text())
        self.assertEqual((manifest["version"], manifest["commit"]), (TAG, "abc123"))
        self.assertEqual(manifest["host"], {"os": "windows", "arch": "x86_64"})
        # the SDK archive extracts to '<stage>/' and keeps the built VERSION bytes
        shutil.unpack_archive(str(out / f"crt-{TAG}-windows-x86_64-01-c.zip"), self.tmp / "x")
        self.assertEqual((self.tmp / "x" / "01-c" / "VERSION").read_text().strip(), TAG)

    def test_stage_source_asset_is_copied_byte_identical(self):
        sources = self.tmp / "stage-sources"
        sdks = {"01-c": self._stage("01-c", sources)}
        _, out = self._run(sdks)
        name = f"crt-{TAG}-windows-01-c-source.tar.xz"
        self.assertEqual((out / name).read_bytes(), (sources / name).read_bytes())

    def test_dirty_tree_is_refused_unless_allowed(self):
        sources = self.tmp / "stage-sources"
        sdks = {"01-c": self._stage("01-c", sources)}
        with self.assertRaisesRegex(release.ReleaseError, "uncommitted changes"):
            self._run(sdks, dirty=True)
        result, _ = self._run(sdks, dirty=True, allow_dirty=True)
        self.assertTrue(result["working_tree_dirty"])

    def test_default_off_04_blocks_the_release(self):
        sources = self.tmp / "stage-sources"
        sdks = {"04-gfx-media": self._stage("04-gfx-media", sources)}
        with self.assertRaisesRegex(release.ReleaseError, "not release-grade"):
            self._run(sdks)

    def test_sdk_version_mismatch_blocks_the_release(self):
        sources = self.tmp / "stage-sources"
        sdks = {"01-c": self._stage("01-c", sources, version="development")}
        with self.assertRaisesRegex(release.ReleaseError, "VERSION is 'development'"):
            self._run(sdks)

    def test_dry_run_writes_nothing(self):
        sources = self.tmp / "stage-sources"
        sdks = {"01-c": self._stage("01-c", sources)}
        _, out = self._run(sdks, dry_run=True)
        self.assertFalse(out.exists())

    def test_sdk_override_directory_must_be_named_after_the_stage(self):
        with self.assertRaisesRegex(release.ReleaseError, "must be named"):
            release.prepare(self.tmp / "dist", TAG, self.tmp / "out", ("01-c",),
                            {"01-c": self.tmp / "wrong-name"}, self.tmp / "s",
                            True, True)

    def test_unpublishable_stage_is_refused(self):
        with self.assertRaisesRegex(release.ReleaseError, "not part of the preview"):
            release.prepare(self.tmp / "dist", TAG, self.tmp / "out", ("05-js",),
                            {}, self.tmp / "s", True, True)


if __name__ == "__main__":
    unittest.main()
