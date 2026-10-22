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

    def run_step(self, name, stage, *, after=(), keys=(), outputs=(), command=None, stamp=None):
        command = command or [sys.executable, "-c", f"open({str(self.marker)!r}, 'a').write('x')"]
        args = Namespace(name=name, stage=stage, state_dir=str(self.state), root=str(self.root), after=list(after),
                         key=list(keys), output=[str(o) for o in outputs], stamp=stamp)
        with contextlib.redirect_stdout(io.StringIO()) as out:
            code = cache.run_step(args, command)
        return code, out.getvalue()

    def runs(self) -> int:
        return len(self.marker.read_text()) if self.marker.exists() else 0

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
        self.run_step("c-dist", "c", outputs=[output], keys=["cc=1"])
        self.run_step("c-dist", "c", outputs=[output], keys=["cc=1"])
        self.assertEqual(1, self.runs())
        self.run_step("c-dist", "c", outputs=[output], keys=["cc=2"])
        self.assertEqual(2, self.runs(), "a changed configuration key reruns")
        output.rmdir()
        self.run_step("c-dist", "c", outputs=[output], keys=["cc=2"])
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


if __name__ == "__main__":
    unittest.main()
