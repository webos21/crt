#!/usr/bin/env python3
"""The dual-license choices (FreeType FTL, Mbed TLS Apache-2.0) must be visible in the SDK."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))

import build_stage_04_gfx_media as stage


class LicenseChoices(unittest.TestCase):
    def test_choices_and_ftl_are_installed_and_listed(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "freetype-src"
            (source / "docs").mkdir(parents=True)
            (source / "src" / "base").mkdir(parents=True)
            (source / "docs" / "FTL.TXT").write_text("FTL text\n")
            (source / "src" / "base" / "ftobjs.c").write_text("/* Copyright (C) 1996-2031 by */\n")
            staged = root / "sdk"
            for name in ("freetype", "mbedtls"):
                (staged / "share" / "licenses" / name).mkdir(parents=True)
            dependencies = [{"name": "freetype", "notices": ["share/licenses/freetype/LICENSE.TXT"]},
                            {"name": "mbedtls", "notices": ["share/licenses/mbedtls/LICENSE"]}]
            stage.record_license_choices(staged, source, dependencies)
            freetype = staged / "share/licenses/freetype"
            self.assertEqual("FTL text\n", (freetype / "FTL.TXT").read_text())
            choice = (freetype / "CRT-LICENSE-CHOICE.txt").read_text()
            self.assertIn("copyright (c) 2031 The FreeType Project", choice)
            self.assertNotIn("\r", choice)
            self.assertIn("Apache License 2.0", (staged / "share/licenses/mbedtls/CRT-LICENSE-CHOICE.txt").read_text())
            self.assertEqual(["share/licenses/freetype/LICENSE.TXT", "share/licenses/freetype/FTL.TXT",
                              "share/licenses/freetype/CRT-LICENSE-CHOICE.txt"], dependencies[0]["notices"])
            self.assertEqual(2, len(dependencies[1]["notices"]))

    def test_missing_year_stops_the_build(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp)
            (source / "src" / "base").mkdir(parents=True)
            (source / "src" / "base" / "ftobjs.c").write_text("no year\n")
            with self.assertRaises(SystemExit):
                stage.record_license_choices(source, source, [])


if __name__ == "__main__":
    unittest.main()
