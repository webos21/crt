#!/usr/bin/env python3
"""Focused tests for imported-libc++ sparse fetch recovery."""

import importlib.util
import hashlib
import json
import os
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
    def test_real_libcxxabi_pristine_patched_and_repeated_application(self):
        project = SCRIPT.parent.parent
        recipe = json.loads((project / 'libstdc++/third_party/libcxxabi/recipe.json').read_text())
        patch = next(p for p in recipe['patches'] if p['file'] == 'src/cxa_exception_storage.cpp')
        recipe = dict(recipe, patches=[patch], extra_files=[])
        original = (SCRIPT.parent / 'fixtures/libcxxabi/cxa_exception_storage.cpp').read_bytes()
        with tempfile.TemporaryDirectory() as folder:
            checkout = Path(folder)
            target = checkout / patch['file']
            target.parent.mkdir()
            target.write_bytes(original)
            DRIVER.apply_patches(recipe, checkout)
            result = target.read_bytes()
            self.assertEqual(hashlib.sha256(result).hexdigest(),
                             recipe['source_checks'][patch['file']]['patched_sha256'])
            stamp = target.stat().st_mtime_ns
            for _ in range(24):
                DRIVER.apply_patches(recipe, checkout)
            self.assertEqual(target.read_bytes(), result)
            self.assertEqual(target.stat().st_mtime_ns, stamp)

    def test_real_libcxxabi_polluted_and_unrelated_drift_rejected_before_writes(self):
        project = SCRIPT.parent.parent
        recipe = json.loads((project / 'libstdc++/third_party/libcxxabi/recipe.json').read_text())
        original = (SCRIPT.parent / 'fixtures/libcxxabi/cxa_exception_storage.cpp').read_bytes()
        for content in (original.replace(b'#elif __has_feature(cxx_thread_local)',
                                         b'#elif __has_feature(cxx_thread_local)' + b' && !defined(_WIN32)' * 24),
                        original + b'// unexpected cached change\n'):
            with self.subTest(polluted=content.count(b'&& !defined(_WIN32)')):
                with tempfile.TemporaryDirectory() as folder:
                    checkout = Path(folder)
                    target = checkout / 'src/cxa_exception_storage.cpp'
                    target.parent.mkdir()
                    target.write_bytes(content)
                    with self.assertRaisesRegex(SystemExit, 'noncanonical source cache'):
                        DRIVER.apply_patches(recipe, checkout)
                    self.assertEqual(target.read_bytes(), content)
                    self.assertFalse((checkout / 'src/__crt_dso_handle.cpp').exists())

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
            path.write_text(relative + "\n", encoding="utf-8", newline="\n")
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
            # The depth under test is the relative part, which alone exceeds Windows' 260-character MAX_PATH
            # whatever the temporary directory is called; the full length depends on the host (337 on the Linux
            # CI runners), so it is not asserted.
            self.assertGreater(len(str(relative)), 260)
            self.assertGreater(len(str(target)), 260)
            self.assertEqual(name.encode(), Path(DRIVER.filesystem_path(target)).read_bytes())
        self.assertFalse((self.source_root / 'libunwind/.git').exists())

    def test_host_crlf_defaults_do_not_change_pinned_source_bytes(self):
        from unittest.mock import patch
        from crt_libcxx_sources import canonical_runtime_sources
        raw_files = {'libunwind/pinned-crlf.txt': b'preserve pinned CRLF\r\n',
                     'cmake/pinned-binary.bin': b'\x00preserve binary\r\n\xff'}
        for relative, data in raw_files.items():
            (self.repository / relative).write_bytes(data)
        subprocess.run(['git', '-c', 'core.autocrlf=false', 'add', '.'], cwd=self.repository, check=True)
        subprocess.run(['git', 'commit', '--quiet', '-m', 'pinned raw bytes'], cwd=self.repository, check=True)
        self.recipe['source']['ref'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=self.repository, text=True).strip()
        config = {
            'GIT_CONFIG_COUNT': '2',
            'GIT_CONFIG_KEY_0': 'core.autocrlf', 'GIT_CONFIG_VALUE_0': 'true',
            'GIT_CONFIG_KEY_1': 'core.eol', 'GIT_CONFIG_VALUE_1': 'crlf',
        }
        with patch.dict(os.environ, config):
            DRIVER.fetch_recipe(self.recipe, self.source_root)
            for relative in ('libunwind/CMakeLists.txt', 'cmake/Modules/HandleCompilerRT.cmake',
                             'runtimes/cmake/Modules/HandleFlags.cmake'):
                expected = subprocess.check_output(['git', 'show', f'{self.ref}:{relative}'], cwd=self.repository)
                self.assertEqual((relative + '\n').encode(), expected)
                self.assertEqual(expected, (self.source_root / relative).read_bytes())
            for relative, data in raw_files.items():
                self.assertEqual(data, (self.source_root / relative).read_bytes())
            recipe_path = self.root / 'project/libstdc++/third_party/libunwind/recipe.json'
            recipe_path.parent.mkdir(parents=True)
            recipe_path.write_text(json.dumps(self.recipe), encoding='utf-8', newline='\n')
            canonical, _ = canonical_runtime_sources(self.root / 'project', self.source_root, 'linux')
            self.assertEqual(b'cmake/Modules/HandleCompilerRT.cmake\n', canonical['cmake/Modules/HandleCompilerRT.cmake'])
            for relative, data in raw_files.items():
                self.assertEqual(data, canonical[relative])
            sibling = self.source_root / 'cmake/Modules/HandleCompilerRT.cmake'
            drift = sibling.read_bytes().replace(b'\n', b'\r\n')
            sibling.write_bytes(drift)
            with self.assertRaisesRegex(SystemExit, 'noncanonical runtime source cache'):
                canonical_runtime_sources(self.root / 'project', self.source_root, 'linux')
            self.assertEqual(drift, sibling.read_bytes())
            # Fetch overrides are command-local, not persistent user/cache config edits.
            clone = self.source_root / '.clone-libunwind'
            self.assertEqual('true', subprocess.check_output(['git', 'config', 'core.autocrlf'], cwd=clone, text=True).strip())
            self.assertEqual('crlf', subprocess.check_output(['git', 'config', 'core.eol'], cwd=clone, text=True).strip())

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

    def test_existing_fetch_rejects_polluted_cache_without_touching_it(self):
        DRIVER.fetch_recipe(self.recipe, self.source_root)
        file = self.source_root / 'libunwind/CMakeLists.txt'
        good = file.read_bytes()
        self.recipe['source_checks'] = {'CMakeLists.txt': {'original_sha256': hashlib.sha256(good).hexdigest()}}
        file.write_bytes(good + b'pollution')
        with self.assertRaisesRegex(SystemExit, 'noncanonical source cache'):
            DRIVER.fetch_recipe(self.recipe, self.source_root)
        self.assertEqual(file.read_bytes(), good + b'pollution')

    def test_canonical_packaging_verifies_whole_cache_and_normalizes_patch_state(self):
        from crt_libcxx_sources import canonical_runtime_sources
        DRIVER.fetch_recipe(self.recipe, self.source_root)
        self.recipe['patches'] = [{'file': 'CMakeLists.txt', 'find': 'libunwind/', 'replace': 'patched/'}]
        self.recipe['extra_files'] = [{'path': 'shim.cpp', 'content': 'owned shim\n'}]
        recipe_path = self.root / 'project/libstdc++/third_party/libunwind/recipe.json'
        recipe_path.parent.mkdir(parents=True)
        recipe_path.write_text(json.dumps(self.recipe))
        canonical, receipt = canonical_runtime_sources(self.root / 'project', self.source_root, 'linux')
        DRIVER.apply_patches(self.recipe, self.source_root / 'libunwind')
        second, second_receipt = canonical_runtime_sources(self.root / 'project', self.source_root, 'linux')
        self.assertEqual((canonical, receipt), (second, second_receipt))
        for relative in ('libunwind/CMakeLists.txt', 'cmake/Modules/HandleCompilerRT.cmake'):
            file = self.source_root / relative
            original = file.read_bytes()
            file.write_bytes(original + b'polluted')
            with self.assertRaisesRegex(SystemExit, 'noncanonical runtime source cache'):
                canonical_runtime_sources(self.root / 'project', self.source_root, 'linux')
            file.write_bytes(original)
        unexpected = self.source_root / 'libunwind/untracked.cpp'
        unexpected.write_bytes(b'not upstream')
        with self.assertRaisesRegex(SystemExit, 'noncanonical runtime source inventory'):
            canonical_runtime_sources(self.root / 'project', self.source_root, 'linux')


if __name__ == "__main__":
    unittest.main()
