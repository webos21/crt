#!/usr/bin/env python3
"""Focused tests for the isolated 04-stage reusable install-layer cache."""

import json
import shutil
import stat
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import build_stage_04_gfx_media as stage04


class Stage04CacheTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_root = Path(tempfile.mkdtemp(prefix="crt-stage-cache-test-"))

    def tearDown(self) -> None:
        shutil.rmtree(self.temp_root, ignore_errors=True)

    def write_manifest(self, root: Path, created_utc: str) -> None:
        root.mkdir(parents=True, exist_ok=True)
        (root / "manifest.json").write_text(json.dumps({
            "format": 1,
            "stage": "03-gfx-simple",
            "target": {"os": "windows", "arch": "x86_64"},
            "created_utc": created_utc,
        }), encoding="utf-8")

    def test_default_dependency_jobs_is_bounded(self) -> None:
        self.assertGreaterEqual(stage04.DEFAULT_DEPENDENCY_JOBS, 1)
        self.assertLessEqual(stage04.DEFAULT_DEPENDENCY_JOBS, 4)

    def test_sdk_fingerprint_ignores_only_manifest_creation_time(self) -> None:
        first = self.temp_root / "first"
        second = self.temp_root / "second"
        self.write_manifest(first, "2026-09-11T00:00:00Z")
        self.write_manifest(second, "2026-09-12T00:00:00Z")
        (first / "include").mkdir()
        (second / "include").mkdir()
        (first / "include" / "crt.h").write_text("same\n", encoding="utf-8")
        (second / "include" / "crt.h").write_text("same\n", encoding="utf-8")

        first_key = stage04.inventory_fingerprint(
            stage04.tree_inventory(first, normalize_manifest=True))
        second_key = stage04.inventory_fingerprint(
            stage04.tree_inventory(second, normalize_manifest=True))
        self.assertEqual(first_key, second_key)

        (second / "include" / "crt.h").write_text("changed\n", encoding="utf-8")
        changed_key = stage04.inventory_fingerprint(
            stage04.tree_inventory(second, normalize_manifest=True))
        self.assertNotEqual(first_key, changed_key)

    def test_install_layer_replays_changes_on_a_clean_sdk(self) -> None:
        staged = self.temp_root / "staged"
        clean = self.temp_root / "clean"
        layer = self.temp_root / "layer"
        (staged / "include").mkdir(parents=True)
        (staged / "lib").mkdir()
        (staged / "include" / "replace.h").write_text("old\n", encoding="utf-8")
        (staged / "lib" / "remove.a").write_bytes(b"remove")
        shutil.copytree(staged, clean)
        before = stage04.tree_inventory(staged)

        (staged / "include" / "replace.h").write_text("new\n", encoding="utf-8")
        (staged / "include" / "added.h").write_text("added\n", encoding="utf-8")
        (staged / "lib" / "remove.a").unlink()
        stage04.capture_install_layer(staged, before, layer)
        stage04.apply_install_layer(layer, clean)

        self.assertEqual(stage04.tree_inventory(staged),
                         stage04.tree_inventory(clean))

    def test_corrupt_layer_is_rejected_before_application(self) -> None:
        staged = self.temp_root / "staged"
        clean = self.temp_root / "clean"
        layer = self.temp_root / "layer"
        staged.mkdir()
        clean.mkdir()
        before = stage04.tree_inventory(staged)
        (staged / "added.txt").write_text("expected\n", encoding="utf-8")
        stage04.capture_install_layer(staged, before, layer)
        (layer / "added.txt").write_text("corrupt\n", encoding="utf-8")

        with self.assertRaises(SystemExit):
            stage04.apply_install_layer(layer, clean)
        self.assertFalse((clean / "added.txt").exists())

    def test_dependency_jobs_reaches_port_driver(self) -> None:
        manifest = {"target": {"os": "windows", "arch": "x86_64"}}
        with mock.patch.object(stage04.subprocess, "run") as run_mock:
            stage04.build_ports(
                self.temp_root / "asset", self.temp_root / "sources",
                self.temp_root / "sdk", self.temp_root / "work",
                manifest, {}, 7)
        command = run_mock.call_args.args[0]
        jobs_index = command.index("--jobs")
        self.assertEqual(command[jobs_index + 1], "7")

    def test_layer_parent_traversal_is_rejected(self) -> None:
        with self.assertRaises(SystemExit):
            stage04.safe_layer_path(self.temp_root, "../outside")

    def test_readonly_work_tree_can_be_invalidated(self) -> None:
        work = self.temp_root / "readonly-work"
        work.mkdir()
        generated = work / "generated.pc"
        generated.write_text("prefix=/old\n", encoding="utf-8")
        generated.chmod(stat.S_IREAD)
        stage04.remove_tree(work)
        self.assertFalse(work.exists())


if __name__ == "__main__":
    unittest.main()
