#!/usr/bin/env python3
"""Focused tests for the SDK scripts tools/create_dist.py writes."""

import shutil
import os
import platform
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import create_dist
from crt_macho import rpaths as macho_rpaths

REPO_ROOT = Path(__file__).resolve().parents[1]
TOOLS = {"cc": "clang", "cxx": "clang++", "ar": "llvm-ar", "ranlib": "llvm-ranlib"}


class ComponentInstall(unittest.TestCase):
    @mock.patch.dict(os.environ, {"KEEP_TOOLCHAIN": "yes", "ZERO_AR_DATE": "external"}, clear=True)
    @mock.patch.object(create_dist.subprocess, "run")
    def test_macos_install_uses_selected_deterministic_ranlib_without_mutating_parent(self, run):
        with tempfile.TemporaryDirectory() as directory:
            sdk = Path(directory); (sdk / "lib").mkdir(); (sdk / "lib/libc.a").touch()
            create_dist.install_component("cmake", Path("build"), sdk, "crt-c", "macos", "selected-ranlib")
            self.assertEqual(run.call_args.args[0], ["selected-ranlib", "-D", str(sdk / "lib/libc.a")])
            self.assertEqual(run.call_count, 2)
        self.assertEqual(os.environ["ZERO_AR_DATE"], "external")
        self.assertTrue(run.call_args.kwargs["check"])

    def test_ordinary_media_dist_builds_and_declares_real_relink_kit(self):
        dependencies = [{"name": "ffmpeg"}]
        with mock.patch.object(create_dist.crt_relink_kit, "find_output", return_value="bin/libcrtmedia.dll"), \
                mock.patch.object(create_dist.crt_relink_kit, "build_kit") as build, \
                mock.patch.object(create_dist.crt_relink_kit, "check_kit", return_value=[]):
            create_dist.install_media_relink_kit(Path("build"), Path("sdk"), dependencies, "windows", True)
            build.assert_called_once_with(Path("build"), "bin/libcrtmedia.dll", Path("sdk"), "libcrtmedia")
            self.assertEqual(dependencies[0]["relink_kit"], "share/crt/relink/libcrtmedia")
            create_dist.install_media_relink_kit(Path("build"), Path("sdk"), dependencies, "windows", False)
            self.assertEqual(build.call_count, 1)  # UI retains its predecessor's exact kit

    def test_missing_or_damaged_inherited_kit_is_not_upgraded_by_declaration(self):
        with self.assertRaisesRegex(SystemExit, "rebuild crt-gfx-media-dist"):
            create_dist.install_media_relink_kit(Path("build"), Path("sdk"), [{"name": "ffmpeg"}], "windows", False)
        with mock.patch.object(create_dist.crt_relink_kit, "check_kit", return_value=["damaged"]):
            with self.assertRaisesRegex(SystemExit, "damaged"):
                create_dist.install_media_relink_kit(Path("build"), Path("sdk"),
                    [{"name": "ffmpeg", "relink_kit": "share/crt/relink/libcrtmedia"}], "windows", False)

    @mock.patch.dict(os.environ, {"KEEP_TOOLCHAIN": "yes"}, clear=True)
    @mock.patch.object(create_dist.subprocess, "run")
    def test_other_hosts_keep_their_install_environment(self, run):
        for host in ("linux", "windows", ""):
            create_dist.install_component("cmake", Path("build"), Path("sdk"), "crt-c", host)
            self.assertEqual(run.call_args.args[0][:2], ["cmake", "--install"])
            self.assertNotIn("env", run.call_args.kwargs)

    @unittest.skipUnless(sys.platform == "darwin", "Darwin ranlib integration")
    def test_darwin_ranlib_repetition_preserves_full_archive_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "symbol.c").write_text("int archive_symbol(void) { return 42; }\n")
            subprocess.run(["/usr/bin/clang", "-c", str(root / "symbol.c"), "-o", str(root / "symbol.o")], check=True)
            subprocess.run(["/usr/bin/ar", "rcs", str(root / "test.a"), str(root / "symbol.o")], check=True)
            (root / "lib").mkdir(); shutil.copy2(root / "test.a", root / "lib/test.a")
            members = subprocess.check_output(["/usr/bin/ar", "-p", str(root / "lib/test.a")])
            create_dist.normalize_macos_archives(root, "/usr/bin/ranlib")
            before = (root / "lib/test.a").read_bytes()
            time.sleep(1.1)
            # A recopy/touch forces Apple's ranlib to rewrite its index.
            os.utime(root / "lib/test.a", None)
            create_dist.normalize_macos_archives(root, "/usr/bin/ranlib")
            self.assertEqual((root / "lib/test.a").read_bytes(), before)
            self.assertEqual(subprocess.check_output(["/usr/bin/ar", "-p", str(root / "lib/test.a")]), members)


