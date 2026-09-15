#!/usr/bin/env python3
"""Minimal dependency-free Mach-O load-command inspection.

Scope matches crt_elf.py/crt_pe.py's own: this project only ever builds and
ships native, host-endian (little-endian, arm64/x86_64) thin Mach-O files --
never a fat/universal archive and never a big-endian one -- so _read_macho()
below only recognizes MH_MAGIC/MH_MAGIC_64 and treats everything else
(including the byte-swapped MH_CIGAM/MH_CIGAM_64 forms and fat FAT_MAGIC
archives) the same as a non-Mach-O file: an empty/None result, not a crash.
"""

import struct
from pathlib import Path


MH_MAGIC = 0xfeedface
MH_MAGIC_64 = 0xfeedfacf

# LC_REQ_DYLD-flagged commands (dyld must understand them or refuse to load)
# carry that bit set in their own numeric value, matching mach-o/loader.h.
_LC_REQ_DYLD = 0x80000000
LC_LOAD_DYLIB = 0xc
LC_ID_DYLIB = 0xd
LC_LOAD_WEAK_DYLIB = 0x18 | _LC_REQ_DYLD
LC_RPATH = 0x1c | _LC_REQ_DYLD
LC_REEXPORT_DYLIB = 0x1f | _LC_REQ_DYLD
LC_LOAD_UPWARD_DYLIB = 0x23 | _LC_REQ_DYLD

# Every load command that records a dependency on another dylib -- as
# opposed to LC_ID_DYLIB, which is a dylib's own self-identify, not a
# dependency, and LC_RPATH, which is a search-path entry, not a dependency.
_DEPENDENCY_COMMANDS = (LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB,
                        LC_REEXPORT_DYLIB, LC_LOAD_UPWARD_DYLIB)


def _read_macho(path: Path) -> bytes | None:
    with path.open("rb") as stream:
        magic = stream.read(4)
        if len(magic) < 4 or struct.unpack("<I", magic)[0] not in (MH_MAGIC, MH_MAGIC_64):
            return None
        return magic + stream.read()


def _header(data: bytes) -> tuple[int, int, int]:
    """Return (load-commands offset, ncmds, sizeofcmds) for a native mach_header/mach_header_64.

    Both layouts place ncmds/sizeofcmds at the same offset (20/24 -- the
    64-bit form only differs by a trailing `reserved` field appended after
    `flags`, which affects nothing read here); only the load-commands
    starting offset (28 vs 32 bytes) differs.
    """
    magic = struct.unpack_from("<I", data, 0)[0]
    header_size = 32 if magic == MH_MAGIC_64 else 28
    if len(data) < header_size:
        raise ValueError("truncated Mach-O header")
    ncmds, sizeofcmds = struct.unpack_from("<II", data, 16)
    return header_size, ncmds, sizeofcmds


def _lc_string(data: bytes, cmd_start: int, cmdsize: int) -> str:
    """Decode a `union lc_str` field: a uint32 byte offset (from cmd_start)
    to a NUL-terminated string, used by both dylib_command's own `name` and
    rpath_command's own `path` -- both place that offset at the same byte
    8 within the command, right after the common cmd/cmdsize header."""
    if cmd_start + 12 > len(data):
        raise ValueError("Mach-O load command is outside the file")
    name_offset = struct.unpack_from("<I", data, cmd_start + 8)[0]
    start = cmd_start + name_offset
    limit = cmd_start + cmdsize
    if name_offset < 8 or start >= limit or limit > len(data):
        raise ValueError("Mach-O load-command string is outside its own command")
    end = data.find(b"\0", start, limit)
    if end < 0:
        raise ValueError("Mach-O load-command string is not NUL-terminated")
    return data[start:end].decode("utf-8")


def _load_commands(data: bytes):
    """Yield (cmd, cmdsize, offset) for every load command in the file."""
    offset, ncmds, sizeofcmds = _header(data)
    limit = offset + sizeofcmds
    if limit > len(data):
        raise ValueError("Mach-O load commands extend past the file")
    for _ in range(ncmds):
        if offset + 8 > limit:
            raise ValueError("Mach-O load command is outside its own table")
        cmd, cmdsize = struct.unpack_from("<II", data, offset)
        if cmdsize < 8 or offset + cmdsize > limit:
            raise ValueError("Mach-O load command has an invalid size")
        yield cmd, cmdsize, offset
        offset += cmdsize


def dependencies(path: Path) -> list[str]:
    """Return every LC_LOAD_DYLIB/LC_LOAD_WEAK_DYLIB/LC_REEXPORT_DYLIB/
    LC_LOAD_UPWARD_DYLIB install-name string, or [] for a non-Mach-O file.
    Each name is exactly as ld64 recorded it -- an absolute system path
    (real frameworks/dylibs), an @rpath//@loader_path//@executable_path/-
    relative reference (this project's own bundled libraries), or, if
    something leaked one, a build-time-only absolute path."""
    data = _read_macho(path)
    if data is None:
        return []
    return [_lc_string(data, offset, cmdsize)
            for cmd, cmdsize, offset in _load_commands(data)
            if cmd in _DEPENDENCY_COMMANDS]


def dylib_id(path: Path) -> str | None:
    """Return this file's own LC_ID_DYLIB name, or None for a file that
    is not a dylib (an executable, a bundle, or a non-Mach-O file)."""
    data = _read_macho(path)
    if data is None:
        return None
    for cmd, cmdsize, offset in _load_commands(data):
        if cmd == LC_ID_DYLIB:
            return _lc_string(data, offset, cmdsize)
    return None


def rpaths(path: Path) -> list[str]:
    """Return every LC_RPATH search-path string, or [] for a non-Mach-O file."""
    data = _read_macho(path)
    if data is None:
        return []
    return [_lc_string(data, offset, cmdsize)
            for cmd, cmdsize, offset in _load_commands(data)
            if cmd == LC_RPATH]
