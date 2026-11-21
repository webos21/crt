"""Linux C-wrapper argv regressions; fake compiler, no native ARM execution."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

WRAPPER = Path(__file__).resolve().with_name("crt-cc")


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux linker policy")
class CLinkage(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="crt C linkage 한글 ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.sdk = self.root / "SDK"
        (self.sdk / "lib").mkdir(parents=True)
        for name in ("crt1.o", "crt1_init_array.o", "libclang_rt.builtins.a",
                     "libc.a", "libm.a", "libdl.a", "libc.so", "libm.so", "libdl.so"):
            (self.sdk / "lib" / name).touch()
        self.capture = self.root / "argv.json"
        self.compiler = self.root / "recorder"
        self.compiler.write_text("#!/usr/bin/python3\nimport json,os,sys\n"
            "if sys.argv[1:]==['-print-resource-dir']: print('/resource')\n"
            "else: json.dump(sys.argv[1:],open(os.environ['CAPTURE'],'w'))\n")
        self.compiler.chmod(0o700)

    def invoke(self, arch, hints=None, extra=()):
        env = {k: v for k, v in os.environ.items() if not k.startswith("CRT_")}
        env.update(CRT_SYSROOT=str(self.sdk), CRT_TARGET_OS="linux", CRT_TARGET_ARCH=arch,
                   CRT_HOST_CC=str(self.compiler), CAPTURE=str(self.capture))
        env.update(hints or {})
        result = subprocess.run([str(WRAPPER), "probe.c", *extra, "-o", str(self.root / "program")],
                                env=env, capture_output=True, text=True)
        argv = json.loads(self.capture.read_text()) if self.capture.exists() else []
        return result, argv

    def test_static_default_and_startup(self):
        for arch in ("x86_64", "aarch64"):
            with self.subTest(arch=arch):
                result, argv = self.invoke(arch)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn(str(self.sdk / "lib/libc.a"), argv)
                self.assertIn(str(self.sdk / "lib/crt1.o"), argv)
                self.assertIn(str(self.sdk / "lib/crt1_init_array.o"), argv)
                self.assertNotIn("-Wl,-rpath," + str(self.sdk / "lib"), argv)

    def test_shared_runtime_and_explicit_port_search_precedes_sdk(self):
        for arch in ("x86_64", "aarch64"):
            with self.subTest(arch=arch):
                port = "-Wl,-rpath,/explicit port/lib"
                result, argv = self.invoke(arch, {"CRT_RUNTIME_LINKAGE": "shared"}, [port])
                self.assertEqual(result.returncode, 0, result.stderr)
                for lib in ("libc.so", "libm.so", "libdl.so"):
                    self.assertIn(str(self.sdk / "lib" / lib), argv)
                self.assertNotIn(str(self.sdk / "lib/libc.a"), argv)
                self.assertIn(str(self.sdk / "lib/crt1.o"), argv)
                self.assertGreater(argv.index("-Wl,-rpath," + str(self.sdk / "lib")), argv.index(port))

    def test_legacy_cxx_hint_and_explicit_neutral_override(self):
        result, argv = self.invoke("x86_64", {"CRT_CXX_RUNTIME_LINKAGE": "shared"})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(str(self.sdk / "lib/libc.so"), argv)
        result, argv = self.invoke("x86_64", {"CRT_CXX_RUNTIME_LINKAGE": "shared", "CRT_RUNTIME_LINKAGE": "static"})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(str(self.sdk / "lib/libc.a"), argv)

    def test_shared_library_has_no_executable_startup(self):
        result, argv = self.invoke("x86_64", {"CRT_RUNTIME_LINKAGE": "static"}, ["-shared"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(str(self.sdk / "lib/libc.so"), argv)
        self.assertNotIn(str(self.sdk / "lib/crt1.o"), argv)

    def test_invalid_linkage_fails_before_link_and_output(self):
        result, argv = self.invoke("x86_64", {"CRT_RUNTIME_LINKAGE": "unknown"})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unsupported Linux runtime linkage", result.stderr)
        self.assertEqual(argv, [])
        self.assertFalse((self.root / "program").exists())


if __name__ == "__main__":
    unittest.main()