def write_sdk(target_os: str, arch: str, triple: str, version="development") -> Path:
    dest = Path(tempfile.mkdtemp()) / "03-gfx-simple"
    dest.mkdir()
    create_dist.write_sdk_files(REPO_ROOT, dest, target_os, arch, triple,
                                "03-gfx-simple", TOOLS, [], version)
    return dest


@unittest.skipUnless(sys.platform == "darwin", "needs the Mach-O tools of a macOS host")
class MachoPortableRpath(unittest.TestCase):
    """The shared networking libraries copied into an SDK must not keep the port link's absolute rpath."""

    def build_dylib(self, directory: Path, rpaths: list) -> Path:
        source = directory / "x.c"
        source.write_text("int crt_probe(void) { return 7; }\n", encoding="utf-8")
        output = directory / "libprobe.dylib"
        command = ["clang", "-dynamiclib", str(source), "-o", str(output)]
        for entry in rpaths:
            command += [f"-Wl,-rpath,{entry}"]
        subprocess.run(command, check=True, capture_output=True)
        return output

    def test_absolute_rpath_is_removed_and_relative_ones_stay(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            library = self.build_dylib(Path(directory), ["/build/tree/dist/01-c/lib", "@loader_path"])
            self.assertEqual(["/build/tree/dist/01-c/lib", "@loader_path"], macho_rpaths(library))
            create_dist.remove_absolute_macho_rpaths(library)
            self.assertEqual(["@loader_path"], macho_rpaths(library))
            create_dist.remove_absolute_macho_rpaths(library)  # idempotent
            self.assertEqual(["@loader_path"], macho_rpaths(library))


    def test_shared_port_paths_helper_removes_only_absolute_build_rpaths(self) -> None:
        """portable_macho_dependency_paths is used by the media SDK and the Development SDK alike."""
        from build_stage_04_gfx_media import portable_macho_dependency_paths
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            library = self.build_dylib(root, ["/build/tree/dist/01-c/lib", "@loader_path", "/usr/lib/swift"])
            portable_macho_dependency_paths(root)
            self.assertEqual(["@loader_path", "/usr/lib/swift"], macho_rpaths(library),
                             "the build-tree rpath goes; relative and operating-system entries stay")


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

    def test_linux_cross_sdk_persists_target_for_unactivated_wrappers(self):
        sdk = self._sdk("linux", "aarch64", "aarch64-linux-gnu")
        self.assertEqual((sdk / "tools/crt-target.sh").read_text(),
                         "CRT_SDK_TARGET_OS=linux\nCRT_SDK_TARGET_ARCH=aarch64\n")
        toolchain = (sdk / "crt-toolchain.cmake").read_text()
        self.assertIn('set(CMAKE_SYSTEM_PROCESSOR "aarch64")', toolchain)

    @unittest.skipUnless(sys.platform == "linux", "Linux cross activation")
    def test_cross_activation_does_not_expose_target_shell_make(self):
        target = "x86_64" if platform.machine().lower() in ("aarch64", "arm64") else "aarch64"
        sdk = self._sdk("linux", target, target + "-linux-gnu")
        env = dict(os.environ, PATH="/usr/bin:/bin")
        result = subprocess.check_output(
            ["/bin/bash", "-c", '. "$1"; printf "%s" "$PATH"', "activate", str(sdk / "activate.sh")],
            env=env, text=True)
        self.assertEqual(result.split(os.pathsep)[0], str(sdk / "tools"))
        self.assertNotIn(str(sdk / "system/bin"), result.split(os.pathsep))

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
