#!/usr/bin/env python3
"""Tests for tools/crt_stage_cache.py."""

import contextlib
import io
import json
import os
import sys
import tempfile
import unittest
from argparse import Namespace
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import crt_stage_cache as cache


EXECUTABLE_SUFFIX = ".cmd" if os.name == "nt" else ""


class ToolIdentity(unittest.TestCase):
    """The cache key follows which tool files are found, not the PATH string."""

    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.first, self.second, self.unrelated = (self.base / n for n in ("first", "second", "unrelated"))
        for directory in (self.first, self.second, self.unrelated):
            directory.mkdir()

    def tool(self, directory: Path, name: str = "faketool", text: str = "#!/bin/sh\n") -> Path:
        # shutil.which only finds a Windows file whose extension is in PATHEXT, so the fake program is a .cmd there.
        path = directory / (name + EXECUTABLE_SUFFIX)
        path.write_text(text)
        path.chmod(0o755)
        return path

    def identity(self, *directories: Path, names=("faketool",)) -> str:
        return cache.tool_identity(names, os.pathsep.join(str(d) for d in directories))

    def test_unrelated_and_reordered_directories_do_not_change_the_identity(self) -> None:
        self.tool(self.first)
        baseline = self.identity(self.first)
        self.assertEqual(baseline, self.identity(self.unrelated, self.first))
        self.assertEqual(baseline, self.identity(self.first, self.unrelated))
        self.assertEqual(baseline, self.identity(self.unrelated, self.first, self.unrelated))

    def test_a_different_file_found_first_changes_the_identity(self) -> None:
        self.tool(self.first)
        self.tool(self.second)
        self.assertNotEqual(self.identity(self.first, self.second), self.identity(self.second, self.first))

    def test_a_modified_tool_changes_the_identity(self) -> None:
        path = self.tool(self.first)
        before = self.identity(self.first)
        path.write_text("#!/bin/sh\necho changed\n")
        self.assertNotEqual(before, self.identity(self.first))

    def test_a_missing_tool_is_recorded_and_installing_it_changes_the_identity(self) -> None:
        absent = self.identity(self.first)
        self.assertEqual(absent, self.identity(self.unrelated, self.first), "still absent")
        self.tool(self.first)
        self.assertNotEqual(absent, self.identity(self.first))

    @unittest.skipIf(os.name == "nt", "needs POSIX symlinks")
    def test_a_symlink_is_identified_by_its_target_not_its_name(self) -> None:
        real = self.tool(self.first, "realtool")
        (self.second / "faketool").symlink_to(real)
        (self.unrelated / "faketool").symlink_to(real)
        self.assertEqual(self.identity(self.second), self.identity(self.unrelated))
        other = self.tool(self.first, "othertool", "#!/bin/sh\necho other\n")
        (self.unrelated / "faketool").unlink()
        (self.unrelated / "faketool").symlink_to(other)
        self.assertNotEqual(self.identity(self.second), self.identity(self.unrelated))

    def test_only_listed_programs_count(self) -> None:
        self.tool(self.first, "listed")
        self.tool(self.first, "unlisted")
        before = self.identity(self.first, names=("listed",))
        (self.first / ("unlisted" + EXECUTABLE_SUFFIX)).write_text("changed")
        self.assertEqual(before, self.identity(self.first, names=("listed",)))


