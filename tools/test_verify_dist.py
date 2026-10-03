#!/usr/bin/env python3
"""Focused unit tests for host-independent distribution validation."""

import copy
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from crt_dist_prerequisites import external_prerequisites_for
from verify_dist import (
    validate_external_prerequisites,
    validate_manifest_schema,
    validate_redistributed_dependencies,
    validate_ui_stage,
)


def valid_manifest() -> dict:
    return {
        "format": 1,
        "stage": "04-gfx-media",
        "target": {"os": "windows", "arch": "x86_64"},
        "target_triple": "x86_64-w64-mingw32",
        "created_utc": "2026-09-14T00:00:00+00:00",
        "compiler_bundled": False,
        "external_tools_used_to_build": {
            "cc": "clang.exe",
            "cxx": "clang++.exe",
            "ar": "llvm-ar.exe",
            "ranlib": "llvm-ranlib.exe",
        },
        "default_compile_options": ["-ffreestanding"],
        "external_toolchain_environment": ["CRT_CC"],
        "external_prerequisites": external_prerequisites_for(
            "windows", "04-gfx-media"),
        "redistributed_dependencies": [],
    }


class ManifestSchemaTests(unittest.TestCase):
    def test_valid_manifest(self) -> None:
        manifest = valid_manifest()
        self.assertIs(validate_manifest_schema(manifest, "04-gfx-media"), manifest)

    def test_rejects_wrong_format_stage_and_target(self) -> None:
        cases = (
            ("format", 2),
            ("stage", "06-web"),
            ("target", {"os": "android", "arch": "x86_64"}),
            ("target", {"os": "windows", "arch": ""}),
        )
        for field, value in cases:
            with self.subTest(field=field, value=value):
                manifest = valid_manifest()
                manifest[field] = value
                with self.assertRaises(SystemExit):
                    validate_manifest_schema(manifest, "04-gfx-media")

    def test_rejects_incomplete_toolchain_identity(self) -> None:
        manifest = valid_manifest()
        manifest["external_tools_used_to_build"]["ar"] = ""
        with self.assertRaises(SystemExit):
            validate_manifest_schema(manifest, "04-gfx-media")


class RedistributedDependencyTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.dist = Path(self.temporary.name)
        for relative in (
                "include/example/example.h", "lib/libexample.a",
                "share/licenses/example/LICENSE",
                "share/crt/dependencies/example/recipe.json"):
            path = self.dist / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("test", encoding="utf-8")
        self.manifest = valid_manifest()
        self.manifest["redistributed_dependencies"] = [{
            "name": "example",
            "kind": "private-static-port",
            "headers": ["include/example"],
            "link_artifacts": ["lib/libexample.a"],
            "runtime_artifacts": [],
            "notices": ["share/licenses/example/LICENSE"],
            "provenance": "share/crt/dependencies/example/recipe.json",
        }]

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_valid_dependency_inventory(self) -> None:
        dependencies = validate_redistributed_dependencies(self.dist, self.manifest)
        self.assertEqual(["example"], list(dependencies))

    def test_rejects_unsafe_portable_paths(self) -> None:
        unsafe = (
            "../outside", "/tmp/outside", "C:/outside", "include\\example",
            "include/./example", "include//example",
        )
        for value in unsafe:
            with self.subTest(value=value):
                manifest = copy.deepcopy(self.manifest)
                manifest["redistributed_dependencies"][0]["headers"] = [value]
                with self.assertRaises(SystemExit):
                    validate_redistributed_dependencies(self.dist, manifest)

    def test_rejects_duplicate_names_and_missing_payload(self) -> None:
        duplicate = copy.deepcopy(self.manifest)
        duplicate["redistributed_dependencies"].append(
            copy.deepcopy(duplicate["redistributed_dependencies"][0]))
        with self.assertRaises(SystemExit):
            validate_redistributed_dependencies(self.dist, duplicate)

        missing = copy.deepcopy(self.manifest)
        missing["redistributed_dependencies"][0]["headers"] = ["include/missing"]
        with self.assertRaises(SystemExit):
            validate_redistributed_dependencies(self.dist, missing)


