#!/usr/bin/env python3
"""Focused tests for imported-libc++ predecessor invalidation."""

import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("crt-libcxx-build.py")
SPEC = importlib.util.spec_from_file_location("crt_libcxx_build", SCRIPT)
assert SPEC and SPEC.loader
DRIVER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DRIVER)


class PredecessorFingerprintTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.sysroot = self.root / "sysroot"
        self.build_root = self.root / "build"
        (self.sysroot / "lib").mkdir(parents=True)
        for name in ("libc.so", "libc.a", "libm.so", "libdl.so"):
            (self.sysroot / "lib" / name).write_bytes((name + "-v1").encode())
        self.order = [{"name": "libunwind"}, {"name": "libcxxabi"}, {"name": "libcxx"}]

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def make_build_trees(self) -> None:
        for recipe in self.order:
            directory = self.build_root / recipe["name"]
            directory.mkdir(parents=True)
            (directory / "sentinel").write_text("configured", encoding="utf-8")

    def test_unchanged_predecessor_preserves_build_trees(self) -> None:
        fingerprint = DRIVER.predecessor_fingerprint(self.sysroot)
        DRIVER.write_predecessor_fingerprint(self.build_root, fingerprint)
        self.make_build_trees()

        DRIVER.invalidate_stale_builds(self.build_root, self.order, fingerprint)

        for recipe in self.order:
            self.assertTrue((self.build_root / recipe["name"] / "sentinel").is_file())

    def test_changed_predecessor_removes_only_recipe_build_trees(self) -> None:
        original = DRIVER.predecessor_fingerprint(self.sysroot)
        DRIVER.write_predecessor_fingerprint(self.build_root, original)
        self.make_build_trees()
        unrelated = self.build_root / "keep-me"
        unrelated.mkdir(parents=True)
        (self.sysroot / "lib" / "libc.so").write_bytes(b"libc-v2")

        changed = DRIVER.predecessor_fingerprint(self.sysroot)
        DRIVER.invalidate_stale_builds(self.build_root, self.order, changed)

        for recipe in self.order:
            self.assertFalse((self.build_root / recipe["name"]).exists())
        self.assertTrue(unrelated.is_dir())

    def test_missing_predecessor_libraries_is_rejected(self) -> None:
        empty = self.root / "empty"
        (empty / "lib").mkdir(parents=True)
        with self.assertRaises(SystemExit):
            DRIVER.predecessor_fingerprint(empty)


if __name__ == "__main__":
    unittest.main()
