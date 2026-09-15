#!/usr/bin/env python3
"""Tests for ELF/PE dependency parsing and manifest classification."""

import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from crt_binary_dependencies import validate_binary_dependencies
from crt_elf import needed_libraries
from crt_pe import imported_libraries
from create_stage_source import STAGES, STAGE_VALIDATION_PROJECT_PATHS


def write_elf(path: Path, needed: list[str]) -> None:
    data = bytearray(768)
    ident = b"\x7fELF" + bytes((2, 1, 1, 0)) + bytes(8)
    struct.pack_into(
        "<16sHHIQQQIHHHHHH", data, 0, ident, 3, 62, 1, 0, 64, 0, 0,
        64, 56, 2, 0, 0, 0,
    )
    strings = bytearray(b"\0")
    offsets = []
    for name in needed:
        offsets.append(len(strings))
        strings.extend(name.encode("utf-8") + b"\0")
    entries = [(5, 0x400200), (10, len(strings))]
    entries.extend((1, offset) for offset in offsets)
    entries.append((0, 0))
    dynamic_size = len(entries) * 16
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 4, 0, 0x400000, 0, 768, 768, 0x1000)
    struct.pack_into(
        "<IIQQQQQQ", data, 120, 2, 6, 0x100, 0x400100, 0,
        dynamic_size, dynamic_size, 8,
    )
    for index, entry in enumerate(entries):
        struct.pack_into("<QQ", data, 0x100 + index * 16, *entry)
    data[0x200:0x200 + len(strings)] = strings
    path.write_bytes(data)


def write_pe(path: Path, imports: list[str], delay_imports: list[str] | None = None) -> None:
    delay_imports = delay_imports or []
    data = bytearray(0x800)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    coff = 0x84
    struct.pack_into("<HHIIIHH", data, coff, 0x8664, 1, 0, 0, 0, 240, 0x22)
    optional = coff + 20
    struct.pack_into("<H", data, optional, 0x20B)
    struct.pack_into("<Q", data, optional + 24, 0x140000000)
    struct.pack_into("<I", data, optional + 108, 16)
    if imports:
        struct.pack_into("<II", data, optional + 112 + 8, 0x1000, (len(imports) + 1) * 20)
    if delay_imports:
        struct.pack_into("<II", data, optional + 112 + 13 * 8,
                         0x1100, (len(delay_imports) + 1) * 32)
    section = optional + 240
    data[section:section + 8] = b".rdata\0\0"
    struct.pack_into("<IIII", data, section + 8, 0x600, 0x1000, 0x600, 0x200)

    next_name_rva = 0x1300
    for index, name in enumerate(imports):
        struct.pack_into("<IIIII", data, 0x200 + index * 20, 0, 0, 0,
                         next_name_rva, 0)
        encoded = name.encode("ascii") + b"\0"
        offset = 0x200 + next_name_rva - 0x1000
        data[offset:offset + len(encoded)] = encoded
        next_name_rva += len(encoded)
    for index, name in enumerate(delay_imports):
        struct.pack_into("<IIIIIIII", data, 0x300 + index * 32, 1,
                         next_name_rva, 0, 0, 0, 0, 0, 0)
        encoded = name.encode("ascii") + b"\0"
        offset = 0x200 + next_name_rva - 0x1000
        data[offset:offset + len(encoded)] = encoded
        next_name_rva += len(encoded)
    path.write_bytes(data)


def manifest(target_os: str, components: list[str]) -> dict:
    return {
        "target": {"os": target_os},
        "external_prerequisites": [{"components": components}],
    }


class BinaryDependencyTests(unittest.TestCase):
    def test_every_stage_asset_carries_validator_import_closure(self) -> None:
        required = set(STAGE_VALIDATION_PROJECT_PATHS)
        for stage, spec in STAGES.items():
            with self.subTest(stage=stage):
                self.assertTrue(required.issubset(spec["project_paths"]))

    def test_elf_needed_parser_and_classification(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            dist = Path(temporary)
            app = dist / "bin" / "app"
            app.parent.mkdir()
            write_elf(app, ["libc.so", "libatomic.so.1"])
            write_elf(dist / "libc.so", [])
            self.assertEqual(["libc.so", "libatomic.so.1"], needed_libraries(app))
            validate_binary_dependencies(dist, manifest("linux", ["libatomic.so.1"]))

    def test_pe_normal_and_delay_imports_are_case_insensitive(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            dist = Path(temporary)
            app = dist / "bin" / "app.exe"
            app.parent.mkdir()
            write_pe(app, ["LIBC.DLL", "KERNEL32.dll"], ["D3D12.dll"])
            write_pe(dist / "libc.dll", [])
            self.assertEqual(
                ["LIBC.DLL", "KERNEL32.dll", "D3D12.dll"],
                imported_libraries(app),
            )
            validate_binary_dependencies(
                dist, manifest("windows", ["kernel32.DLL", "d3d12.DLL"]))

    def test_undeclared_and_embedded_path_dependencies_fail(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            dist = Path(temporary)
            app = dist / "app"
            write_elf(app, ["libmissing.so"])
            (dist / "libmissing.so").write_text("not a runtime library", encoding="utf-8")
            with self.assertRaises(SystemExit):
                validate_binary_dependencies(dist, manifest("linux", []))
            write_elf(app, ["/checkout/libbad.so"])
            (dist / "libbad.so").write_bytes(b"bundled name must not excuse path")
            with self.assertRaises(SystemExit):
                validate_binary_dependencies(dist, manifest("linux", []))


if __name__ == "__main__":
    unittest.main()
