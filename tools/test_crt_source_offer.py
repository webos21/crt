#!/usr/bin/env python3
"""Tests for tools/crt_source_offer.py (LGPL source offer and corresponding-source bundle, Tranche 10)."""

import io
import json
import lzma
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import crt_source_offer as offer

ROOT = offer.ROOT


class ConfigTests(unittest.TestCase):
    def test_real_config_is_valid(self) -> None:
        config = offer.load_config(ROOT)
        self.assertTrue(config["issue_tracker_url"].startswith("https://"))
        self.assertGreaterEqual(config["offer_years"], 3)

    def test_period_below_the_lgpl_minimum_and_non_https_urls_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "third_party" / "crt-runtime").mkdir(parents=True)
            path = root / offer.RELEASE_CONFIG
            good = {"repository_url": "https://example.org/r", "issue_tracker_url": "https://example.org/r/issues", "offer_years": 3}
            for patch in ({"offer_years": 2}, {"issue_tracker_url": "http://example.org/i"}, {"offer_years": True}):
                path.write_text(json.dumps({**good, **patch}))
                with self.assertRaises(ValueError, msg=str(patch)):
                    offer.load_config(root)
            path.write_text(json.dumps(good))
            self.assertEqual(3, offer.load_config(root)["offer_years"])


class RenderTests(unittest.TestCase):
    def setUp(self) -> None:
        self.config = offer.load_config(ROOT)

    def test_web_stage_offer_names_both_components_with_hashes_and_the_release(self) -> None:
        components = offer.components_from_root(ROOT, "06-web", True)
        text = offer.render_offer(ROOT, self.config, "06-web", "v1.2.3", components, "abc123")
        for item in components:
            self.assertIn(item["archive"]["sha256"], text)
            self.assertIn(item["archive"]["url"], text)
        self.assertIn("https://github.com/webos21/crt/releases/tag/v1.2.3", text)
        self.assertIn("crt-v1.2.3-corresponding-source.tar.xz", text)
        self.assertIn("3 years", text)
        self.assertIn(self.config["issue_tracker_url"], text)
        self.assertEqual([], offer.unresolved_problems(text))
        self.assertIn("the 23 CRT patches", text)

    def test_web_stage_offer_names_the_mpl_and_bison_files_that_are_compiled_in(self) -> None:
        components = offer.components_from_root(ROOT, "06-web", True)
        text = offer.render_offer(ROOT, self.config, "06-web", "v1.2.3", components, "abc123")
        for needle in ("Mozilla Public License 2.0", "StringBuilderJSON.cpp", "XPathGrammar.cpp", "Bison's special exception"):
            self.assertIn(needle, text)
        media = offer.render_offer(ROOT, self.config, "05-ui", "v1", offer.components_from_root(ROOT, "05-ui", True))
        self.assertNotIn("Mozilla", media)

    def test_media_stage_offer_is_ffmpeg_only(self) -> None:
        text = offer.render_offer(ROOT, self.config, "05-ui", "v1", offer.components_from_root(ROOT, "05-ui", True))
        self.assertIn("FFmpeg", text)
        self.assertNotIn("WebKit", text)

    def test_development_build_points_at_the_repository_not_a_release(self) -> None:
        text = offer.render_offer(ROOT, self.config, "04-gfx-media", "development",
                                  offer.components_from_root(ROOT, "04-gfx-media", True), "deadbeef")
        self.assertIn("development build, not a release", text)
        self.assertNotIn("/releases/tag/development", text)
        self.assertIn("deadbeef", text)

    def test_a_stage_without_lgpl_code_gets_no_offer(self) -> None:
        with self.assertRaises(ValueError):
            offer.render_offer(ROOT, self.config, "02-cxx", "v1", [])

    def test_unresolved_text_is_caught(self) -> None:
        self.assertEqual(1, len(offer.unresolved_problems("fine\nsee {{oops}}\n")))
        self.assertEqual(1, len(offer.unresolved_problems("contact: [PLACEHOLDER]\n")))
        self.assertEqual([], offer.unresolved_problems("nothing to resolve\n"))

    def test_installed_offer_check(self) -> None:
        components = offer.components_from_root(ROOT, "06-web", True)
        text = offer.render_offer(ROOT, self.config, "06-web", "v1", components)
        with tempfile.TemporaryDirectory() as tmp:
            sdk = Path(tmp)
            self.assertIn("is missing", offer.check_installed_offer(sdk, "06-web", "v1", components)[0])
            offer.install_offer(sdk, text)
            self.assertEqual([], offer.check_installed_offer(sdk, "06-web", "v1", components))
            self.assertTrue(offer.check_installed_offer(sdk, "06-web", "v2", components))
            (sdk / offer.OFFER_RELATIVE).write_text(text.replace(components[1]["archive"]["sha256"], "0" * 64))
            self.assertIn("WPE WebKit", " ".join(offer.check_installed_offer(sdk, "06-web", "v1", components)))
            (sdk / offer.OFFER_RELATIVE).write_text(text + "\nTODO: fill in\n")
            self.assertTrue(offer.check_installed_offer(sdk, "06-web", "v1", components))


class BundleTests(unittest.TestCase):
    def test_bundle_holds_the_committed_tree_and_names_the_upstream_archives(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp) / "repo"
            repo.mkdir()
            run = lambda *a: subprocess.run(["git", "-C", str(repo), *a], check=True, capture_output=True)  # noqa: E731
            run("init", "-q")
            run("config", "user.email", "t@example.org")
            run("config", "user.name", "t")
            (repo / "a.txt").write_text("alpha\n")
            run("add", "a.txt")
            run("commit", "-q", "-m", "x")
            config = {"repository_url": "https://example.org/r", "issue_tracker_url": "https://example.org/r/issues", "offer_years": 3}
            components = offer.components_from_root(ROOT, "06-web", True)
            path = offer.build_bundle(repo, config, "v9", Path(tmp) / "out", components)
            self.assertEqual("crt-v9-corresponding-source.tar.xz", path.name)
            with lzma.open(path) as stream, tarfile.open(fileobj=io.BytesIO(stream.read())) as archive:
                names = archive.getnames()
                self.assertIn("crt-v9/a.txt", names)
                manifest = json.load(archive.extractfile("crt-v9/CORRESPONDING-SOURCE.json"))
            self.assertEqual(offer.git(repo, "rev-parse", "HEAD"), manifest["commit"])
            self.assertEqual({"FFmpeg", "WPE WebKit"}, {a["name"] for a in manifest["upstream_archives"]})
            self.assertTrue(all(len(a["sha256"]) == 64 for a in manifest["upstream_archives"]))
            (repo / "a.txt").write_text("changed\n")
            with self.assertRaises(ValueError):
                offer.build_bundle(repo, config, "v9", Path(tmp) / "out2", components)
            offer.build_bundle(repo, config, "v9", Path(tmp) / "out2", components, allow_dirty=True)

    def test_commit_lookup_tolerates_a_source_asset_without_git(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            self.assertIsNone(offer.try_commit(Path(tmp)))


if __name__ == "__main__":
    unittest.main()
