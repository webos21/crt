#!/usr/bin/env python3
"""Tests for tools/crt_relink_kit.py (the relink kit of the shared libcrtmedia, which statically folds in FFmpeg)."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))

import crt_relink_kit as kit
import verify_dist

POSIX_LINK = (": && /sdk/tools/crt-cc --sysroot=/sdk -shared -o lib/libcrtmedia.so -Wl,-soname,libcrtmedia.so "
              "libcrtmedia/CMakeFiles/crtmedia_shared.dir/src/a.c.o libcrtmedia/CMakeFiles/crtmedia_shared.dir/src/b.c.o "
              "libcrtmedia/exports.map /sdk/lib/libavcodec.a /sdk/lib/libavutil.a /usr/lib/libm.so.6 && :")
WINDOWS_LINK = ('C:\\WINDOWS\\system32\\cmd.exe /C "cd . && C:\\sdk\\tools\\crt-cc.cmd --sysroot=C:/sdk -shared '
                '-o bin\\libcrtmedia.dll -Wl,--out-implib,lib\\libcrtmedia_dll.dll.a '
                'libcrtmedia/CMakeFiles/crtmedia_shared.dir/C_/src/a.c.obj C:/sdk/lib/libavcodec.a '
                '"C:/Program Files (x86)/Windows Kits/10/Lib/x64/Mf.lib" && cd ."')


class Runner(unittest.TestCase):
    def test_relative_paths_are_made_absolute_before_the_link_changes_directory(self) -> None:
        import json, subprocess, tempfile
        from pathlib import Path
        with tempfile.TemporaryDirectory() as raw:
            work = Path(raw)
            kit_dir = work / "sdk" / "share" / "crt" / "relink" / "libcrtmedia"
            kit_dir.mkdir(parents=True)
            (kit_dir / "relink.json").write_text(json.dumps(
                {"compiler": "cc", "args": ["-o", "@OUT@/libcrtmedia.so", "@SDK@/lib/libavutil.a"]}), encoding="utf-8")
            (kit_dir / "relink.py").write_text(kit.RUNNER.format(name="libcrtmedia", ffmpeg=["libavutil.a"]), encoding="utf-8")
            (work / "libs").mkdir()
            (work / "libs" / "libavutil.a").write_bytes(b"")
            result = subprocess.run([sys.executable, str(kit_dir / "relink.py"), "--print", "--out", "rel/out",
                                     "--ffmpeg-libdir", "libs"], cwd=work, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            printed = result.stdout.split()
            self.assertEqual(printed[printed.index("-o") + 1], str((work / "rel" / "out").resolve() / "libcrtmedia.so"))
            self.assertEqual(printed[-1], str((work / "libs" / "libavutil.a").resolve()))


class Parse(unittest.TestCase):
    def test_posix_link_command_is_found_among_the_ninja_wrapper(self) -> None:
        tokens = kit.link_tokens(POSIX_LINK, "lib/libcrtmedia.so", windows=False)
        self.assertEqual("/sdk/tools/crt-cc", tokens[0])
        self.assertNotIn("&&", tokens)
        self.assertIn("libcrtmedia/exports.map", tokens)

    def test_windows_link_command_keeps_quoted_paths_with_spaces(self) -> None:
        tokens = kit.link_tokens(WINDOWS_LINK, "bin/libcrtmedia.dll", windows=True)
        self.assertEqual("C:\\sdk\\tools\\crt-cc.cmd", tokens[0])
        self.assertIn("C:/Program Files (x86)/Windows Kits/10/Lib/x64/Mf.lib", tokens)

    def test_no_link_command_is_an_error(self) -> None:
        with self.assertRaises(SystemExit):
            kit.link_tokens(": && clang -c x.c && :", "lib/libcrtmedia.so", windows=False)


class Rewrite(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.build = Path(self.temp.name).resolve() / "build"
        directory = self.build / "libcrtmedia" / "CMakeFiles" / "crtmedia_shared.dir" / "src"
        directory.mkdir(parents=True)
        for name in ("a.c.o", "b.c.o"):
            (directory / name).write_bytes(name.encode())
        (self.build / "libcrtmedia" / "exports.map").write_text("{ global: *; };\n")

    def test_objects_support_files_and_sdk_paths_become_placeholders(self) -> None:
        tokens = kit.link_tokens(POSIX_LINK, "lib/libcrtmedia.so", windows=False)
        objects, support = {}, {}
        result = kit.rewrite(tokens, self.build, ["/sdk"], objects, support)
        self.assertEqual(["a.c.o", "b.c.o"], sorted(objects))
        self.assertEqual(["exports.map"], list(support))
        self.assertEqual("@SDK@/tools/crt-cc", result[0])
        self.assertIn("--sysroot=@SDK@", result)
        self.assertIn("@OUT@/libcrtmedia.so", result)
        self.assertIn("@SDK@/lib/libavcodec.a", result)
        self.assertIn("@OBJ@/a.c.o", result)
        self.assertIn("@SUPPORT@/exports.map", result)
        self.assertIn("/usr/lib/libm.so.6", result)  # a host library stays as recorded
        self.assertFalse([item for item in result if str(self.build) in item])

    def test_a_build_tree_library_outside_the_sdk_fails_the_kit(self) -> None:
        with self.assertRaises(SystemExit):
            kit.rewrite(["/sdk/tools/crt-cc", "-shared", "port-tests/install/lib/libx.a"], self.build, ["/sdk"], {}, {})

    def test_colliding_object_names_stay_distinct(self) -> None:
        other = self.build / "libcrtmedia" / "CMakeFiles" / "other.dir"
        other.mkdir(parents=True)
        (other / "a.c.o").write_bytes(b"other")
        tokens = ["cc", "libcrtmedia/CMakeFiles/crtmedia_shared.dir/src/a.c.o", "libcrtmedia/CMakeFiles/other.dir/a.c.o"]
        objects = {}
        kit.rewrite(tokens, self.build, ["/sdk"], objects, {})
        self.assertEqual(2, len(objects))


class Installed(unittest.TestCase):
    def make_sdk(self, root: Path) -> tuple:
        build = root / "build"
        directory = build / "libcrtmedia" / "CMakeFiles" / "crtmedia_shared.dir" / "src"
        directory.mkdir(parents=True)
        (directory / "a.c.o").write_bytes(b"object a")
        sdk = root / "sdk"
        (sdk / "lib").mkdir(parents=True)
        (sdk / "tools").mkdir()
        (sdk / "tools" / "crt-cc").write_text("compiler driver\n")
        for name in kit.FFMPEG_ARCHIVES:
            (sdk / "lib" / name).write_bytes(b"archive")
        line = (f": && {sdk.as_posix()}/tools/crt-cc --sysroot={sdk.as_posix()} -shared -o lib/libcrtmedia.so "
                f"libcrtmedia/CMakeFiles/crtmedia_shared.dir/src/a.c.o {sdk.as_posix()}/lib/libavcodec.a && :")
        return build, sdk, subprocess.CompletedProcess([], 0, stdout=line + "\n")

    def write_kit(self, root: Path) -> tuple:
        build, sdk, result = self.make_sdk(root)
        with mock.patch.object(kit.subprocess, "run", return_value=result):
            return sdk, kit.build_kit(build, "lib/libcrtmedia.so", sdk, "libcrtmedia")

    def test_kit_is_written_checked_and_detects_damage(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            sdk, folder = self.write_kit(Path(temp).resolve())
            self.assertEqual([], kit.check_kit(sdk, "libcrtmedia"))
            recipe = json.loads((folder / "relink.json").read_text())
            self.assertEqual("@SDK@/tools/crt-cc", recipe["compiler"])
            self.assertEqual(b"object a", (folder / "objects" / "a.c.o").read_bytes())
            (folder / "objects" / "a.c.o").write_bytes(b"tampered")
            self.assertTrue(any("differs" in problem for problem in kit.check_kit(sdk, "libcrtmedia")))
            (folder / "objects" / "a.c.o").write_bytes(b"object a")
            (sdk / "lib" / "libavutil.a").unlink()
            self.assertTrue(any("libavutil.a" in problem for problem in kit.check_kit(sdk, "libcrtmedia")))
            (folder / "relink.py").unlink()
            self.assertTrue(any("relink.py" in problem for problem in kit.check_kit(sdk, "libcrtmedia")))

    def test_verify_dist_requires_the_kit_for_isolated_ffmpeg_stages_only(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            sdk, folder = self.write_kit(Path(temp).resolve())
            declared = {"ffmpeg": {"relink_kit": "share/crt/relink/libcrtmedia"}}
            verify_dist.validate_relink_kit(sdk, {"built_from": {}}, declared)
            verify_dist.validate_relink_kit(sdk, {"built_from": {}}, {"zlib": {}})  # no FFmpeg, nothing to relink
            verify_dist.validate_relink_kit(sdk, {}, {"ffmpeg": {}})  # in-tree developer SDK
            with self.assertRaises(SystemExit):
                verify_dist.validate_relink_kit(sdk, {"built_from": {}}, {"ffmpeg": {}})  # kit not declared
            (folder / "relink.json").unlink()
            with self.assertRaises(SystemExit):
                verify_dist.validate_relink_kit(sdk, {"built_from": {}}, declared)

    def test_runner_prints_the_command_and_swaps_the_ffmpeg_archives(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp).resolve()
            sdk, folder = self.write_kit(root)
            own = root / "mine"
            own.mkdir()
            (own / "libavcodec.a").write_bytes(b"modified")
            environment = {**os.environ, "CRT_SYSROOT": str(sdk)}
            shown = subprocess.run([sys.executable, str(folder / "relink.py"), "--print", "--sdk", str(sdk),
                                    "--ffmpeg-libdir", str(own), "--out", str(root / "out")],
                                   capture_output=True, text=True, env=environment)
            self.assertEqual(0, shown.returncode, shown.stderr)
            self.assertIn(str(own / "libavcodec.a"), shown.stdout)
            self.assertNotIn("@SDK@", shown.stdout)
            bare = {key: value for key, value in environment.items() if key != "CRT_SYSROOT"}
            missing = subprocess.run([sys.executable, str(folder / "relink.py"), "--sdk", str(sdk)],
                                     capture_output=True, text=True, env=bare)
            self.assertNotEqual(0, missing.returncode)
            self.assertIn("activate the SDK", missing.stderr)


if __name__ == "__main__":
    unittest.main()
