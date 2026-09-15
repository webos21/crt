#!/usr/bin/env python3
"""Focused tests for dependency-free ELF runtime-path handling."""

import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from crt_elf import remove_absolute_runtime_paths, runtime_paths
from verify_dist import validate_elf_runtime_paths


def write_elf(path: Path, runpath: str) -> None:
    """Write the smallest ELF64 fixture needed by the parser."""
    data = bytearray(512)
    ident = b"\x7fELF" + bytes((2, 1, 1, 0)) + bytes(8)
    struct.pack_into(
        "<16sHHIQQQIHHHHHH", data, 0, ident, 3, 62, 1, 0, 64, 0, 0,
        64, 56, 2, 0, 0, 0,
    )
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 4, 0, 0x400000, 0, 512, 512, 0x1000)
    struct.pack_into("<IIQQQQQQ", data, 120, 2, 6, 0x100, 0x400100, 0, 64, 64, 8)
    encoded = b"\0" + runpath.encode("utf-8") + b"\0"
    struct.pack_into("<QQ", data, 0x100, 5, 0x400180)
    struct.pack_into("<QQ", data, 0x110, 10, len(encoded))
    struct.pack_into("<QQ", data, 0x120, 29, 1)
    struct.pack_into("<QQ", data, 0x130, 0, 0)
    data[0x180:0x180 + len(encoded)] = encoded
    path.write_bytes(data)


class ElfRuntimePathTests(unittest.TestCase):
    def test_rewrite_removes_absolute_fallback(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / "libexample.so"
            write_elf(binary, "$ORIGIN:/checkout/out/dist/01-c/lib")
            original_size = binary.stat().st_size
            self.assertTrue(remove_absolute_runtime_paths(binary, require_origin=True))
            self.assertEqual(["$ORIGIN"], runtime_paths(binary))
            self.assertEqual(original_size, binary.stat().st_size)

    def test_distribution_rejects_absolute_runpath(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            dist = Path(temporary)
            binary = dist / "lib" / "libexample.so"
            binary.parent.mkdir()
            write_elf(binary, "$ORIGIN:/checkout/lib")
            with self.assertRaises(SystemExit):
                validate_elf_runtime_paths(dist)
            remove_absolute_runtime_paths(binary, require_origin=True)
            validate_elf_runtime_paths(dist)

    def test_non_elf_is_ignored(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            archive = Path(temporary) / "libexample.a"
            archive.write_bytes(b"!<arch>\n")
            self.assertEqual([], runtime_paths(archive))
            self.assertFalse(remove_absolute_runtime_paths(archive))

    def test_absolute_only_runpath_can_be_cleared(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / "make"
            write_elf(binary, "/temporary/port-prefix/lib")
            self.assertTrue(remove_absolute_runtime_paths(binary))
            self.assertEqual([""], runtime_paths(binary))

    def test_relocatable_elf_without_program_headers_is_ignored(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            obj = Path(temporary) / "crt1.o"
            data = bytearray(64)
            ident = b"\x7fELF" + bytes((2, 1, 1, 0)) + bytes(8)
            struct.pack_into(
                "<16sHHIQQQIHHHHHH", data, 0, ident, 1, 62, 1, 0, 0, 0, 0,
                64, 0, 0, 0, 0, 0,
            )
            obj.write_bytes(data)
            self.assertEqual([], runtime_paths(obj))


if __name__ == "__main__":
    unittest.main()
