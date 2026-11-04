#!/usr/bin/env python3
"""Windows/aarch64 compiles drop asynchronous unwind tables (tools/crt-cc and tools/crt-c++).

clang enables them by default for aarch64-w64-mingw32 once -fhosted is given, and LLVM 22.1.8 then emits a
CFA that ignores the stack allocated after the `__chkstk` probe of a large frame, so an exception unwinding
through such a frame terminates although a handler exists (found by the Windows/arm64 D2 installer replay).
The flag is chosen from the target variables only, so these checks run on every POSIX host.
"""

import os
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
FLAG = "-fno-asynchronous-unwind-tables"


@unittest.skipIf(os.name == "nt", "POSIX shell wrappers")
class WindowsUnwindTablesTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        self.sysroot = root / "sysroot"
        (self.sysroot / "lib").mkdir(parents=True)
        (self.sysroot / "include").mkdir()
        fake = root / "fake-compiler"
        fake.write_text('#!/bin/sh\nprintf \'%s\\n\' "$@"\n')
        fake.chmod(fake.stat().st_mode | stat.S_IXUSR)
        self.fake = fake
        (root / "t.c").write_text("int main(void) { return 0; }\n")
        (root / "t.cc").write_text("int main() { return 0; }\n")
        self.root = root

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def arguments(self, wrapper: str, target_os: str, arch: str) -> list:
        env = {"PATH": os.environ["PATH"], "CRT_SYSROOT": str(self.sysroot), "CRT_TARGET_OS": target_os,
               "CRT_TARGET_ARCH": arch, "CRT_HOST_CC": str(self.fake), "CRT_HOST_CXX": str(self.fake)}
        source = str(self.root / ("t.cc" if wrapper == "crt-c++" else "t.c"))
        done = subprocess.run([str(TOOLS / wrapper), "-c", source, "-o", str(self.root / "t.o")],
                              env=env, capture_output=True, text=True)
        self.assertEqual(0, done.returncode, done.stderr)
        return done.stdout.splitlines()

    def test_windows_aarch64_compiles_use_synchronous_tables_only(self) -> None:
        for wrapper in ("crt-cc", "crt-c++"):
            with self.subTest(wrapper=wrapper):
                self.assertEqual(1, self.arguments(wrapper, "windows", "aarch64").count(FLAG))

    def test_other_targets_are_unchanged(self) -> None:
        for wrapper in ("crt-cc", "crt-c++"):
            for target_os, arch in (("windows", "x86_64"), ("linux", "aarch64"), ("linux", "x86_64")):
                with self.subTest(wrapper=wrapper, target=f"{target_os}/{arch}"):
                    self.assertNotIn(FLAG, self.arguments(wrapper, target_os, arch))


if __name__ == "__main__":
    unittest.main()
