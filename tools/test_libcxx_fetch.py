#!/usr/bin/env python3
"""Focused tests for imported-libc++ sparse fetch recovery."""

import importlib.util
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("crt-libcxx-build.py")
sys.path.insert(0, str(SCRIPT.parent))
SPEC = importlib.util.spec_from_file_location("crt_libcxx_build", SCRIPT)
assert SPEC and SPEC.loader
DRIVER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DRIVER)


class LongPathCopyTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp(prefix='libcxx fetch actual spaces '))
        self.addCleanup(lambda: shutil.rmtree(DRIVER.filesystem_path(self.root)))

    def test_deep_primary_and_sibling_source_bytes_are_preserved(self):
        relative = Path(*['upstream directory ' + str(i) + 'x' * 40 for i in range(5)]) / 'pinned.cpp'
        for name in ('libcxx', 'cmake'):
            source = self.root / 'clone' / name
            file = Path(DRIVER.filesystem_path(source / relative))
            file.parent.mkdir(parents=True)
            file.write_bytes(b'unchanged pinned upstream source')
            (source / '.git').mkdir()
            (source / '.git/private').write_text('not source')
            cache = self.root / 'actual source cache with spaces'
            target = cache / name
            DRIVER.copy_fetched_tree(source, target, cache)
            copied = Path(DRIVER.filesystem_path(target / relative))
            self.assertGreater(len(str(target / relative)), 350)
            self.assertEqual(file.read_bytes(), copied.read_bytes())
            self.assertFalse((target / '.git').exists())
            self.assertEqual([], list(cache.glob('.copy-*')))

    def test_copy_failure_never_publishes_a_partial_checkout(self):
        from unittest.mock import patch
        cache = self.root / 'cache'
        def partial(source, destination, **kwargs):
            destination.mkdir()
            (destination / 'incomplete').write_text('partial')
            raise OSError('injected copy failure')
        with patch.object(DRIVER, 'copy_tree', side_effect=partial):
            with self.assertRaisesRegex(OSError, 'injected copy failure'):
                DRIVER.copy_fetched_tree(self.root / 'source', cache / 'libcxx', cache)
        self.assertFalse((cache / 'libcxx').exists())
        self.assertEqual([], list(cache.iterdir()))

    def test_failed_replacement_preserves_existing_checkout(self):
        from unittest.mock import patch
        cache = self.root / 'cache'
        target = cache / 'libcxx'
        target.mkdir(parents=True)
        (target / 'complete.cpp').write_text('old complete source')
        with patch.object(DRIVER, 'copy_tree', side_effect=OSError('injected')):
            with self.assertRaises(OSError):
                DRIVER.copy_fetched_tree(self.root / 'source', target, cache)
        self.assertEqual('old complete source', (target / 'complete.cpp').read_text())
        self.assertEqual([], list(cache.glob('.copy-*')))

    def test_copy_cannot_replace_cache_root_or_outside_directory(self):
        cache = self.root / 'cache'
        cache.mkdir()
        for target in (cache, self.root / 'outside'):
            with self.assertRaisesRegex(ValueError, 'inside its source cache'):
                DRIVER.copy_fetched_tree(self.root / 'source', target, cache)

    def test_isolated_cxx_source_asset_already_carries_shared_file_helper(self):
        import create_stage_source
        self.assertIn('tools/crt_file_ops.py', create_stage_source.STAGES['02-cxx']['project_paths'])


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
        # Keep TemporaryDirectory's read-only Git-object cleanup while opting
        # its file IO into long paths; external Git arguments stayed ordinary.
        self.temporary.name = DRIVER.filesystem_path(self.root)
        self.temporary.cleanup()

    def test_sparse_fetch_copies_deep_primary_and_extra_paths(self) -> None:
        relative = Path(*['deep upstream ' + str(i) + 'x' * 42 for i in range(5)]) / 'original.cpp'
        for name in ('libunwind', 'cmake', 'runtimes/cmake'):
            path = Path(DRIVER.filesystem_path(self.repository / name / relative))
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(name.encode())
        subprocess.run(['git', '-c', 'core.longpaths=true', 'add', '.'], cwd=self.repository, check=True)
        subprocess.run(['git', 'commit', '--quiet', '-m', 'deep pinned source'], cwd=self.repository, check=True)
        self.recipe['source']['ref'] = subprocess.check_output(
            ['git', 'rev-parse', 'HEAD'], cwd=self.repository, text=True).strip()
        DRIVER.fetch_recipe(self.recipe, self.source_root)
        for name in ('libunwind', 'cmake', 'runtimes/cmake'):
            target = self.source_root / name / relative
            self.assertGreater(len(str(target)), 350)
            self.assertEqual(name.encode(), Path(DRIVER.filesystem_path(target)).read_bytes())
        self.assertFalse((self.source_root / 'libunwind/.git').exists())

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

    def test_existing_primary_with_missing_sibling_recovers_instead_of_skipping(self):
        from unittest.mock import patch
        DRIVER.fetch_recipe(self.recipe, self.source_root)
        shutil.rmtree(self.source_root / 'cmake')
        self.assertTrue((self.source_root / 'libunwind/CMakeLists.txt').is_file())
        with patch.object(DRIVER, 'run', wraps=DRIVER.run) as run:
            DRIVER.fetch_recipe(self.recipe, self.source_root)
        self.assertTrue((self.source_root / 'cmake/Modules/HandleCompilerRT.cmake').is_file())
        self.assertTrue(run.called)
        self.assertFalse(any('fetch' in call.args[0] for call in run.call_args_list))


if __name__ == "__main__":
    unittest.main()
