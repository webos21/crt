#!/usr/bin/env python3
"""Parser and comparison behaviour of compare_dev_sdk_pe_abi.py (R05-01)."""

import struct
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))

from compare_dev_sdk_pe_abi import compare, parse_imports, parse_machines, pe_machine

READOBJ_IMPORTS = """
File: a.exe
Import {
  Name: KERNEL32.dll
  ImportLookupTableRVA: 0x1000
  Symbol: ExitProcess (123)
  Symbol:  (7)
}
DelayImport {
  Name: libc.dll
  Symbol: malloc (0)
}
"""


class PeAbiParserTests(unittest.TestCase):
    def test_imports_keep_dll_and_symbol_for_normal_ordinal_and_delay_blocks(self):
        self.assertEqual([["kernel32.dll", "ExitProcess"], ["kernel32.dll", "ordinal:7"], ["libc.dll", "malloc"]],
                         parse_imports(READOBJ_IMPORTS))

    def test_machines_are_collected_from_every_member(self):
        text = "Machine: IMAGE_FILE_MACHINE_AMD64 (0x8664)\n  Machine: IMAGE_FILE_MACHINE_ARM64 (0xAA64)\n"
        self.assertEqual({"IMAGE_FILE_MACHINE_AMD64", "IMAGE_FILE_MACHINE_ARM64"}, parse_machines(text))

    def test_pe_machine_reads_the_coff_header_and_rejects_garbage(self):
        data = bytearray(0x100)
        data[:2] = b"MZ"
        struct.pack_into("<I", data, 0x3C, 0x80)
        data[0x80:0x84] = b"PE\0\0"
        struct.pack_into("<H", data, 0x84, 0x8664)
        self.assertEqual(0x8664, pe_machine(bytes(data)))
        with self.assertRaises(ValueError):
            pe_machine(b"MZ" + bytes(0x100))

    def test_removed_and_added_names_are_reported_per_image(self):
        old = {"binaries": {"bin/libc.dll": {"sha256": "a", "exports": [["malloc"], ["old"]],
                                             "imports": [["kernel32.dll", "A"]]}}, "aliases": {}}
        new = {"binaries": {"bin/libc.dll": {"sha256": "b", "exports": [["malloc"], ["new"]],
                                             "imports": [["kernel32.dll", "A"]]},
                            "bin/x.dll": {"sha256": "c", "exports": [], "imports": []}}, "aliases": {}}
        delta = compare(old, new)
        self.assertEqual(["bin/x.dll"], delta["added_images"])
        self.assertEqual({"removed": [("old",)], "added": [("new",)]}, delta["changed_metadata"]["bin/libc.dll"]["exports"])
        self.assertNotIn("imports", delta["changed_metadata"]["bin/libc.dll"])
        self.assertEqual(["bin/libc.dll"], delta["changed_binary_bytes"])


if __name__ == "__main__":
    unittest.main()