class UiStageTests(unittest.TestCase):
    """validate_ui_stage() on a small fake tree: the ordinary in-tree 05-ui package
    (companion headers installed unconditionally, companion libraries only with
    Skia/FFmpeg) must pass, and the isolated option-ON package must be held to
    the stricter contract. Guards the 2026-10-03 check that first draft got wrong:
    it required the companion libraries of every package and would have rejected
    the legitimate Skia-OFF one."""

    DEPENDENCIES = ("freetype", "ffmpeg", "skia", "curl", "mbedtls", "zlib")

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.dist = Path(self.temporary.name)
        for relative in (
                "include/crtui/ui.h", "include/crtui/api.h", "include/crtui/crtgfx.h",
                "include/crtui/skia.h", "include/crtui/skia_media.h",
                "examples/ui-basic/main.c", "examples/ui-basic/CMakeLists.txt",
                "examples/bin/crtui_window_demo.exe",
                "lib/libcrtui.a", "lib/libcrtui_dll.dll.a"):
            self.write(relative, "int main(void) { return 0; }\n")
        self.redistributed: dict = {}
        self.manifest: dict = {}

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write(self, relative: str, text: str = "test") -> None:
        path = self.dist / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def make_isolated(self) -> None:
        for relative in ("lib/libcrtui_skia.a", "lib/libcrtui_skia_media.a",
                         "share/licenses/lvgl/LICENCE.txt",
                         "share/crt/dependencies/lvgl/recipe.json"):
            self.write(relative)
        self.manifest = {"built_from": {"stage": "04-gfx-media"}}
        names = ("lvgl",) + self.DEPENDENCIES
        self.redistributed = {
            name: {"headers": [], "link_artifacts": []} for name in names}

    def check(self) -> None:
        validate_ui_stage(self.dist, self.manifest, self.redistributed, ".exe")

    def test_ordinary_package_without_companion_libraries_passes(self) -> None:
        # Headers installed, libraries absent: the Skia-OFF in-tree package.
        self.check()

    def test_ordinary_package_still_rejects_a_missing_core_library(self) -> None:
        (self.dist / "lib" / "libcrtui.a").unlink()
        with self.assertRaises(SystemExit):
            self.check()

    def test_ordinary_package_rejects_a_leaked_lvgl_header_and_sample(self) -> None:
        self.write("include/lvgl/lvgl.h")
        with self.assertRaises(SystemExit):
            self.check()
        (self.dist / "include" / "lvgl" / "lvgl.h").unlink()
        self.write("examples/ui-basic/main.c", "lv_obj_create(NULL);\n")
        with self.assertRaises(SystemExit):
            self.check()

    def test_complete_isolated_package_passes(self) -> None:
        self.make_isolated()
        self.check()

    def test_isolated_package_requires_every_companion_and_lvgl_artifact(self) -> None:
        for relative in ("lib/libcrtui_skia.a", "lib/libcrtui_skia_media.a",
                         "include/crtui/skia.h", "include/crtui/skia_media.h",
                         "share/licenses/lvgl/LICENCE.txt",
                         "share/crt/dependencies/lvgl/recipe.json"):
            with self.subTest(missing=relative):
                self.make_isolated()
                (self.dist / relative).unlink()
                with self.assertRaises(SystemExit):
                    self.check()

    def test_isolated_package_requires_the_lvgl_record_and_keeps_it_private(self) -> None:
        self.make_isolated()
        del self.redistributed["lvgl"]
        with self.assertRaises(SystemExit):
            self.check()
        self.make_isolated()
        self.redistributed["lvgl"]["headers"] = ["include/lvgl"]
        with self.assertRaises(SystemExit):
            self.check()
        self.make_isolated()
        self.redistributed["lvgl"]["link_artifacts"] = ["lib/liblvgl.a"]
        with self.assertRaises(SystemExit):
            self.check()

    def test_isolated_package_must_keep_every_04_dependency_record(self) -> None:
        for name in self.DEPENDENCIES:
            with self.subTest(dropped=name):
                self.make_isolated()
                del self.redistributed[name]
                with self.assertRaises(SystemExit):
                    self.check()


class ExternalPrerequisiteTests(unittest.TestCase):
    def test_cumulative_contract_for_every_target_and_stage(self) -> None:
        for target_os in ("linux", "macos", "windows"):
            previous_ids: set[str] = set()
            for stage in ("01-c", "02-cxx", "03-gfx-simple",
                          "04-gfx-media", "05-ui"):
                with self.subTest(target_os=target_os, stage=stage):
                    manifest = valid_manifest()
                    manifest["target"]["os"] = target_os
                    manifest["stage"] = stage
                    manifest["external_prerequisites"] = (
                        external_prerequisites_for(target_os, stage))
                    validated = validate_external_prerequisites(manifest)
                    self.assertTrue(previous_ids <= set(validated))
                    previous_ids = set(validated)

    def test_rejects_missing_duplicate_and_contradictory_entries(self) -> None:
        for mutation in ("missing", "duplicate", "contradictory"):
            with self.subTest(mutation=mutation):
                manifest = valid_manifest()
                if mutation == "missing":
                    manifest["external_prerequisites"].pop()
                elif mutation == "duplicate":
                    manifest["external_prerequisites"].append(
                        copy.deepcopy(manifest["external_prerequisites"][0]))
                else:
                    manifest["external_prerequisites"][0]["bundled"] = True
                with self.assertRaises(SystemExit):
                    validate_external_prerequisites(manifest)


if __name__ == "__main__":
    unittest.main()
