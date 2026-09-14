#!/usr/bin/env python3
"""Focused unit tests for host-independent distribution validation."""

import copy
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from verify_dist import validate_manifest_schema, validate_redistributed_dependencies


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
        "redistributed_dependencies": [],
    }


class ManifestSchemaTests(unittest.TestCase):
    def test_valid_manifest(self) -> None:
        manifest = valid_manifest()
        self.assertIs(validate_manifest_schema(manifest, "04-gfx-media"), manifest)

    def test_rejects_wrong_format_stage_and_target(self) -> None:
        cases = (
            ("format", 2),
            ("stage", "05-js"),
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


if __name__ == "__main__":
    unittest.main()
