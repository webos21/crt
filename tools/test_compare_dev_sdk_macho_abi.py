from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).parent))
import compare_dev_sdk_macho_abi as tool


class SymbolRows(unittest.TestCase):
    def test_exports_keep_weakness_and_section(self):
        self.assertEqual(("__Exit", "(__TEXT,__text) external", None),
                         tool.parse_symbol_row("0000000000003468 (__TEXT,__text) external __Exit"))
        self.assertEqual(("__Znwm", "(__TEXT,__text) weak external", None),
                         tool.parse_symbol_row("0000000000000010 (__TEXT,__text) weak external __Znwm"))

    def test_imports_keep_the_two_level_namespace_provider(self):
        self.assertEqual(("_dlopen", "(undefined) external", "libSystem"),
                         tool.parse_symbol_row("                 (undefined) external _dlopen (from libSystem)"))
        self.assertEqual(("_x", "(undefined) weak external", "dynamic lookup"),
                         tool.parse_symbol_row("                 (undefined) weak external _x (dynamically looked up)"))

    def test_re_exports_name_their_source(self):
        self.assertEqual(("_a", "(indirect) external", "re-export of _b"),
                         tool.parse_symbol_row("                 (indirect) external _a (for _b)"))

    def test_an_unknown_row_fails_closed(self):
        with self.assertRaises(ValueError):
            tool.parse_symbol_row("not a symbol row")


class LoadCommands(unittest.TestCase):
    def test_build_and_dylib_versions(self):
        text = """Load command 1
          cmd LC_ID_DYLIB
      cmdsize 48
         name @rpath/libc.dylib (offset 24)
      current version 1.0.0
compatibility version 1.0.0
Load command 2
      cmd LC_BUILD_VERSION
  cmdsize 32
 platform 1
    minos 27.2
      sdk 27.2
   ntools 1
     tool 3
  version 1.0
"""
        self.assertEqual({"platform": "1", "minimum_os": "27.2", "sdk": "27.2",
                          "dylib_versions": {"LC_ID_DYLIB @rpath/libc.dylib":
                                             {"current": "1.0.0", "compatibility": "1.0.0"}}},
                         tool.parse_load_commands(text))


if __name__ == "__main__":
    unittest.main()
