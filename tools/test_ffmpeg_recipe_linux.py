#!/usr/bin/env python3
"""Linux FFmpeg recipe invariants that only an aarch64 build can break."""

import json
import unittest
from pathlib import Path

RECIPE = Path(__file__).resolve().parent.parent / "porting" / "recipes" / "ffmpeg.json"


class LinuxFfmpegRecipeTests(unittest.TestCase):
    def linux_args(self):
        recipe = json.loads(RECIPE.read_text(encoding="utf-8"))
        return recipe["build"]["target_overrides"]["linux"]["configure_args"]

    def test_linux_perf_stays_disabled_while_configure_sees_the_host_uapi(self):
        # FFmpeg enables linux_perf by itself on arm/aarch64 targets (never on x86_64). The recipe
        # exposes the host kernel UAPI to configure with -fcrt-real-linux-sdk and strips it from the
        # compile, so a probe that finds <linux/perf_event.h> would fix CONFIG_LINUX_PERF=1 and the
        # compile of libavutil/timer.h (<asm/unistd.h>) would fail on Linux/aarch64.
        args = self.linux_args()
        self.assertIn("--extra-cflags=-fcrt-real-linux-sdk", args)
        self.assertIn("--disable-linux-perf", args)


if __name__ == "__main__":
    unittest.main()
