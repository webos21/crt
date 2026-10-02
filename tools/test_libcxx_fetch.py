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
