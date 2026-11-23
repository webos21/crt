#!/usr/bin/env python3
"""PAL-04: the ordinary crt-media-dist/crt-ui-dist option matrix (cmake/crt_media_dist_options.cmake).

Runs the module's script mode with `cmake -P`, so the exact predicate the build uses is the one tested, and checks that
CMakeLists.txt actually applies it to the Linux dist targets."""

import itertools
import re
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MODULE = ROOT / "cmake" / "crt_media_dist_options.cmake"
SUPPORTED = {("OFF", "OFF", "OFF"), ("OFF", "OFF", "ON"), ("ON", "ON", "ON")}


@unittest.skipUnless(shutil.which("cmake"), "cmake is required")
class MediaDistOptionMatrixTests(unittest.TestCase):
    def run_matrix(self, skia: str, ffmpeg: str, curl: str) -> subprocess.CompletedProcess:
        return subprocess.run(
            ["cmake", f"-DCRTGFX_ENABLE_SKIA={skia}", f"-DCRTMEDIA_ENABLE_FFMPEG={ffmpeg}",
             f"-DCRTMEDIA_ENABLE_CURL={curl}", "-P", str(MODULE)],
            capture_output=True, text=True)

    def test_exactly_the_supported_rows_pass(self) -> None:
        for combination in itertools.product(("OFF", "ON"), repeat=3):
            with self.subTest(skia_ffmpeg_curl=combination):
                done = self.run_matrix(*combination)
                if combination in SUPPORTED:
                    self.assertEqual(done.returncode, 0, done.stderr)
                else:
                    self.assertNotEqual(done.returncode, 0)

    def test_the_two_reproduced_pal04_rows_get_a_readable_diagnostic(self) -> None:
        for combination in (("OFF", "ON", "ON"), ("ON", "OFF", "ON")):
            with self.subTest(skia_ffmpeg_curl=combination):
                done = self.run_matrix(*combination)
                self.assertNotEqual(done.returncode, 0)
                message = " ".join(done.stderr.split())
                self.assertIn("cannot package this option combination", message)
                self.assertIn(f"CRTGFX_ENABLE_SKIA={combination[0]}, CRTMEDIA_ENABLE_FFMPEG={combination[1]}, "
                              f"CRTMEDIA_ENABLE_CURL={combination[2]}", message)
                self.assertIn("all OFF", message)
                self.assertIn("all ON", message)

    def test_the_linux_dist_target_applies_the_rule_and_keeps_the_ordinary_command(self) -> None:
        text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertRegex(text, r'if\(CRT_TARGET_OS STREQUAL "linux"\)\s+include\(cmake/crt_media_dist_options\.cmake\)')
        self.assertRegex(text, r"if\(CRT_MEDIA_DIST_OPTION_PROBLEM\)\s+add_custom_target\(crt-media-dist")
        # The supported path keeps the unchanged ordinary command, including the full-path dependency copy.
        self.assertRegex(text, r"else\(\)\s+crt_add_cached_stage_target\(crt-media-dist NAME gfx-media-dist")
        self.assertIn("--skia-source", text)


if __name__ == "__main__":
    unittest.main()
