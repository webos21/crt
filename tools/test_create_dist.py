#!/usr/bin/env python3
"""Focused tests for the SDK scripts tools/create_dist.py writes."""

import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import create_dist

REPO_ROOT = Path(__file__).resolve().parents[1]
TOOLS = {"cc": "clang", "cxx": "clang++", "ar": "llvm-ar", "ranlib": "llvm-ranlib"}


def write_sdk(target_os: str, arch: str, triple: str, version="development") -> Path:
    dest = Path(tempfile.mkdtemp()) / "03-gfx-simple"
    dest.mkdir()
    create_dist.write_sdk_files(REPO_ROOT, dest, target_os, arch, triple,
                                "03-gfx-simple", TOOLS, [], version)
    return dest


class ActivationScripts(unittest.TestCase):
    def _sdk(self, *args, **kwargs) -> Path:
        dest = write_sdk(*args, **kwargs)
        self.addCleanup(shutil.rmtree, dest.parent, ignore_errors=True)
        return dest

    def test_windows_activation_puts_the_runtime_dll_dir_on_path(self):
        # Windows has no RPATH: a program rebuilt with the SDK imports
        # libcrtgfx.dll/libc.dll from <sdk>\bin and exits with
        # STATUS_DLL_NOT_FOUND (0xC0000135) unless activation adds that
        # directory to PATH.
        text = (self._sdk("windows", "x86_64", "x86_64-w64-mingw32")
                / "activate.cmd").read_text(encoding="utf-8")
        path_lines = [l for l in text.splitlines() if l.startswith('set "PATH=')]
        self.assertEqual(len(path_lines), 1)
        entries = path_lines[0][len('set "PATH='):-1].split(";")
        self.assertEqual(entries, [
            "%CRT_SYSROOT%tools", "%CRT_SYSROOT%system\\bin",
            "%CRT_SYSROOT%bin", "%PATH%"])

    def test_windows_arm64_activation_also_adds_it(self):
        text = (self._sdk("windows", "aarch64", "aarch64-w64-mingw32")
                / "activate.cmd").read_text(encoding="utf-8")
        self.assertIn("%CRT_SYSROOT%bin;%PATH%", text)

    def test_posix_activation_needs_no_lib_dir_on_the_search_path(self):
        # Linux/macOS consumers find shared libraries through the RPATH that
        # crt-toolchain.cmake sets (ELF RUNPATH / Mach-O @rpath), not the
        # environment, so activate.sh deliberately stays as it was.
        sdk = self._sdk("linux", "x86_64", "x86_64-linux-gnu")
        sh = (sdk / "activate.sh").read_text(encoding="utf-8")
        self.assertIn('export PATH="$_crt_dist_root/tools:$_crt_dist_root/system/bin:$PATH"', sh)
        self.assertNotIn("LD_LIBRARY_PATH", sh)
        toolchain = (sdk / "crt-toolchain.cmake").read_text(encoding="utf-8")
        self.assertIn('set(CMAKE_BUILD_RPATH "${CRT_DISTRIBUTION_ROOT}/lib")', toolchain)
        self.assertIn('set(CMAKE_INSTALL_RPATH "${CRT_DISTRIBUTION_ROOT}/lib")', toolchain)

    def test_release_version_is_written_to_the_version_file(self):
        sdk = self._sdk("linux", "aarch64", "aarch64-linux-gnu", version="v0.4.0-preview.1")
        self.assertEqual((sdk / "VERSION").read_text(encoding="utf-8").strip(),
                         "v0.4.0-preview.1")
        self.assertEqual(
            (self._sdk("linux", "aarch64", "aarch64-linux-gnu") / "VERSION")
            .read_text(encoding="utf-8").strip(), "development")


if __name__ == "__main__":
    unittest.main()
