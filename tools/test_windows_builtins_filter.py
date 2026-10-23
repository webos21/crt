"""Real archiver/CMake fixture for stable compiler-rt filtering, no CRT compile."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class WindowsBuiltinsFilter(unittest.TestCase):
    def test_filter_is_stable_and_updates_only_for_changed_kept_bytes(self):
        cmake, ar = shutil.which("cmake"), shutil.which("llvm-ar")
        if not cmake or not ar:
            self.skipTest("CMake and llvm-ar are build-host requirements")
        with tempfile.TemporaryDirectory(prefix="compiler rt filter spaces ") as temp:
            root = Path(temp)
            names = ("safe.c.obj", "emutls.c.obj", "eprintf.c.obj")
            for name in names:
                (root / name).write_bytes(name.encode())
            source, output = root / "source.lib", root / "output.a"
            def run(command):
                return subprocess.run(command, check=True, capture_output=True, text=True,
                    creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            run([ar, "rcs", str(source), *[str(root / name) for name in names]])
            command = [cmake, "-DINPUT=" + str(source), "-DOUTPUT=" + str(output), "-DAR=" + ar,
                       "-P", str(ROOT / "cmake/filter_windows_builtins.cmake")]
            original = hashlib.sha256(source.read_bytes()).hexdigest()
            run(command)
            self.assertEqual(["safe.c.obj"], run([ar, "t", str(output)]).stdout.splitlines())
            first = output.read_bytes()
            stamp = output.stat().st_mtime_ns
            run(command)
            self.assertEqual(first, output.read_bytes())
            self.assertEqual(stamp, output.stat().st_mtime_ns)
            self.assertEqual(original, hashlib.sha256(source.read_bytes()).hexdigest())
            (root / "safe.c.obj").write_bytes(b"changed kept helper")
            run([ar, "r", str(source), str(root / "safe.c.obj")])
            run(command)
            self.assertNotEqual(first, output.read_bytes())
            self.assertEqual(["safe.c.obj"], run([ar, "t", str(output)]).stdout.splitlines())
