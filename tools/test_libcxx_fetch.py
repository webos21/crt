#!/usr/bin/env python3
"""Focused tests for imported-libc++ sparse fetch recovery."""

import importlib.util
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("crt-libcxx-build.py")
SPEC = importlib.util.spec_from_file_location("crt_libcxx_build", SCRIPT)
assert SPEC and SPEC.loader
DRIVER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DRIVER)


class SourcePatchTests(unittest.TestCase):
    def test_replacement_containing_original_is_idempotent(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source = root / "source.cpp"
            original = "#elif __has_feature(cxx_thread_local)"
            replacement = original + " && !defined(_WIN32)"
            source.write_text(original + "\n", encoding="utf-8")
            recipe = {"name": "fixture", "patches": [
                {"file": source.name, "find": original, "replace": replacement}]}
            DRIVER.apply_patches(recipe, root)
            stamp = source.stat().st_mtime_ns
            for _ in range(3):
                DRIVER.apply_patches(recipe, root)
            self.assertEqual(source.read_text(), replacement + "\n")
            self.assertEqual(source.stat().st_mtime_ns, stamp)

    def test_missing_nonempty_replacement_still_rejects_drift(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "source.cpp").write_text("unrelated upstream\n", encoding="utf-8")
            recipe = {"name": "fixture", "patches": [
                {"file": "source.cpp", "find": "expected", "replace": "patched"}]}
            with self.assertRaisesRegex(SystemExit, "patch target text not found"):
                DRIVER.apply_patches(recipe, root)

    def test_deletion_remains_idempotent(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source = root / "source.cpp"
            source.write_text("remove me\nkeep me\n", encoding="utf-8")
            recipe = {"name": "fixture", "patches": [
                {"file": source.name, "find": "remove me\n", "replace": ""}]}
            DRIVER.apply_patches(recipe, root)
            DRIVER.apply_patches(recipe, root)
            self.assertEqual(source.read_text(), "keep me\n")


class SparseFetchRecoveryTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.repository = self.root / "repository"
        self.source_root = self.root / "sources"
        self.repository.mkdir()
        subprocess.run(["git", "init", "--quiet"], cwd=self.repository, check=True)
        subprocess.run(["git", "config", "user.name", "CRT Test"], cwd=self.repository, check=True)
        subprocess.run(["git", "config", "user.email", "crt-test@example.invalid"], cwd=self.repository, check=True)
        for relative in (
            "libunwind/CMakeLists.txt",
            "cmake/Modules/HandleCompilerRT.cmake",
            "runtimes/cmake/Modules/HandleFlags.cmake",
        ):
            path = self.repository / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(relative + "\n", encoding="utf-8")
        subprocess.run(["git", "add", "."], cwd=self.repository, check=True)
        subprocess.run(["git", "commit", "--quiet", "-m", "fixture"], cwd=self.repository, check=True)
        self.ref = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=self.repository,
            check=True,
            text=True,
            stdout=subprocess.PIPE,
        ).stdout.strip()
        self.recipe = {
            "name": "libunwind",
            "source": {
                "type": "git",
                "repository": str(self.repository),
                "ref": self.ref,
                "sparse_paths": ["libunwind", "cmake", "runtimes/cmake"],
                "checkout_subdir": "libunwind",
                "extra_checkout_dirs": ["cmake", "runtimes/cmake"],
            },
        }

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_missing_sparse_worktree_recovers_from_cached_commit(self) -> None:
        DRIVER.fetch_recipe(self.recipe, self.source_root)
        clone = self.source_root / ".clone-libunwind"

        shutil.rmtree(self.source_root / "libunwind")
        shutil.rmtree(self.source_root / "cmake")
        shutil.rmtree(self.source_root / "runtimes")
        shutil.rmtree(clone / "libunwind")
        shutil.rmtree(clone / "cmake")
        shutil.rmtree(clone / "runtimes")

        DRIVER.fetch_recipe(self.recipe, self.source_root)

        self.assertTrue((self.source_root / "libunwind" / "CMakeLists.txt").is_file())
        self.assertTrue((self.source_root / "cmake" / "Modules" / "HandleCompilerRT.cmake").is_file())
        self.assertTrue((self.source_root / "runtimes" / "cmake" / "Modules" / "HandleFlags.cmake").is_file())


if __name__ == "__main__":
    unittest.main()
