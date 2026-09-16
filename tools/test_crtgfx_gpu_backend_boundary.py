#!/usr/bin/env python3
"""Regression guard for the in-progress libcrtgfx GPU boundary migration.

The migration starts from a deliberately explicit inventory.  Until the fixed
wrapper work removes the known Skia/test exceptions, fail if another source
file gains gpu_internal.h access or concrete backend-field access.  Each
backend tranche shrinks these sets; completion makes NON_OWNER_FIELD_USERS
empty and removes Skia/the generic smoke from INTERNAL_HEADER_USERS.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCAN_ROOTS = (
    ROOT / "libcrtgfx" / "src",
    ROOT / "libcrtgfx" / "tests",
    ROOT / "libcrtgfx" / "tools",
    ROOT / "examples",
)

INTERNAL_HEADER_USERS = {
    "libcrtgfx/src/gpu.c",
    "libcrtgfx/src/skia_bridge.cc",
    "libcrtgfx/src/arch/linux/gpu_vulkan.c",
    "libcrtgfx/src/arch/windows/gpu_win32.c",
    "libcrtgfx/src/arch/macos/gpu_metal.c",
    "libcrtgfx/tests/skia_gpu_offscreen_smoke.cc",
}

BACKEND_OWNERS = {
    "libcrtgfx/src/arch/linux/gpu_vulkan.c",
    "libcrtgfx/src/arch/windows/gpu_win32.c",
    "libcrtgfx/src/arch/macos/gpu_metal.c",
}

LAYOUT_DEFINITION = "libcrtgfx/src/gpu_internal.h"

# Known violations being removed by the backend-specific tranches.  This is
# an allowlist that must shrink, never a general permission for these files.
NON_OWNER_FIELD_USERS = {
    "libcrtgfx/src/skia_bridge.cc",
    "libcrtgfx/tests/skia_gpu_offscreen_smoke.cc",
}

SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp", ".m", ".mm"}
INTERNAL_INCLUDE_RE = re.compile(r'^\s*#\s*include\s+["<]gpu_internal\.h[">]', re.MULTILINE)
BACKEND_FIELD_RE = re.compile(
    r"->(?:vk_|d3d12_|dxgi_|mtl_|ganesh_wrapped\b)|"
    r"->device->(?:vk_|d3d12_|dxgi_|mtl_)"
)


def source_files() -> list[Path]:
    files: list[Path] = []
    for scan_root in SCAN_ROOTS:
        if not scan_root.exists():
            continue
        files.extend(
            path
            for path in scan_root.rglob("*")
            if path.is_file()
            and path.suffix in SOURCE_SUFFIXES
            and "third_party" not in path.parts
        )
    return files


def relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


class CrtgfxGpuBackendBoundaryTest(unittest.TestCase):
    def test_private_header_users_match_the_audited_migration_set(self) -> None:
        actual = {
            relative(path)
            for path in source_files()
            if INTERNAL_INCLUDE_RE.search(path.read_text(encoding="utf-8"))
        }
        self.assertEqual(actual, INTERNAL_HEADER_USERS)

    def test_no_new_non_owner_gains_concrete_backend_field_access(self) -> None:
        actual = {
            relative(path)
            for path in source_files()
            if BACKEND_FIELD_RE.search(path.read_text(encoding="utf-8"))
            and relative(path) not in BACKEND_OWNERS
            and relative(path) != LAYOUT_DEFINITION
        }
        self.assertEqual(actual, NON_OWNER_FIELD_USERS)


if __name__ == "__main__":
    unittest.main()
