#!/usr/bin/env python3
"""Configuration regressions for native Windows WebKit replay."""
import unittest
from pathlib import PureWindowsPath
from unittest import mock

import build_webkit_crt_port as port


class ConfigurePathTests(unittest.TestCase):
    def command(self, root):
        with mock.patch.object(port, "ROOT", root):
            return port.configure_command(root / "tree", root / "build", root / "sdk",
                                          root / "deps", "x86_64", [], "windows")

    def test_windows_cmake_definitions_use_forward_slashes(self):
        command = self.command(PureWindowsPath("C:/Users/Lee/crt"))
        for item in command:
            if item.startswith("-D"):
                self.assertNotIn("\\", item)
        self.assertIn("-DCRT_WEBKIT_TESTS=C:/Users/Lee/crt/libcrtweb/tests/webkit", command)

    def test_space_paths_remain_single_arguments(self):
        command = self.command(PureWindowsPath("C:/SDK with spaces/crt"))
        self.assertIn("-DCMAKE_TOOLCHAIN_FILE=C:/SDK with spaces/crt/sdk/crt-toolchain.cmake", command)
        self.assertIn("-DCRT_LIBCRTWEB_DIR=C:/SDK with spaces/crt/libcrtweb", command)


if __name__ == "__main__":
    unittest.main()
