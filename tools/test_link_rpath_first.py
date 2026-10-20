#!/usr/bin/env python3
"""CRT_LINK_RPATH_FIRST in tools/crt-cc and tools/crt-c++ (Web gate: port libcurl before the SDK's)."""

import os
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
SYSROOT_RPATH = "-Wl,-rpath,{}/lib"


@unittest.skipIf(os.name == "nt", "POSIX shell wrappers")
class LinkRpathFirstTests(unittest.TestCase):
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

    def arguments(self, wrapper: str, extra: list, first: str | None) -> list:
        env = {"PATH": os.environ["PATH"], "CRT_SYSROOT": str(self.sysroot), "CRT_TARGET_OS": "linux",
               "CRT_HOST_CC": str(self.fake), "CRT_HOST_CXX": str(self.fake),
               "CRT_CXX_RUNTIME_LINKAGE": "shared"}
        if first is not None:
            env["CRT_LINK_RPATH_FIRST"] = first
        source = str(self.root / ("t.cc" if wrapper == "crt-c++" else "t.c"))
        done = subprocess.run([str(TOOLS / wrapper), source, *extra, "-o", str(self.root / "t.out")],
                              env=env, capture_output=True, text=True)
        return done.stdout.splitlines()

    def test_link_puts_the_requested_directories_before_the_sysroot(self) -> None:
        for wrapper in ("crt-cc", "crt-c++"):
            with self.subTest(wrapper=wrapper):
                args = self.arguments(wrapper, [], "/a/lib:/b/lib")
                rpaths = [a for a in args if a.startswith("-Wl,-rpath,")]
                self.assertEqual("-Wl,-rpath,/a/lib:/b/lib", rpaths[0])
                if wrapper == "crt-c++":  # crt-cc links the static libc by default: no sysroot RPATH
                    self.assertIn(SYSROOT_RPATH.format(self.sysroot), rpaths[1:])

    def test_unset_changes_nothing(self) -> None:
        for wrapper in ("crt-cc", "crt-c++"):
            with self.subTest(wrapper=wrapper):
                args = self.arguments(wrapper, [], None)
                self.assertNotIn("-Wl,-rpath,/a/lib:/b/lib", args)
                if wrapper == "crt-c++":
                    self.assertIn(SYSROOT_RPATH.format(self.sysroot), args)

    def test_compile_only_is_left_alone(self) -> None:
        for wrapper in ("crt-cc", "crt-c++"):
            with self.subTest(wrapper=wrapper):
                args = self.arguments(wrapper, ["-c"], "/a/lib")
                self.assertFalse([a for a in args if a.startswith("-Wl,-rpath,")])


if __name__ == "__main__":
    unittest.main()
