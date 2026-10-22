#!/usr/bin/env python3
"""Tests for tools/list_cmake_targets.py (classification only; the ninja call is exercised by CI)."""

import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import list_cmake_targets as lt

CATALOG = {"groups": {"stage 01-c": [{"target": "crt-c-build"}, {"target": "crt-c-dist"}],
                      "ports": [{"target": "port-test-recipes"}]},
           "os_specific": {"crt-win-only": ["windows"]},
           "internal_stage_parts": {"crt-gfx-build": "part"}}


class Classify(unittest.TestCase):
    def test_listed_targets_are_user_facing_and_the_rest_internal(self) -> None:
        r = lt.classify(["crt-c-build", "crt-c-dist", "port-build-zlib", "crtgfx-skia-build", "crt_stage_x_test", "libc"], CATALOG)
        self.assertEqual({"crt-c-build": "stage 01-c", "crt-c-dist": "stage 01-c"}, r["user"])
        self.assertEqual("port recipes", r["internal"]["port-build-zlib"])
        self.assertEqual("feature preparation", r["internal"]["crtgfx-skia-build"])
        self.assertEqual("libraries and tools", r["internal"]["libc"])

    def test_a_documented_target_that_is_missing_is_reported(self) -> None:
        r = lt.classify(["crt-c-build"], CATALOG)
        self.assertEqual(["crt-c-dist", "port-test-recipes"], r["missing"])

    def test_os_specific_documented_targets_are_not_missing_elsewhere(self) -> None:
        cat = json.loads(json.dumps(CATALOG))
        cat["groups"]["stage 01-c"].append({"target": "crt-win-only"})
        r = lt.classify(["crt-c-build", "crt-c-dist", "port-test-recipes"], cat, "macos")
        self.assertEqual([], r["missing"])

    def test_os_specific_target_is_required_on_its_own_os(self) -> None:
        cat = json.loads(json.dumps(CATALOG))
        cat["groups"]["stage 01-c"].append({"target": "crt-win-only"})
        r = lt.classify(["crt-c-build", "crt-c-dist", "port-test-recipes"], cat, "windows")
        self.assertEqual(["crt-win-only"], r["missing"])

    def test_readme_documents_every_public_target_and_no_planned_one(self) -> None:
        import re
        catalog = lt.load_catalog()
        text = (lt.ROOT / "README.md").read_text(encoding="utf-8")
        build = text.split("## Build\n", 1)[1].split("\n## ", 1)[0]
        documented = set(re.findall(r"--target ([a-z0-9-]+)", build))
        listed = {entry["target"] for entries in catalog["groups"].values() for entry in entries}
        self.assertEqual(listed, documented)
        self.assertTrue(documented.isdisjoint(catalog["planned"]))

    def test_a_new_stage_looking_target_must_be_documented_or_declared_a_part(self) -> None:
        r = lt.classify(["crt-c-build", "crt-c-dist", "port-test-recipes", "crt-gfx-build", "crt-new-dist"], CATALOG)
        self.assertEqual(["crt-new-dist"], r["unlisted_stage_like"])

    def test_the_real_catalog_lists_only_stage_looking_or_known_targets(self) -> None:
        catalog = lt.load_catalog()
        listed = [e["target"] for entries in catalog["groups"].values() for e in entries]
        self.assertEqual(len(listed), len(set(listed)), "a target is listed twice")
        self.assertTrue(set(catalog["planned"]).isdisjoint(listed), "a planned target is also listed as existing")


if __name__ == "__main__":
    unittest.main()
