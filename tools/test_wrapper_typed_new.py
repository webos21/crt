#!/usr/bin/env python3
"""macOS C++ compiles through tools/crt-c++ must not use Apple clang's typed `operator new` (tools/crt-c++).

Apple clang 21 (Xcode 26) lowers `new T[n]` to `operator new[](size_t, std::__type_descriptor_t)`, which only Apple's libc++ exports;
the imported libc++ does not, so the program failed at load (`Symbol not found: __ZnamSt19__type_descriptor_t`). The flag is chosen
from the target variables and a probe of the compiler, so these checks run on every POSIX host with a fake compiler.
"""

import os
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
FLAG = "-fno-typed-cxx-new-delete"


@unittest.skipIf(os.name == "nt", "POSIX shell wrappers")
class TypedNewDeleteTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        self.root = root
        self.sysroot = root / "sysroot"
        (self.sysroot / "lib").mkdir(parents=True)
        (self.sysroot / "include").mkdir()
        # Prints its arguments; as a flag probe (-fsyntax-only) it rejects the flag when the test says the compiler is too old.
        self.fake = root / "fake-compiler"
        self.fake.write_text('#!/bin/sh\nif [ -n "$FAKE_REJECTS_TYPED_NEW_FLAG" ]; then\n'
                             '  for a in "$@"; do [ "$a" = "-fsyntax-only" ] && exit 1; done\nfi\nprintf \'%s\\n\' "$@"\n')
        self.fake.chmod(self.fake.stat().st_mode | stat.S_IXUSR)
        (root / "t.cc").write_text("int main() { return 0; }\n")
        (root / "t.c").write_text("int main(void) { return 0; }\n")
        (root / "t.S").write_text("")

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def arguments(self, wrapper, target_os, arch, source="t.cc", old_compiler=False):
        env = {"PATH": os.environ["PATH"], "CRT_SYSROOT": str(self.sysroot), "CRT_TARGET_OS": target_os,
               "CRT_TARGET_ARCH": arch, "CRT_HOST_CC": str(self.fake), "CRT_HOST_CXX": str(self.fake)}
        if old_compiler:
            env["FAKE_REJECTS_TYPED_NEW_FLAG"] = "1"
        done = subprocess.run([str(TOOLS / wrapper), "-c", str(self.root / source), "-o", str(self.root / "t.o")],
                              env=env, capture_output=True, text=True)
        self.assertEqual(0, done.returncode, done.stderr)
        return done.stdout.splitlines()

    def test_macos_cxx_compiles_disable_typed_allocation_once(self) -> None:
        self.assertEqual(1, self.arguments("crt-c++", "macos", "aarch64").count(FLAG))

    def test_a_compiler_that_does_not_know_the_flag_is_not_given_it(self) -> None:
        self.assertNotIn(FLAG, self.arguments("crt-c++", "macos", "aarch64", old_compiler=True))

    def test_assembly_and_other_targets_are_unchanged(self) -> None:
        self.assertNotIn(FLAG, self.arguments("crt-c++", "macos", "aarch64", source="t.S"))
        for target_os, arch in (("linux", "x86_64"), ("linux", "aarch64"), ("windows", "x86_64")):
            with self.subTest(target=f"{target_os}/{arch}"):
                self.assertNotIn(FLAG, self.arguments("crt-c++", target_os, arch))

    def test_the_c_wrapper_is_unchanged(self) -> None:
        self.assertNotIn(FLAG, self.arguments("crt-cc", "macos", "aarch64", source="t.c"))


if __name__ == "__main__":
    unittest.main()
