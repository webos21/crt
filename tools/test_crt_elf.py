#!/usr/bin/env python3
"""Focused tests for dependency-free ELF runtime-path handling."""

import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from crt_elf import archive_machine_ids, machine_id, relocate_absolute_runtime_paths, remove_absolute_runtime_paths, runtime_paths, set_runtime_paths
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
    def test_archive_checks_all_members_and_long_names(self):
        def member(name, data):
            header = (name.ljust(16) + "0".ljust(12) + "0".ljust(6) + "0".ljust(6) +
                      "100644".ljust(8) + str(len(data)).ljust(10) + "`\n").encode()
            return header + data + (b"\n" if len(data) & 1 else b"")
        def elf(machine):
            return b"\x7fELF" + bytes((2, 1, 1)) + bytes(11) + machine.to_bytes(2, "little")
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / "target.a"
            symbols = member("/", b"symbol table") + member("//", b"long-name.o/\n")
            target = b"!<arch>\n" + symbols + member("/0", elf(183))
            file.write_bytes(target)
            self.assertEqual(archive_machine_ids(file), {183})
            # A foreign object hidden behind BSD's long-name prefix is not
            # forgiven just because the first archive member is AArch64.
            file.write_bytes(target + member("#1/4", b"host" + elf(62)))
            self.assertEqual(archive_machine_ids(file), {62, 183})
            for bad in (b"!<thin>\n", target[:-1], b"!<arch>\n" + member("x.o/", b"not ELF")):
                file.write_bytes(bad)
                with self.assertRaises(ValueError):
                    archive_machine_ids(file)

    def test_machine_id_reads_target_identity_and_rejects_truncated_elf(self):
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / "fixture"
            file.write_bytes(b"plain text")
            self.assertIsNone(machine_id(file))
            write_elf(file, "$ORIGIN")
            self.assertEqual(machine_id(file), 62)
            data = bytearray(file.read_bytes())
            data[18:20] = (183).to_bytes(2, "little")
            file.write_bytes(data)
            self.assertEqual(machine_id(file), 183)
            file.write_bytes(b"\x7fELF")
            with self.assertRaisesRegex(ValueError, "invalid ELF"):
                machine_id(file)

    def test_set_mixed_runtime_path_for_private_package_layout(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / "libcrtweb.so"
            write_elf(binary, "$ORIGIN:/a/long/build/tree/lib:/another/private/build/path")
            size = binary.stat().st_size
            self.assertTrue(set_runtime_paths(binary, "$ORIGIN:$ORIGIN/../libexec/crtweb/lib"))
            self.assertEqual(runtime_paths(binary), ["$ORIGIN:$ORIGIN/../libexec/crtweb/lib"])
            self.assertEqual(binary.stat().st_size, size)
            with self.assertRaises(ValueError):
                set_runtime_paths(binary, "a" * 500)
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

    def test_absolute_only_executable_runpath_is_relocated(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / "curl"
            write_elf(binary, "/temporary/stage/sdk/lib")
            original_size = binary.stat().st_size
            with self.assertRaises(ValueError):
                remove_absolute_runtime_paths(binary, require_origin=True)
            self.assertTrue(relocate_absolute_runtime_paths(binary, "$ORIGIN/../lib"))
            self.assertEqual(["$ORIGIN/../lib"], runtime_paths(binary))
            self.assertEqual(original_size, binary.stat().st_size)
            # Already portable now: the normal cleanup is a no-op, not an error.
            self.assertFalse(remove_absolute_runtime_paths(binary, require_origin=True))
            self.assertFalse(relocate_absolute_runtime_paths(binary, "$ORIGIN/../lib"))

    def test_relocate_leaves_origin_runpath_alone(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / "libexample.so"
            write_elf(binary, "$ORIGIN:/checkout/lib")
            self.assertFalse(relocate_absolute_runtime_paths(binary, "$ORIGIN/../lib"))
            self.assertEqual(["$ORIGIN:/checkout/lib"], runtime_paths(binary))

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
