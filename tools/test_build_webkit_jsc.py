#!/usr/bin/env python3
"""Tests for the JavaScriptCore build-mode contract and native ABI audit."""

import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

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


class WindowsMediaABIAuditTests(unittest.TestCase):
    def audit(self, name, imports):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sdk, deps, build = (root / part for part in ("sdk", "deps", "build"))
            for base in (sdk, deps, build):
                (base / "bin").mkdir(parents=True)
            binary = build / "bin" / name
            binary.touch()
            output = "\n".join(f"    DLL Name: {item}" for item in imports)
            with mock.patch("shutil.which", return_value="llvm-objdump"), \
                    mock.patch.object(build_webkit_jsc, "run", return_value=
                                      subprocess.CompletedProcess([], 0, output, "")):
                return build_webkit_jsc.host_abi_audit_windows(sdk, deps, build, [binary])

    def test_declared_media_pal_imports_are_recorded_for_crtmedia(self):
        result = self.audit("libcrtmedia.dll", ["KERNEL32.dll", "ole32.dll", "MFPlat.DLL", "MFReadWrite.dll", "MF.dll"])
        self.assertTrue(result["ok"], result["violations"])
        self.assertEqual(set(result["known_host_libraries"]),
                         {"ole32.dll", "mfplat.dll", "mfreadwrite.dll", "mf.dll"})

    def test_webkit_cannot_import_the_media_pal_directly(self):
        result = self.audit("WebProcess.exe", ["MFPlat.DLL"])
        self.assertFalse(result["ok"])

    def test_media_boundary_does_not_allow_the_host_c_runtime(self):
        for name in ("ucrtbase.dll", "api-ms-win-crt-runtime-l1-1-0.dll"):
            with self.subTest(name=name):
                result = self.audit("libcrtmedia.dll", [name])
                self.assertFalse(result["ok"])


if __name__ == "__main__":
    unittest.main()
