#!/usr/bin/env python3
"""Minimal dependency-free PE import-table inspection."""

import struct
from pathlib import Path


def exported_symbols(path: Path) -> list[str]:
    """Read named PE exports (including forwarders); reject malformed tables.

    No external objdump/Windows loader is needed, so package policy can also
    test PE fixtures on Linux. An ordinal-only export is reported explicitly.
    """
    data = path.read_bytes()
    if data[:2] != b"MZ":
        raise ValueError("expected a PE binary for the export audit")
    try:
        pe = struct.unpack_from("<I", data, 0x3c)[0]
        if data[pe:pe + 4] != b"PE\0\0":
            raise ValueError("invalid PE signature")
        coff = pe + 4
        count = struct.unpack_from("<H", data, coff + 2)[0]
        size = struct.unpack_from("<H", data, coff + 16)[0]
        optional = coff + 20
        kind = struct.unpack_from("<H", data, optional)[0]
        if kind not in (0x10b, 0x20b):
            raise ValueError("unsupported PE optional header")
        directory = optional + (112 if kind == 0x20b else 96)
        if optional + size > len(data) or directory + 8 > optional + size:
            raise ValueError("truncated PE optional header")
        rva, length = struct.unpack_from("<II", data, directory)
        if not rva:
            return []
        sections = []
        for index in range(count):
            pos = optional + size + index * 40
            virtual_size, address, raw_size, raw = struct.unpack_from("<IIII", data, pos + 8)
            sections.append((address, raw_size, raw))

        def offset(address, size=1):
            for start, raw_size, raw in sections:
                if start <= address and address + size <= start + raw_size:
                    result = raw + address - start
                    if result + size <= len(data):
                        return result
            raise ValueError("PE export RVA outside file-backed section")

        table = offset(rva, 40)
        base, functions, names, function_rva, name_rva, ordinal_rva = struct.unpack_from("<IIIIII", data, table + 16)
        if functions > len(data) // 4 or names > len(data) // 4:
            raise ValueError("invalid PE export counts")
        addresses = offset(function_rva, functions * 4)
        named = set()
        exports = []
        pointers = offset(name_rva, names * 4) if names else 0
        ordinals = offset(ordinal_rva, names * 2) if names else 0
        for index in range(names):
            ordinal = struct.unpack_from("<H", data, ordinals + index * 2)[0]
            if ordinal >= functions:
                raise ValueError("PE export ordinal outside table")
            named.add(ordinal)
            name = struct.unpack_from("<I", data, pointers + index * 4)[0]
            exports.append(_cstring(data, offset(name)))
        for index in range(functions):
            if index not in named and struct.unpack_from("<I", data, addresses + index * 4)[0]:
                exports.append(f"ordinal:{base + index}")
        return exports
    except struct.error as error:
        raise ValueError("truncated PE export table") from error


def _cstring(data: bytes, offset: int) -> str:
    if offset < 0 or offset >= len(data):
        raise ValueError("PE import name is outside the file")
    end = data.find(b"\0", offset)
    if end < 0:
        raise ValueError("PE import name is not NUL-terminated")
    return data[offset:end].decode("ascii")


def imported_libraries(path: Path) -> list[str]:
    """Return normal and delay-load DLL names, or [] for non-PE files."""
    with path.open("rb") as stream:
        magic = stream.read(2)
        if magic != b"MZ":
            return []
        data = magic + stream.read()
    if len(data) < 0x40:
        raise ValueError("truncated DOS header")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_offset + 24 > len(data) or data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise ValueError("invalid PE signature")
    coff = pe_offset + 4
    section_count = struct.unpack_from("<H", data, coff + 2)[0]
    optional_size = struct.unpack_from("<H", data, coff + 16)[0]
    optional = coff + 20
    if optional + optional_size > len(data):
        raise ValueError("PE optional header is outside the file")
    kind = struct.unpack_from("<H", data, optional)[0]
    if kind == 0x10B:
        directory_offset = optional + 96
        image_base = struct.unpack_from("<I", data, optional + 28)[0]
    elif kind == 0x20B:
        directory_offset = optional + 112
        image_base = struct.unpack_from("<Q", data, optional + 24)[0]
    else:
        raise ValueError("unsupported PE optional-header kind")
    section_table = optional + optional_size
    sections = []
    for index in range(section_count):
        offset = section_table + index * 40
        if offset + 40 > len(data):
            raise ValueError("PE section header is outside the file")
        virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from(
            "<IIII", data, offset + 8)
        sections.append((virtual_address, max(virtual_size, raw_size), raw_offset, raw_size))

    def rva_to_offset(rva: int) -> int:
        for virtual_address, mapped_size, raw_offset, raw_size in sections:
            if virtual_address <= rva < virtual_address + mapped_size:
                delta = rva - virtual_address
                if delta >= raw_size:
                    raise ValueError("PE RVA points outside section file data")
                return raw_offset + delta
        raise ValueError("PE RVA is outside every section")

    def directory(index: int) -> tuple[int, int]:
        offset = directory_offset + index * 8
        if offset + 8 > optional + optional_size:
            return 0, 0
        return struct.unpack_from("<II", data, offset)

    imports = []
    import_rva, import_size = directory(1)
    if import_rva and import_size:
        table = rva_to_offset(import_rva)
        limit = min(table + import_size, len(data))
        for offset in range(table, limit, 20):
            if offset + 20 > len(data):
                raise ValueError("PE import descriptor is outside the file")
            descriptor = struct.unpack_from("<IIIII", data, offset)
            if not any(descriptor):
                break
            imports.append(_cstring(data, rva_to_offset(descriptor[3])))

    delay_rva, delay_size = directory(13)
    if delay_rva and delay_size:
        table = rva_to_offset(delay_rva)
        limit = min(table + delay_size, len(data))
        for offset in range(table, limit, 32):
            if offset + 32 > len(data):
                raise ValueError("PE delay-import descriptor is outside the file")
            descriptor = struct.unpack_from("<IIIIIIII", data, offset)
            if not any(descriptor):
                break
            attributes, name = descriptor[:2]
            name_rva = name if attributes & 1 else name - image_base
            imports.append(_cstring(data, rva_to_offset(name_rva)))
    return list(dict.fromkeys(imports))
