#!/usr/bin/env python3
"""Tests for tools/fetch_webkit.py and the pinned libcrtweb/third_party/webkit/recipe.json."""

import copy
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import fetch_webkit

ROOT = Path(__file__).resolve().parents[1]
RECIPE = ROOT / "libcrtweb" / "third_party" / "webkit" / "recipe.json"


def fixture_recipe(archive_bytes: bytes) -> dict:
    return {
        "schema_version": 1, "name": "wpewebkit", "version": "9.9.9",
        "source": {
            "archive_url": "file:///unused", "archive_name": "wpewebkit-9.9.9.tar.xz",
            "archive_size": len(archive_bytes),
            "archive_sha256": hashlib.sha256(archive_bytes).hexdigest(),
            "tag": "wpewebkit-9.9.9", "expected_commit": "a" * 40,
        },
    }


class RealPinTests(unittest.TestCase):
    def test_the_committed_pin_is_well_formed(self) -> None:
        recipe = json.loads(RECIPE.read_text(encoding="utf-8"))
        source = fetch_webkit.validate_recipe(recipe)
        self.assertEqual("2.54.0", recipe["version"])
        self.assertEqual("wpewebkit-2.54.0.tar.xz", source["archive_name"])
        self.assertTrue(source["archive_url"].endswith("/" + source["archive_name"]))
        self.assertEqual(source["tag"], "wpewebkit-" + recipe["version"])

    def test_the_pin_keeps_its_unverified_items_explicit(self) -> None:
        # Verification is partial by design (no per-file license scan, no proof the
        # tarball equals the whole tagged tree); the recipe must keep saying so.
        recipe = json.loads(RECIPE.read_text(encoding="utf-8"))
        self.assertTrue(recipe["verification"]["not_yet_verified"])
        self.assertTrue(recipe["source"]["tag_signature"]["verified"])
        self.assertEqual(40, len(recipe["source"]["tag_signature"]["fingerprint"]))


class RecipeValidationTests(unittest.TestCase):
    def test_rejects_malformed_recipes(self) -> None:
        good = fixture_recipe(b"x")
        cases = {
            "schema": lambda r: r.update(schema_version=2),
            "name": lambda r: r.update(name="lvgl"),
            "no source": lambda r: r.pop("source"),
            "missing field": lambda r: r["source"].pop("archive_sha256"),
            "short sha": lambda r: r["source"].update(archive_sha256="abc"),
            "upper sha": lambda r: r["source"].update(archive_sha256="A" * 64),
            "short commit": lambda r: r["source"].update(expected_commit="abc"),
            "zero size": lambda r: r["source"].update(archive_size=0),
            "path in name": lambda r: r["source"].update(archive_name="../wpewebkit-9.9.9.tar.xz"),
            "wrong suffix": lambda r: r["source"].update(archive_name="wpewebkit-9.9.9.zip"),
            "version not in name": lambda r: r["source"].update(archive_name="wpewebkit-1.0.tar.xz"),
        }
        for label, mutate in cases.items():
            with self.subTest(label):
                recipe = copy.deepcopy(good)
                mutate(recipe)
                with self.assertRaises(SystemExit):
                    fetch_webkit.validate_recipe(recipe)


class VerifyTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.cache = Path(self.tmp.name)
        self.payload = b"pretend tarball bytes" * 100
        self.recipe = fixture_recipe(self.payload)
        self.archive = self.cache / "wpewebkit-9.9.9.tar.xz"

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def test_accepts_exact_bytes(self) -> None:
        self.archive.write_bytes(self.payload)
        digest = fetch_webkit.verify(self.archive, self.recipe["source"])
        self.assertEqual(hashlib.sha256(self.payload).hexdigest(), digest)

    def test_rejects_changed_and_truncated_archives(self) -> None:
        for label, data in (("changed", b"X" + self.payload[1:]), ("truncated", self.payload[:-1])):
            with self.subTest(label):
                self.archive.write_bytes(data)
                with self.assertRaises(SystemExit):
                    fetch_webkit.verify(self.archive, self.recipe["source"])

    def test_main_downloads_then_verifies_and_never_overwrites_a_bad_cache(self) -> None:
        source_file = self.cache / "origin.bin"
        source_file.write_bytes(self.payload)
        self.recipe["source"]["archive_url"] = source_file.as_uri()
        recipe_path = self.cache / "recipe.json"
        recipe_path.write_text(json.dumps(self.recipe), encoding="utf-8")
        cache = self.cache / "dl"
        argv = ["fetch_webkit.py", "--recipe", str(recipe_path), "--cache", str(cache)]
        old = sys.argv
        try:
            sys.argv = argv
            self.assertEqual(0, fetch_webkit.main())
            self.assertEqual(self.payload, (cache / "wpewebkit-9.9.9.tar.xz").read_bytes())
            (cache / "wpewebkit-9.9.9.tar.xz").write_bytes(b"corrupt")
            with self.assertRaises(SystemExit):
                fetch_webkit.main()
            self.assertEqual(b"corrupt", (cache / "wpewebkit-9.9.9.tar.xz").read_bytes())
            sys.argv = argv + ["--no-download"]
            (cache / "wpewebkit-9.9.9.tar.xz").unlink()
            with self.assertRaises(SystemExit):
                fetch_webkit.main()
        finally:
            sys.argv = old


if __name__ == "__main__":
    unittest.main()
