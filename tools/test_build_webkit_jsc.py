#!/usr/bin/env python3
"""Unit tests for the JavaScriptCore build-mode contract."""

import unittest

import build_webkit_jsc


class BuildWebKitJSCModeTests(unittest.TestCase):
    def options(self, mode):
        return dict(build_webkit_jsc.configure_options(mode))

    def test_sampling_profiler_has_its_own_full_tier_build(self):
        options = self.options("sampling-profiler")
        self.assertEqual(options["-DENABLE_SAMPLING_PROFILER=ON"],
                         "Tranche 1D-D: signal-based sampling of VM threads")
        for option in ("-DENABLE_JIT=ON", "-DENABLE_DFG_JIT=ON",
                       "-DENABLE_FTL_JIT=ON", "-DENABLE_WEBASSEMBLY=ON"):
            self.assertIn(option, options)

    def test_existing_modes_keep_sampling_profiler_disabled(self):
        for mode in ("interpreter", "baseline-jit"):
            options = self.options(mode)
            self.assertIn("-DENABLE_SAMPLING_PROFILER=OFF", options)
            self.assertNotIn("-DENABLE_SAMPLING_PROFILER=ON", options)

    def test_macos_arm64_reserves_the_darwin_platform_register(self):
        arm64 = build_webkit_jsc.target_c_flags("macos", "arm64")
        self.assertIn("-DWTF_CRT_DARWIN_ARM64_ABI=1", arm64)
        self.assertNotIn("-DWTF_CRT_DARWIN_ARM64_ABI=1",
                         build_webkit_jsc.target_c_flags("macos", "x86_64"))
        self.assertNotIn("-DWTF_CRT_DARWIN_ARM64_ABI=1",
                         build_webkit_jsc.target_c_flags("linux", "aarch64"))


if __name__ == "__main__":
    unittest.main()
