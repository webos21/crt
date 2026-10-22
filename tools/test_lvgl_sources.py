#!/usr/bin/env python3
"""The committed LVGL source list must match the pinned version (and the fetched tree when one is present)."""

import json
import re
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LIST = ROOT / "libcrtui/cmake/lvgl_sources.cmake"
RECIPE = ROOT / "libcrtui/third_party/lvgl/recipe.json"


class LvglSources(unittest.TestCase):
    def test_list_names_the_pinned_version(self) -> None:
        version = json.loads(RECIPE.read_text(encoding="utf-8"))["version"]
        self.assertIn(f'set(CRTUI_LVGL_PINNED_VERSION "{version}")', LIST.read_text(encoding="utf-8"))

    def test_list_is_sorted_unique_and_relative(self) -> None:
        names = re.findall(r"^  (\S+\.c)$", LIST.read_text(encoding="utf-8"), re.M)
        self.assertGreater(len(names), 300)
        self.assertEqual(sorted(set(names)), names)
        self.assertTrue(all(n.startswith("src/") for n in names))

    def test_matches_a_fetched_tree_when_present(self) -> None:
        for tree in ROOT.glob("out/*/lvgl/lvgl-*"):
            if (tree / "src").is_dir():
                result = subprocess.run([sys.executable, str(ROOT / "tools/gen_lvgl_sources.py"), str(tree), "--check"],
                                        capture_output=True, text=True)
                self.assertEqual(0, result.returncode, result.stderr)
                return
        self.skipTest("no fetched LVGL tree under out/")


if __name__ == "__main__":
    unittest.main()
