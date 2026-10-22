#!/usr/bin/env python3
"""Offline stage-preflight regressions; no target compiler/process is run."""

import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import crt_stage_config as config


class StageConfig(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="crt config spaces ")
        self.addCleanup(self.temp.cleanup)
        self.build = Path(self.temp.name)
        (self.build / "CMakeFiles").mkdir()
        self.cache = {"CMAKE_HOME_DIRECTORY": str(config.ROOT), "CRT_TARGET_OS": "windows",
                      "CRT_TARGET_ARCH": "host", "CMAKE_C_COMPILER": sys.executable,
                      "CMAKE_CXX_COMPILER": sys.executable, "CMAKE_MAKE_PROGRAM": sys.executable,
                      "CMAKE_BUILD_TYPE": "Debug", "CMAKE_GENERATOR": "Ninja"}
        for flags in config.FEATURES.values():
            self.cache.update({flag: "ON" for flag in flags})
        self.save()

    def save(self):
        (self.build / "CMakeCache.txt").write_text(
            "// fixture\n" + "\n".join(f"{key}:STRING={value}" for key, value in self.cache.items()) + "\n",
            encoding="utf-8")

    def check(self, stage="ui", os_name="windows", arch="AMD64"):
        return config.preflight(self.build, stage, os_name, arch)

    def test_complete_config_for_every_lower_stage_and_os(self):
        for os_name in ("linux", "macos", "windows"):
            self.cache["CRT_TARGET_OS"] = os_name
            for key in ("CRTMEDIA_LINUX_VAAPI_LIB", "CRTMEDIA_LINUX_VAAPI_DRM_LIB"):
                self.cache[key] = sys.executable  # present target link-input fixture
            self.save()
            for stage in config.STAGES[:-1]:
                with self.subTest(os=os_name, stage=stage):
                    report = self.check(stage, os_name, "arm64")
                    self.assertEqual({"os": os_name, "arch": "aarch64"}, report["target"])

    def test_c_stage_does_not_require_upper_features(self):
        for flags in config.FEATURES.values():
            self.cache.update({flag: "OFF" for flag in flags})
        self.save()
        self.assertEqual({}, self.check("c")["required_features"])
        with self.assertRaisesRegex(ValueError, "CRT_USE_IMPORTED_LIBCXX.*cmake --preset"):
            self.check("libcxx")

    def test_each_disabled_or_missing_required_option_is_reported(self):
        for flags in config.FEATURES.values():
            for flag in flags:
                with self.subTest(flag=flag):
                    self.cache[flag] = "OFF"
                    self.save()
                    with self.assertRaisesRegex(ValueError, flag):
                        self.check()
                    del self.cache[flag]
                    self.save()
                    with self.assertRaisesRegex(ValueError, flag):
                        self.check()
                    self.cache[flag] = "ON"

    def test_web_unsupported_pair_rejected_without_restricting_ui(self):
        self.check("ui", arch="arm64")
        with self.assertRaisesRegex(ValueError, "06-web is unsupported for windows/aarch64"):
            self.check("web", arch="arm64")
        self.check("web", arch="x64")
        self.cache["CRT_TARGET_OS"] = "macos"
        self.save()
        self.check("web", "macos", "arm64")
        with self.assertRaisesRegex(ValueError, "06-web is unsupported"):
            self.check("web", "macos", "x64")

    def test_linux_media_requires_target_libva_but_c_does_not(self):
        self.cache["CRT_TARGET_OS"] = "linux"
        self.cache["CRTMEDIA_LINUX_VAAPI_LIB"] = "CRTMEDIA_LINUX_VAAPI_LIB-NOTFOUND"
        self.save()
        self.check("c", "linux")
        with self.assertRaisesRegex(ValueError, "install libva-dev"):
            self.check("gfx-media", "linux")

    def test_wrong_source_target_or_missing_tool_is_rejected(self):
        for key, value, expected in (
                ("CMAKE_HOME_DIRECTORY", str(self.build), "source tree"),
                ("CRT_TARGET_OS", "linux", "target OS"),
                ("CRT_TARGET_ARCH", "arm64", "target architecture"),
                ("CMAKE_C_COMPILER", str(self.build / "missing clang"), "configured tool")):
            with self.subTest(key=key):
                old = self.cache[key]
                self.cache[key] = value
                self.save()
                with self.assertRaisesRegex(ValueError, expected):
                    self.check()
                self.cache[key] = old
        self.save()

    def test_report_is_stable_and_failure_removes_older_success(self):
        args = ["--build-dir", str(self.build), "--stage", "ui", "--target-os", "windows",
                "--target-arch", "x64"]
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(0, config.main(args))
            output = self.build / "stage-config/ui.json"
            before = output.stat().st_mtime_ns
            self.assertEqual(0, config.main(args))
            self.assertEqual(before, output.stat().st_mtime_ns)
            self.assertIn("configuration-only", json.loads(output.read_text())["scope"])
            self.cache["CRTUI_ENABLE_LVGL"] = "OFF"
            self.save()
            self.assertEqual(1, config.main(args))
            self.assertFalse(output.exists())

    def test_no_processes_are_started(self):
        with patch("subprocess.run", side_effect=AssertionError("must not run build or fetch")):
            self.check()

    def test_reapplied_preset_may_cache_a_bare_compiler_name(self):
        self.cache["CMAKE_C_COMPILER"] = "fixture-clang"
        self.save()
        with patch.object(config.shutil, "which", return_value=sys.executable) as which:
            self.assertEqual(sys.executable, self.check()["compilers"]["c"])
            which.assert_called_once_with("fixture-clang")

    def test_cmake_resolved_tool_takes_precedence_over_cache_or_path(self):
        self.cache["CMAKE_C_COMPILER"] = "fixture-clang"
        self.save()
        with patch.object(config.shutil, "which", side_effect=AssertionError("must use the resolved CMake tool")):
            report = config.preflight(self.build, "ui", "windows", "x64",
                                      configured_tools={"CMAKE_C_COMPILER": sys.executable})
            self.assertEqual(str(Path(sys.executable).resolve()), report["compilers"]["c"])


if __name__ == "__main__":
    unittest.main()
