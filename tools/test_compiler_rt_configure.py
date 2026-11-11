#!/usr/bin/env python3
"""Configure-time behavior when compiler-rt builtins are missing on a native Linux/aarch64 host.

Linux/aarch64 long double is software binary128: printf and gdtoa need compiler-rt's __multf3/__netf2 family. A CI leg that
installed LLVM with --no-install-recommends had no compiler-rt, configure still succeeded and the failure appeared much later
as undefined references while linking shell/sh. CMakeLists.txt now fails the configure with the remedy. The check runs only
where the build can link, so this test runs only on a native Linux/aarch64 host.
"""
import os
from pathlib import Path
import platform
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
NATIVE_AARCH64 = platform.system() == "Linux" and platform.machine() in ("aarch64", "arm64")
CLANG = shutil.which("clang")
CMAKE = shutil.which("cmake")


@unittest.skipUnless(NATIVE_AARCH64 and CLANG and CMAKE, "needs a native Linux/aarch64 host with clang and cmake")
class CompilerRtConfigure(unittest.TestCase):
    def setUp(self):
        # The build directory must live under the checkout's out/ (documented CRT_BUILD_PRESET_NAME limitation).
        base = ROOT / "out"
        base.mkdir(exist_ok=True)
        self.work = Path(tempfile.mkdtemp(prefix="rt-configure-", dir=base))
        self.addCleanup(shutil.rmtree, self.work, True)
        # A compiler that behaves as the real one except that it finds no compiler-rt archive.
        self.fake = self.work / "clang-without-compiler-rt"
        self.fake.write_text(
            "#!/bin/sh\n"
            'for a in "$@"; do\n'
            '  if [ "$a" = "--print-libgcc-file-name" ]; then echo /nonexistent/libclang_rt.builtins-aarch64.a; exit 0; fi\n'
            "done\n"
            f'exec "{CLANG}" "$@"\n')
        self.fake.chmod(self.fake.stat().st_mode | stat.S_IXUSR)

    def configure(self, *extra):
        build = self.work / "build"
        shutil.rmtree(build, True)
        command = [CMAKE, "--fresh", "--preset", "linux-host-ninja-debug", "-B", str(build), f"-DCMAKE_C_COMPILER={self.fake}", *extra]
        return subprocess.run(command, capture_output=True, text=True, timeout=900, cwd=ROOT)

    def test_missing_builtins_fail_the_configure_with_the_remedy(self):
        result = self.configure()
        output = result.stdout + result.stderr
        self.assertNotEqual(result.returncode, 0, output[-800:])
        self.assertIn("compiler-rt builtins archive", output)
        self.assertIn("libclang-rt-<N>-dev", output)
        self.assertIn("CRT_COMPILER_RT_BUILTINS_OVERRIDE", output)

    def test_an_explicit_archive_still_configures(self):
        real = subprocess.run([CLANG, "--rtlib=compiler-rt", "--print-libgcc-file-name"], capture_output=True, text=True).stdout.strip()
        if not real or not Path(real).is_file():
            self.skipTest("this host's clang has no compiler-rt archive to name")
        result = self.configure(f"-DCRT_COMPILER_RT_BUILTINS_OVERRIDE={real}")
        self.assertEqual(result.returncode, 0, (result.stdout + result.stderr)[-1200:])


if __name__ == "__main__":
    unittest.main()