class StageCache(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "repo"
        (self.root / "libc").mkdir(parents=True)
        (self.root / "libui").mkdir()
        (self.root / "tools").mkdir()
        (self.root / "libc/a.c").write_text("a\n")
        (self.root / "libui/u.c").write_text("u\n")
        (self.root / "tools/test_x.py").write_text("t\n")
        config = Path(self.temp.name) / "stage_cache.json"
        config.write_text(json.dumps({"stages": {
            "c": {"inputs": ["libc", "tools", "tools/*.py"]}, "ui": {"inputs": ["libui"], "includes": ["c"]}}}))
        self.patch = (cache.CONFIG, os.environ.pop("CRT_STAGE_CACHE", None))
        cache.CONFIG = config
        self.addCleanup(self.restore)
        self.state = Path(self.temp.name) / "state"
        self.marker = Path(self.temp.name) / "ran"

    def restore(self) -> None:
        cache.CONFIG = self.patch[0]
        if self.patch[1] is not None:
            os.environ["CRT_STAGE_CACHE"] = self.patch[1]

    def run_step(self, name, stage, *, after=(), keys=(), outputs=(), inputs=(), config_files=(), command=None, stamp=None):
        command = command or [sys.executable, "-c", f"open({str(self.marker)!r}, 'a').write('x')"]
        args = Namespace(name=name, stage=stage, state_dir=str(self.state), root=str(self.root), after=list(after),
                         key=list(keys), output=[str(o) for o in outputs], input=list(inputs),
                         config_file=list(config_files), stamp=stamp)
        with contextlib.redirect_stdout(io.StringIO()) as out:
            code = cache.run_step(args, command)
        return code, out.getvalue()

    def runs(self) -> int:
        return len(self.marker.read_text()) if self.marker.exists() else 0

    def test_the_path_string_is_not_a_key_but_the_resolved_tools_are(self) -> None:
        from unittest.mock import patch
        bin_a, bin_b, other = (Path(self.temp.name) / n for n in ("bin_a", "bin_b", "other"))
        for directory in (bin_a, bin_b, other):
            directory.mkdir()
        for directory in (bin_a, bin_b):
            (directory / ("faketool" + EXECUTABLE_SUFFIX)).write_text("#!/bin/sh\n")
            (directory / ("faketool" + EXECUTABLE_SUFFIX)).chmod(0o755)
        (bin_b / ("faketool" + EXECUTABLE_SUFFIX)).write_text("#!/bin/sh\necho second\n")
        config = json.loads(cache.CONFIG.read_text())
        config["tool_identity"] = {"programs": ["faketool"]}
        cache.CONFIG.write_text(json.dumps(config))
        with patch.dict(os.environ, {"PATH": f"{bin_a}"}):
            self.run_step("tooled", "c")
        with patch.dict(os.environ, {"PATH": f"{other}{os.pathsep}{bin_a}{os.pathsep}/nowhere"}):
            self.run_step("tooled", "c")
        self.assertEqual(1, self.runs(), "a different PATH string that resolves the same tool must not rerun")
        with patch.dict(os.environ, {"PATH": f"{bin_b}{os.pathsep}{bin_a}"}):
            self.run_step("tooled", "c")
        self.assertEqual(2, self.runs(), "a PATH that resolves a different tool file must rerun")

    def test_second_run_with_the_same_inputs_is_skipped(self) -> None:
        self.assertEqual(0, self.run_step("c-dist", "c")[0])
        code, text = self.run_step("c-dist", "c")
        self.assertEqual(0, code)
        self.assertIn("up to date", text)
        self.assertEqual(1, self.runs())

    def test_editing_a_source_reruns_but_a_touch_does_not(self) -> None:
        self.run_step("c-dist", "c")
        source = self.root / "libc/a.c"
        os.utime(source, (source.stat().st_atime + 100, source.stat().st_mtime + 100))
        self.run_step("c-dist", "c")
        self.assertEqual(1, self.runs(), "an mtime-only change must not rerun (content hash is unchanged)")
        source.write_text("changed\n")
        self.run_step("c-dist", "c")
        self.assertEqual(2, self.runs())

    def test_tests_are_not_inputs(self) -> None:
        self.run_step("c-dist", "c")
        (self.root / "tools/test_x.py").write_text("edited test\n")
        self.run_step("c-dist", "c")
        self.assertEqual(1, self.runs())

    def test_upstream_change_invalidates_downstream_only(self) -> None:
        self.run_step("c-dist", "c")
        self.run_step("ui-dist", "ui", after=["c-dist"])
        self.assertEqual(2, self.runs())
        (self.root / "libui/u.c").write_text("ui edit\n")
        self.run_step("c-dist", "c")
        self.run_step("ui-dist", "ui", after=["c-dist"])
        self.assertEqual(3, self.runs(), "editing libui reruns only the ui step")
        (self.root / "libc/a.c").write_text("libc edit\n")
        self.run_step("c-dist", "c")
        self.run_step("ui-dist", "ui", after=["c-dist"])
        self.assertEqual(5, self.runs(), "editing libc reruns the stage and everything after it")

    def test_includes_make_a_stage_depend_on_upstream_sources_without_running_them(self) -> None:
        self.run_step("ui-test", "ui")
        self.run_step("ui-test", "ui")
        self.assertEqual(1, self.runs())
        (self.root / "libc/a.c").write_text("libc edit\n")
        self.run_step("ui-test", "ui")
        self.assertEqual(2, self.runs(), "ui includes c, so a libc edit reruns ui even though c-dist never ran")

    def test_sequential_commands_stop_at_the_first_failure(self) -> None:
        ok = [sys.executable, "-c", f"open({str(self.marker)!r}, 'a').write('x')"]
        bad = [sys.executable, "-c", "raise SystemExit(5)"]
        code, _ = self.run_step("seq", "c", command=ok + ["++"] + bad + ["++"] + ok)
        self.assertEqual(5, code)
        self.assertEqual(1, self.runs())

    def test_a_failed_run_is_not_recorded_and_forgets_the_older_success(self) -> None:
        self.run_step("c-dist", "c")
        (self.root / "libc/a.c").write_text("broken\n")
        code, _ = self.run_step("c-dist", "c", command=[sys.executable, "-c", "raise SystemExit(3)"])
        self.assertEqual(3, code)
        self.run_step("c-dist", "c", command=[sys.executable, "-c", "raise SystemExit(3)"])
        self.assertEqual(1, self.runs())
        self.assertEqual({}, cache.read_state(self.state, "c-dist"))

    def test_missing_output_and_key_changes_rerun(self) -> None:
        output = Path(self.temp.name) / "dist"
        output.mkdir()
        command = [sys.executable, "-c", f"from pathlib import Path; Path({str(output)!r}).mkdir(exist_ok=True); "
                   f"open({str(self.marker)!r}, 'a').write('x')"]
        self.run_step("c-dist", "c", outputs=[output], keys=["cc=1"], command=command)
        self.run_step("c-dist", "c", outputs=[output], keys=["cc=1"], command=command)
        self.assertEqual(1, self.runs())
        self.run_step("c-dist", "c", outputs=[output], keys=["cc=2"], command=command)
        self.assertEqual(2, self.runs(), "a changed configuration key reruns")
        output.rmdir()
        self.run_step("c-dist", "c", outputs=[output], keys=["cc=2"], command=command)
        self.assertEqual(3, self.runs(), "a missing declared output reruns")

    def test_cache_can_be_disabled_and_stamp_is_touched(self) -> None:
        stamp = Path(self.temp.name) / "stamp"
        self.run_step("c-dist", "c", stamp=str(stamp))
        self.assertTrue(stamp.exists())
        os.environ["CRT_STAGE_CACHE"] = "off"
        try:
            self.run_step("c-dist", "c")
        finally:
            del os.environ["CRT_STAGE_CACHE"]
        self.assertEqual(2, self.runs())

    def test_glob_inputs_exclude_tests(self) -> None:
        (self.root / "tools/real.py").write_text("x\n")
        names = [name for name, _ in cache.expand(self.root, ["tools/*.py"])]
        self.assertEqual(["tools/real.py"], names)

    def test_deleted_or_corrupt_nested_payload_reruns(self):
        output = Path(self.temp.name) / "sdk"
        command = [sys.executable, "-c", f"from pathlib import Path; p=Path({str(output)!r}); p.mkdir(exist_ok=True); "
                   f"(p/'runtime.dll').write_text('valid'); open({str(self.marker)!r}, 'a').write('x')"]
        self.run_step("payload", "c", outputs=[output], command=command)
        self.run_step("payload", "c", outputs=[output], command=command)
        self.assertEqual(1, self.runs())
        (output / "runtime.dll").unlink()
        self.run_step("payload", "c", outputs=[output], command=command)
        self.assertEqual(2, self.runs())
        (output / "runtime.dll").write_text("corrupt")
        self.run_step("payload", "c", outputs=[output], command=command)
        self.assertEqual(3, self.runs())

    def test_executed_test_driver_is_hashed_despite_glob_exclusion(self):
        driver = self.root / "tools/test_driver.py"
        driver.write_text(f"open({str(self.marker)!r}, 'a').write('x')")
        command = [sys.executable, str(driver)]
        self.run_step("driver", "c", command=command)
        self.run_step("driver", "c", command=command)
        self.assertEqual(1, self.runs())
        driver.write_text("raise SystemExit(7)")
        self.assertEqual(7, self.run_step("driver", "c", command=command)[0])
        self.assertEqual({}, cache.read_state(self.state, "driver"))

    def test_changed_predecessor_config_and_env_are_inputs(self):
        predecessor = Path(self.temp.name) / "predecessor.dll"
        predecessor.write_text("original")
        config_file = Path(self.temp.name) / "CMakeCache.txt"
        config_file.write_text("CMAKE_C_FLAGS:STRING=-O0")
        options = dict(inputs=[predecessor], config_files=[config_file])
        self.run_step("inputs", "c", **options)
        self.run_step("inputs", "c", **options)
        self.assertEqual(1, self.runs())
        predecessor.write_text("updated")
        self.run_step("inputs", "c", **options)
        config_file.write_text("CMAKE_C_FLAGS:STRING=-O2")
        self.run_step("inputs", "c", **options)
        from unittest.mock import patch
        with patch.dict(os.environ, {"CRT_CXX_RUNTIME_LINKAGE": "shared"}):
            self.run_step("inputs", "c", **options)
        self.assertEqual(4, self.runs())

    def test_missing_declared_output_is_failure_even_with_zero_exit(self):
        code, _ = self.run_step("missing", "c", outputs=[Path(self.temp.name) / "absent"])
        self.assertEqual(1, code)
        self.assertEqual({}, cache.read_state(self.state, "missing"))

    def test_spawn_failure_invalidates_old_success(self):
        self.run_step("spawn", "c")
        with self.assertRaises(OSError):
            self.run_step("spawn", "c", command=[str(self.root / "missing.exe")])
        self.assertEqual({}, cache.read_state(self.state, "spawn"))

    def test_symlink_target_change_invalidates_inventory(self):
        root = Path(self.temp.name)
        (root / "first").write_text("one")
        (root / "second").write_text("two")
        alias = root / "alias"
        try:
            alias.symlink_to("first")
        except OSError as error:
            self.skipTest(f"native symlinks unavailable: {error}")
        before = cache.path_inventory([alias], cache.FileHashes(None))
        alias.unlink()
        alias.symlink_to("second")
        self.assertNotEqual(before, cache.path_inventory([alias], cache.FileHashes(None)))

    def test_changed_source_during_command_does_not_cache_a_false_success(self):
        source = self.root / "libc/a.c"
        command = [sys.executable, "-c", f"open({str(source)!r}, 'w').write('changed during execution')"]
        self.assertEqual(0, self.run_step("racing", "c", command=command)[0])
        self.assertEqual({}, cache.read_state(self.state, "racing"))

    def test_packaging_commit_changes_without_invalidating_unchanged_tests(self):
        from unittest.mock import patch
        with patch.object(cache, "producer_commit", return_value="a" * 40):
            self.run_step("c-dist", "c")
            self.run_step("c-dist", "c")
            self.run_step("c-test", "c")
        self.assertEqual(2, self.runs())
        with patch.object(cache, "producer_commit", return_value="b" * 40):
            self.run_step("c-dist", "c")
            self.run_step("c-test", "c")
        self.assertEqual(3, self.runs())


if __name__ == "__main__":
    unittest.main()
