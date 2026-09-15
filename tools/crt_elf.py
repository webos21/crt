#!/usr/bin/env python3
"""Minimal ELF runtime-search-path inspection and in-place rewriting."""

import struct
from pathlib import Path, PurePosixPath, PureWindowsPath


PT_LOAD = 1
PT_DYNAMIC = 2
DT_NULL = 0
DT_NEEDED = 1
DT_STRTAB = 5
DT_STRSZ = 10
DT_RPATH = 15
DT_RUNPATH = 29


def _layout(data: bytes):
    if data[:4] != b"\x7fELF":
        return None
    if len(data) < 64:
        raise ValueError("truncated ELF header")
    elf_class, encoding = data[4], data[5]
    if elf_class not in (1, 2) or encoding not in (1, 2):
        raise ValueError("unsupported ELF class or byte order")
    endian = "<" if encoding == 1 else ">"
    if elf_class == 1:
        phoff = struct.unpack_from(endian + "I", data, 28)[0]
        phentsize, phnum = struct.unpack_from(endian + "HH", data, 42)
        ph_format = endian + "IIIIIIII"
        dynamic_format = endian + "II"
    else:
        phoff = struct.unpack_from(endian + "Q", data, 32)[0]
        phentsize, phnum = struct.unpack_from(endian + "HH", data, 54)
        ph_format = endian + "IIQQQQQQ"
        dynamic_format = endian + "QQ"
    if phnum == 0:
        return phoff, phentsize, phnum, ph_format, dynamic_format, elf_class
    if phentsize < struct.calcsize(ph_format):
        raise ValueError("invalid ELF program-header size")
    return phoff, phentsize, phnum, ph_format, dynamic_format, elf_class


def _dynamic_string_slots(data: bytes, wanted_tags: tuple[int, ...]) -> list[tuple[int, int, str]]:
    layout = _layout(data)
    if layout is None:
        return []
    phoff, phentsize, phnum, ph_format, dynamic_format, elf_class = layout
    loads = []
    dynamic = None
    for index in range(phnum):
        offset = phoff + index * phentsize
        if offset + struct.calcsize(ph_format) > len(data):
            raise ValueError("ELF program header is outside the file")
        values = struct.unpack_from(ph_format, data, offset)
        if elf_class == 1:
            kind, file_offset, virtual_address, _, file_size = values[:5]
        else:
            kind, _, file_offset, virtual_address, _, file_size = values[:6]
        if kind == PT_LOAD:
            loads.append((virtual_address, file_offset, file_size))
        elif kind == PT_DYNAMIC:
            dynamic = (file_offset, file_size)
    if dynamic is None:
        return []

    entry_size = struct.calcsize(dynamic_format)
    tags = []
    dynamic_offset, dynamic_size = dynamic
    for offset in range(dynamic_offset, dynamic_offset + dynamic_size, entry_size):
        if offset + entry_size > len(data):
            raise ValueError("ELF dynamic table is outside the file")
        tag, value = struct.unpack_from(dynamic_format, data, offset)
        if tag == DT_NULL:
            break
        tags.append((tag, value))
    strtab_address = next((value for tag, value in tags if tag == DT_STRTAB), None)
    strtab_size = next((value for tag, value in tags if tag == DT_STRSZ), None)
    if strtab_address is None or strtab_size is None:
        return []
    strtab_offset = None
    for virtual_address, file_offset, file_size in loads:
        if virtual_address <= strtab_address < virtual_address + file_size:
            strtab_offset = file_offset + strtab_address - virtual_address
            break
    if strtab_offset is None or strtab_offset + strtab_size > len(data):
        raise ValueError("ELF dynamic string table is outside a load segment")

    slots = []
    for tag, value in tags:
        if tag not in wanted_tags:
            continue
        start = strtab_offset + value
        limit = strtab_offset + strtab_size
        if start >= limit:
            raise ValueError("ELF dynamic string is outside the string table")
        end = data.find(b"\0", start, limit)
        if end < 0:
            raise ValueError("ELF dynamic string is not NUL-terminated")
        slots.append((start, end - start, data[start:end].decode("utf-8")))
    return slots


def _runtime_path_slots(data: bytes) -> list[tuple[int, int, str]]:
    return _dynamic_string_slots(data, (DT_RPATH, DT_RUNPATH))


def _read_elf(path: Path) -> bytes | None:
    with path.open("rb") as stream:
        magic = stream.read(4)
        if magic != b"\x7fELF":
            return None
        return magic + stream.read()


def runtime_paths(path: Path) -> list[str]:
    """Return DT_RPATH/DT_RUNPATH strings, or an empty list for non-ELF files."""
    data = _read_elf(path)
    if data is None:
        return []
    return [value for _, _, value in _runtime_path_slots(data)]


def needed_libraries(path: Path) -> list[str]:
    """Return DT_NEEDED names, or an empty list for non-ELF files."""
    data = _read_elf(path)
    if data is None:
        return []
    return [value for _, _, value in _dynamic_string_slots(data, (DT_NEEDED,))]


def is_absolute_runtime_entry(entry: str) -> bool:
    """Recognize both POSIX and Windows absolute paths in an ELF path list."""
    return PurePosixPath(entry).is_absolute() or PureWindowsPath(entry).is_absolute()


def remove_absolute_runtime_paths(path: Path, require_origin: bool = False) -> bool:
    """Remove absolute ELF RPATH/RUNPATH entries without growing the file."""
    contents = _read_elf(path)
    if contents is None:
        return False
    data = bytearray(contents)
    changed = False
    for start, size, value in _runtime_path_slots(data):
        entries = value.split(":")
        rewritten = ":".join(entry for entry in entries
                             if entry and not is_absolute_runtime_entry(entry))
        if rewritten == value:
            continue
        if require_origin and "$ORIGIN" not in rewritten.split(":"):
            raise ValueError(f"refusing to remove the only usable runtime path from {path}")
        encoded = rewritten.encode("utf-8")
        if len(encoded) > size:
            raise ValueError(f"rewritten runtime path does not fit in {path}")
        # Replace only the new string and its terminator. Linkers may suffix-
        # share dynamic strings, so zeroing the whole old slot could corrupt a
        # different entry that starts inside the removed absolute fallback.
        data[start:start + len(encoded) + 1] = encoded + b"\0"
        changed = True
    if changed:
        path.write_bytes(data)
    return changed
