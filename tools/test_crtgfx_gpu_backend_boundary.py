#!/usr/bin/env python3
"""Regression guard for the libcrtgfx GPU backend ownership boundary."""

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
}

BACKEND_OWNERS = {
    "libcrtgfx/src/arch/linux/gpu_vulkan.c",
    "libcrtgfx/src/arch/windows/gpu_win32.c",
    "libcrtgfx/src/arch/macos/gpu_metal.c",
}

LAYOUT_DEFINITION = "libcrtgfx/src/gpu_internal.h"

# All three concrete backend layouts are owner-local.
NON_OWNER_FIELD_USERS: set[str] = set()

SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp", ".m", ".mm"}
INTERNAL_INCLUDE_RE = re.compile(r'^\s*#\s*include\s+["<]gpu_internal\.h[">]', re.MULTILINE)
BACKEND_FIELD_RE = re.compile(
    r"->(?:vk_|d3d12_|dxgi_|mtl_|ganesh_wrapped\b)|"
    r"->device->(?:vk_|d3d12_|dxgi_|mtl_)"
)

# Backend-specific guards let each completed tranche close its own boundary
# even while later backends still have audited migration exceptions.
D3D12_FIELD_RE = re.compile(r"->(?:d3d12_|dxgi_)|->device->(?:d3d12_|dxgi_)")
METAL_FIELD_RE = re.compile(r"->mtl_|->device->mtl_")

COMMON_LAYOUT_RE = re.compile(
    r"struct crtgfx_gpu_(?:device|surface)\s*\{(?P<body>.*?)\n\};",
    re.DOTALL,
)
CONDITIONAL_RE = re.compile(r"^\s*#\s*(?:if|ifdef|ifndef|elif|else|endif)\b", re.MULTILINE)
DIRECTORY_HAVE_DEFINE_RE = re.compile(
    r"\badd_compile_definitions\s*\([^)]*CRTGFX_HAVE_", re.DOTALL
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
    def test_common_object_layouts_do_not_contain_feature_conditionals(self) -> None:
        header = (ROOT / "libcrtgfx" / "src" / "gpu_internal.h").read_text(encoding="utf-8")
        layouts = list(COMMON_LAYOUT_RE.finditer(header))
        self.assertEqual(len(layouts), 2)
        for layout in layouts:
            self.assertIsNone(CONDITIONAL_RE.search(layout.group("body")))

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

    def test_d3d12_concrete_fields_stay_in_the_windows_owner(self) -> None:
        actual = {
            relative(path)
            for path in source_files()
            if D3D12_FIELD_RE.search(path.read_text(encoding="utf-8"))
            and relative(path) not in BACKEND_OWNERS
            and relative(path) != LAYOUT_DEFINITION
        }
        self.assertEqual(actual, set())

    def test_metal_concrete_fields_stay_in_the_macos_owner(self) -> None:
        actual = {
            relative(path)
            for path in source_files()
            if METAL_FIELD_RE.search(path.read_text(encoding="utf-8"))
            and relative(path) not in BACKEND_OWNERS
            and relative(path) != LAYOUT_DEFINITION
        }
        self.assertEqual(actual, set())

    def test_generic_gpu_smoke_uses_only_backend_neutral_test_control(self) -> None:
        smoke = (ROOT / "libcrtgfx" / "tests" / "skia_gpu_offscreen_smoke.cc").read_text(
            encoding="utf-8"
        )
        self.assertIn('#include "gpu_test_control.h"', smoke)
        self.assertNotIn('#include "gpu_internal.h"', smoke)
        self.assertNotRegex(smoke, r"crtgfx_gpu_(?:vulkan|win32|metal)_(?:borrow|test_force_device_loss)")

    def test_backend_feature_defines_are_not_directory_scoped(self) -> None:
        cmake_files = [ROOT / "libcrtgfx" / "CMakeLists.txt"]
        cmake_files.extend((ROOT / "libcrtgfx" / "cmake").glob("*.cmake"))
        cmake_files.append(ROOT / "distribution" / "stages" / "04-gfx-media" / "CMakeLists.txt")
        offenders = []
        for path in cmake_files:
            text = path.read_text(encoding="utf-8")
            text_without_comments = re.sub(r"#[^\n]*", "", text)
            if DIRECTORY_HAVE_DEFINE_RE.search(text_without_comments):
                offenders.append(relative(path))
        self.assertEqual(offenders, [])


if __name__ == "__main__":
    unittest.main()
