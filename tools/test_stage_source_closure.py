#!/usr/bin/env python3
"""Every private project header a stage-source asset's sources #include must be
bundled in that asset.

tools/create_stage_source.py lists, per stage and OS, exactly which repository
files go into the pinned source asset a packaged SDK builds the next stage from.
Nothing else from the checkout is available there.  When a source gains a new
private ``#include "..."`` and this list is not extended, the mistake stays
invisible until an isolated stage build (hours long) reaches that compile --
which is how ``gpu_test_control.h``, ``gpu_win32_test.h`` and
``codec_test_control.h`` went missing from ``04-gfx-media`` for several days.

This is a static, textual include-closure check.  It cannot see preprocessor
conditions, so a header that is only included under, say, ``#if
defined(CRT_TARGET_OS_LINUX)`` is a false positive for the other OSes and is
listed in CONDITIONAL_ON_OTHER_OSES below with the reason.
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import create_stage_source as stage_source

ROOT = Path(__file__).resolve().parents[1]
BACKSLASH = chr(92)

#: Directories quoted includes may resolve against, besides the including file's.
INCLUDE_DIRS = tuple(ROOT / path for path in (
    "libcrtgfx/src", "libcrtgfx/include", "libcrtmedia/src", "libcrtmedia/include",
    "libc/include", "libcrtgfx/tests", "libcrtmedia/tests"))

#: (stage, os) -> {header: reason} for includes guarded so the OS never compiles them.
CONDITIONAL_ON_OTHER_OSES = {
    ("04-gfx-media", "windows"): {
        "libcrtgfx/src/arch/linux/gpu_vulkan_test.h":
            "tests/gpu_test.c includes it under CRT_TARGET_OS_LINUX && CRTGFX_HAVE_VULKAN"},
    ("04-gfx-media", "macos"): {
        "libcrtgfx/src/arch/linux/gpu_vulkan_test.h":
            "tests/gpu_test.c includes it under CRT_TARGET_OS_LINUX && CRTGFX_HAVE_VULKAN"},
}

QUOTED_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
SOURCE_SUFFIXES = (".c", ".cc", ".cpp", ".h", ".m")


def relative(path: Path) -> str:
    return str(path.relative_to(ROOT)).replace(BACKSLASH, "/")


def expand(paths) -> set:
    files = set()
    for entry in paths:
        target = ROOT / entry
        if target.is_dir():
            files.update(f.resolve() for f in target.rglob("*") if f.is_file())
        elif target.is_file():
            files.add(target.resolve())
    return files


def unbundled_project_headers(stage: str, target_os: str) -> dict:
    """{header: including file} for project headers reached but not bundled."""
    spec = stage_source.STAGES[stage]
    bundled = expand(list(spec["project_paths"]) +
                     list(spec.get("project_paths_by_os", {}).get(target_os, ())))
    missing: dict = {}
    seen: set = set()
    pending = [f for f in bundled if f.suffix in SOURCE_SUFFIXES]
    while pending:
        current = pending.pop()
        if current in seen:
            continue
        seen.add(current)
        text = current.read_text(encoding="utf-8", errors="replace")
        for name in QUOTED_INCLUDE.findall(text):
            candidates = [current.parent / name] + [d / name for d in INCLUDE_DIRS]
            found = next((c.resolve() for c in candidates if c.is_file()), None)
            if found is None:
                continue  # system or third-party header
            try:
                found.relative_to(ROOT)
            except ValueError:
                continue
            if "third_party" in found.relative_to(ROOT).parts:
                continue
            if found in bundled:
                if found not in seen:
                    pending.append(found)
                continue
            missing.setdefault(relative(found), relative(current))
    return missing


class StageSourceClosure(unittest.TestCase):
    def test_every_stage_bundles_the_private_headers_its_sources_include(self):
        for stage in sorted(stage_source.STAGES):
            spec = stage_source.STAGES[stage]
            systems = set(spec.get("project_paths_by_os", {})) or {"windows", "linux", "macos"}
            for target_os in sorted(systems):
                allowed = CONDITIONAL_ON_OTHER_OSES.get((stage, target_os), {})
                missing = {header: includer for header, includer
                           in unbundled_project_headers(stage, target_os).items()
                           if header not in allowed}
                with self.subTest(stage=stage, os=target_os):
                    self.assertEqual(
                        missing, {},
                        f"{stage} ({target_os}) source asset lacks project headers "
                        "its bundled sources include; add them to "
                        "tools/create_stage_source.py")

    def test_the_headers_that_once_went_missing_are_bundled(self):
        for header in ("libcrtgfx/src/gpu_test_control.h",
                       "libcrtmedia/src/codec_test_control.h"):
            for target_os in ("windows", "linux", "macos"):
                spec = stage_source.STAGES["04-gfx-media"]
                bundled = expand(list(spec["project_paths"]) +
                                 list(spec["project_paths_by_os"][target_os]))
                self.assertIn((ROOT / header).resolve(), bundled, (header, target_os))
        windows = expand(stage_source.STAGES["04-gfx-media"]["project_paths_by_os"]["windows"])
        self.assertIn((ROOT / "libcrtgfx/src/arch/windows/gpu_win32_test.h").resolve(), windows)
        linux = expand(stage_source.STAGES["04-gfx-media"]["project_paths_by_os"]["linux"])
        self.assertIn((ROOT / "libcrtgfx/src/arch/linux/gpu_vulkan_test.h").resolve(), linux)

    def test_the_stage_links_every_macos_framework_the_in_tree_build_does(self):
        # The isolated 04-gfx-media project keeps its own copy of the macOS
        # framework list.  CoreFoundation was added only to libcrtmedia's copy,
        # so the release tag could not link libcrtmedia.dylib on macOS.
        def frameworks(relative_path: str) -> set:
            text = (ROOT / relative_path).read_text(encoding="utf-8")
            block = re.search(r"set\(CRTMEDIA_MACOS_FRAMEWORKS\b(.*?)\)", text, re.S)
            self.assertIsNotNone(block, relative_path)
            return set(re.findall(r'"-framework (\w+)"', block.group(1)))

        in_tree = frameworks("libcrtmedia/CMakeLists.txt")
        stage = frameworks("distribution/stages/04-gfx-media/CMakeLists.txt")
        self.assertIn("CoreFoundation", in_tree)
        self.assertEqual(in_tree - stage, set(),
                         "distribution/stages/04-gfx-media/CMakeLists.txt is missing "
                         "macOS frameworks that libcrtmedia/CMakeLists.txt links")

    def test_the_allowlist_only_names_headers_that_really_are_unbundled(self):
        # A stale allowlist entry would hide a future genuine miss.
        for (stage, target_os), allowed in CONDITIONAL_ON_OTHER_OSES.items():
            missing = unbundled_project_headers(stage, target_os)
            for header in allowed:
                with self.subTest(stage=stage, os=target_os, header=header):
                    self.assertIn(header, missing)


if __name__ == "__main__":
    unittest.main()
