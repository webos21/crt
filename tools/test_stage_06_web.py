#!/usr/bin/env python3
"""Fast guards for the Web SDK boundary, host generators and advisory parser."""
import copy
from datetime import datetime, timezone, timedelta
import json
import io
import os
import platform
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import tarfile
import unittest

import build_stage_06_web as stage
import check_webkit_security as security
from crt_linux_runtime import normalize_cxx_aliases
from crt_dist_prerequisites import external_prerequisites_for
from crt_stage_recipe import STAGE_SUCCESSORS
from crt_pe import exported_symbols
from test_stage_06_browser import observer_font_config, allowed_cache_mapping, anonymous_mapping
from verify_dist import validate_web_stage

ROOT = Path(__file__).resolve().parents[1]


class SecurityTests(unittest.TestCase):
    def page(self, declaration):
        return '<p>CVE-2026-1234</p><li>Versions affected: ' + declaration + '.</li><p>Memory corruption.</p>'

    def test_version_ranges(self):
        for declaration in ("WebKitGTK and WPE WebKit before 2.54.0 or earlier",
                            "WebKitGTK before 2.54.0 and WPE WebKit before 2.54.0",
                            "WebKitGTK and WPE WebKit before 2.38.6 and 2.40 branch before 2.40.1",
                            "WebKitGTK and WPE WebKit before 2.36.5 if USE_LIBWEBRTC is enabled"):
            with self.subTest(declaration=declaration):
                self.assertFalse(security.evaluate(self.page(declaration), "2.54.0")["affected"])
                self.assertTrue(security.evaluate(self.page(declaration), "2.30.0")["affected"])
        self.assertFalse(security.evaluate(self.page("WebKitGTK before 2.54.1"), "2.54.0")["affected"])

    def test_unknown_or_newer_bounds_fail_closed(self):
        self.assertTrue(security.evaluate(self.page("WebKitGTK and WPE WebKit before 2.54.1"), "2.54.0")["affected"])
        for value in ("everything through 2.54.0", "WebKitGTK and WPE WebKit through 2.54.0", "unavailable"):
            with self.assertRaises(ValueError):
                security.evaluate(self.page(value), "2.54.0")
        with self.assertRaises(ValueError):
            security.evaluate("<p>server error</p>", "2.54.0")
        with self.assertRaises(ValueError):
            security.advisory_ids("<p>server error</p>")

    def test_index_order_and_duplicates(self):
        self.assertEqual(security.advisory_ids('<a href="WSA-2026-0005.html">x</a>'
                            '<a href="WSA-2026-0006.html">x</a><a href="WSA-2026-0006.html">x</a>'),
                         ["WSA-2026-0006", "WSA-2026-0005"])


class PackagingTests(unittest.TestCase):
    def test_notices_ship_exact_pinned_bytes_and_reject_content_changes(self):
        import hashlib
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "LICENSE"
            canonical = b"Copyright holder\nPermission text.\n"
            expected = hashlib.sha256(canonical).hexdigest()
            for data in (canonical, canonical.replace(b"\n", b"\r\n")):
                source.write_bytes(data)
                self.assertEqual(stage.verified_notice_bytes(source, expected), canonical)
                self.assertEqual(source.read_bytes(), data)  # checkout remains untouched
            raw = canonical.replace(b"\n", b"\r\n")
            source.write_bytes(raw)
            self.assertEqual(stage.verified_notice_bytes(source, hashlib.sha256(raw).hexdigest()), raw)
            for data in (canonical + b" ", canonical.replace(b"\n", b"\r"), b"modified notice\n"):
                source.write_bytes(data)
                with self.assertRaisesRegex(ValueError, "differs"):
                    stage.verified_notice_bytes(source, expected)

    def test_owned_icu_text_bytes_match_evidence_on_windows(self):
        from crt_icu_host_pkgdata import write_owned_text
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "options.inc"
            write_owned_text(path, "a=1\nb=2\n")
            self.assertEqual(path.read_bytes(), b"a=1\nb=2\n")

    def test_windows_fontconfig_binds_both_probes_and_make_to_host_gperf(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            recipe = {"dependencies":["make","gperf","freetype"],
                      "build":{"env":{"GPERF":"target/gperf"},"path_prepend":["target/bin"]}}
            result = stage.fontconfig_host_generator_recipe(
                recipe, root/"sdk", root/"work", root/"native/gperf.exe", "windows")
            self.assertEqual(result["dependencies"],["make","freetype"])
            self.assertEqual(recipe["dependencies"],["make","gperf","freetype"])
            self.assertEqual(result["build"]["path_prepend"],[stage.web.crt_path(root/"work/host/fontconfig-tools")])
            self.assertIn("fontconfig-tools/gperf",result["build"]["env"]["GPERF"])
            relay=(root/"work/host/fontconfig-tools/gperf").read_bytes()
            self.assertNotIn(b"\r",relay)
            self.assertIn(b"crt-native-tool",relay)
            self.assertIn(b"native/gperf.exe",relay)

    def test_linux_fontconfig_never_builds_or_runs_target_gperf(self):
        recipe = {"dependencies": ["make", "gperf", "freetype"],
                  "build": {"env": {"GPERF": "target/gperf"}, "path_prepend": ["target/bin"]}}
        result = stage.fontconfig_host_generator_recipe(
            recipe, Path("/sdk"), Path("/work"), Path("/host/bin/gperf"), "linux")
        self.assertEqual(result["dependencies"], ["make", "freetype"])
        self.assertEqual(result["build"]["env"]["GPERF"], "/host/bin/gperf")
        self.assertEqual(result["build"]["path_prepend"], ["/host/bin"])

    def test_linux_cross_recipes_name_build_and_target(self):
        autoconf = {"name": "icu", "build": {"system": "configure", "configure_args": []}}
        result = stage.linux_cross_recipe(autoconf)
        self.assertIn("--build=x86_64-pc-linux-gnu", result["build"]["configure_args"])
        self.assertIn("--host=aarch64-linux-gnu", result["build"]["configure_args"])
        zlib = stage.linux_cross_recipe(
            {"name": "zlib", "build": {"system": "configure", "configure_args": [], "env": {}}})
        self.assertEqual(zlib["build"]["env"]["CHOST"], "aarch64-linux-gnu")

    def test_native_configure_interpreter_reads_host_not_crt_root_paths(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("crt_port_build_test", ROOT / "tools/crt-port-build.py")
        ports = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(ports)
        with tempfile.TemporaryDirectory() as directory:
            sdk = Path(directory) / "SDK with spaces"
            sdk.mkdir()
            (sdk / "manifest.json").write_text("{}")
            self.assertEqual(ports.root_for_configure(sdk,"windows",True), "")
            self.assertEqual(ports.root_for_configure(sdk,"windows",True,"perl"), sdk.as_posix())
            self.assertEqual(ports.root_for_configure(sdk,"windows",False,"perl"), sdk.as_posix())
            self.assertEqual(ports.root_for_configure(sdk,"linux",True,"perl"), str(sdk))

    def test_recipe_prepend_retains_crt_applets_and_drive_namespace(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("crt_port_build_path_test", ROOT / "tools/crt-port-build.py")
        ports = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(ports)
        env = {"PATH": "/system/bin:/bin:/usr/bin"}
        ports.prepend_recipe_path(env, ["C:/SDK with spaces/bin"], "windows", True)
        self.assertEqual(env["PATH"], "/c/SDK with spaces/bin:/system/bin:/bin:/usr/bin")
        env = {"PATH": "native"}
        ports.prepend_recipe_path(env, ["C:/SDK with spaces/bin"], "windows", False)
        self.assertEqual(env["PATH"], "C:/SDK with spaces/bin" + ports.os.pathsep + "native")

    def test_windows_sdk_cmake_uses_external_archiver_not_missing_repo_wrapper(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("crt_port_build_ar_test", ROOT / "tools/crt-port-build.py")
        ports = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(ports)
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            self.assertEqual(ports.windows_cmake_archiver(root,{}),(root/"tools/crt-ar.cmd").as_posix())
            (root/"manifest.json").write_text("{}")
            archiver=root/"host tool/llvm-ar.exe"
            archiver.parent.mkdir()
            archiver.touch()
            self.assertEqual(ports.windows_cmake_archiver(root,{"CRT_AR":str(archiver)}),archiver.as_posix())
            with self.assertRaises(ValueError):
                ports.windows_cmake_archiver(root,{"CRT_AR":str(root/"missing.exe")})

    def test_target_data_paths_use_crt_namespace_not_native_host_paths(self):
        from pathlib import PureWindowsPath, PurePosixPath
        native = PureWindowsPath("C:/SDK with spaces/ca.pem")
        self.assertEqual(stage.target_path(native, "windows"), "/c/SDK with spaces/ca.pem")
        self.assertEqual(stage.target_path(PurePosixPath("/sdk/ca.pem"), "linux"), "/sdk/ca.pem")

    def test_native_icu_shared_install_replaces_stub_with_real_named_data(self):
        from crt_icu_host_pkgdata import install_shared_data
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, target = root / "generated", root / "prefix/bin"
            source.mkdir()
            target.mkdir(parents=True)
            (source / "icudt78.dll").write_bytes(b"MZ real generated data")
            (source / "libicudt.dll.a").write_bytes(b"!<arch>\n generated import archive")
            (target / "icudt78.dll").write_bytes(b"MZ stub")
            args = ["-m","dll","-L","icudt","-r","78","-d",str(source),"-I",str(target)]
            result = install_shared_data(args,"SO=dll\nLIBPREFIX=\n")
            self.assertEqual(len(result),2)
            self.assertEqual((target/"icudt78.dll").read_bytes(),(source/"icudt78.dll").read_bytes())
            self.assertEqual((target.parent/"lib/libicudt.dll.a").read_bytes(),(source/"libicudt.dll.a").read_bytes())
            self.assertEqual(install_shared_data(["-m","static","-I",str(target)],""),{})
            with self.assertRaises(ValueError):
                install_shared_data(args,"SO=so\nLIBPREFIX=lib\n")

    def test_native_icu_options_translate_only_crt_commands_and_fail_closed(self):
        from crt_icu_host_pkgdata import native_config, native_arguments
        self.assertEqual(native_arguments([r"C:\source\data", "-p", r"literal\value"]),
                         ["C:/source/data", "-p", r"literal\value"])
        sdk = Path("SDK with spaces")
        original = ("SO=dll\nCOMPILE=/system/bin/mksh /tools/crt-cc -c -DU_BIONIC=1\n"
                    "GENLIB=/system/bin/mksh /tools/crt-cc -shared\n"
                    "AR=/system/bin/mksh /tools/crt-native-tool C:/LLVM/llvm-ar.exe\n"
                    "RANLIB=/system/bin/mksh /tools/crt-native-tool C:/LLVM/llvm-ranlib.exe\n")
        actual = native_config(original, sdk, Path("owned build"))
        self.assertIn("SO=dll\n", actual)
        self.assertIn("crt-cc.sh' -c -DU_BIONIC=1", actual)
        self.assertIn("crt-cc.sh' -shared", actual)
        self.assertNotIn("/system/bin/mksh /tools/", actual)
        self.assertIn("SDK with spaces/system/bin/mksh.exe", actual)
        for bad in (original.replace("COMPILE=/system/bin/mksh /tools/crt-cc", "COMPILE=gcc"),
                    original.replace("AR=/system/bin/mksh /tools/crt-native-tool C:/LLVM/llvm-ar.exe\n", ""),
                    original + "COMPILE=/system/bin/mksh /tools/crt-cc -c\n"):
            with self.assertRaises(ValueError):
                native_config(bad, sdk, Path("owned build"))

    def test_packaging_retry_only_reuses_heavy_inputs_not_sample_objects(self):
        self.assertFalse(stage.compile_input("examples/web-browser/main.cc"))
        self.assertFalse(stage.compile_input("examples/web-browser/CMakeLists.txt"))
        self.assertTrue(stage.compile_input("libcrtweb/src/crtweb.cpp"))
        self.assertTrue(stage.compile_input("libcrtweb/include/crtweb/webview.h"))
        script = (ROOT / "tools/build_stage_06_web.py").read_text()
        self.assertIn('"--build", native, "--clean-first"', script)

    def test_windows_predecessor_uses_pe_layout_and_complete_provenance(self):
        with tempfile.TemporaryDirectory() as directory:
            sdk = Path(directory)
            manifest = {"stage": "05-ui", "target": {"os": "windows"},
                        "redistributed_dependencies": [{"name": name} for name in
                            ("freetype", "ffmpeg", "skia", "curl", "mbedtls", "zlib", "lvgl")]}
            files = ("bin/libc.dll", "bin/libc++.dll", "bin/libcrtmedia.dll", "lib/libskia.a",
                     "lib/libcrtui_skia.a", "include/include/core/SkSurface.h",
                     "include/crtgfx/shared_image.h", "include/crtgfx/skia_shared.h")
            for name in files:
                path = sdk / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
            stage.require_predecessor(sdk, manifest)
            (sdk / "bin/libcrtmedia.dll").unlink()
            with self.assertRaisesRegex(ValueError, "bin/libcrtmedia.dll"):
                stage.require_predecessor(sdk, manifest)

    def test_windows_private_dll_copy_excludes_colliding_curl_and_public_wrapper(self):
        with tempfile.TemporaryDirectory() as directory:
            source, target = Path(directory) / "source", Path(directory) / "target"
            source.mkdir()
            target.mkdir()
            for name in ("libxml2-16.dll", "libcurl.dll", "libcrtweb.dll", "libc++.dll", "gperf.exe"):
                (source / name).write_bytes(name.encode())
            stage.copy_private_libraries(source, target, "windows")
            self.assertEqual([path.name for path in target.iterdir()], ["libxml2-16.dll"])

    @unittest.skipUnless(sys.platform == "win32", "native Windows observer")
    def test_native_windows_observer_snapshots_real_process_identity_and_modules(self):
        import os
        from test_stage_06_browser import windows_native
        snapshot, identity, _ = windows_native()
        self.assertIn(os.getpid(), snapshot())
        self.assertGreater(identity(os.getpid()), 0)
        modules = snapshot(os.getpid())
        self.assertTrue(any(Path(path).name.lower() == "kernel32.dll" for path in modules))

    def test_macos_ipc_exception_is_unlinked_nonexecutable_and_pid_scoped(self):
        from unittest.mock import patch
        # A Darwin-shaped /path resolves on the current Windows drive. Keep
        # the fixture there too (CI can use D: for cwd and C: for TEMP).
        with tempfile.TemporaryDirectory(dir=Path.cwd()) as directory:
            temporary = Path(directory).resolve()
            for name, permissions, present, allowed in (
                ("crt_memfd_77_000001.tmp", "rw-", False, True),
                ("crt_memfd_77_000001.tmp", "r-x", False, False),
                ("crt_memfd_78_000001.tmp", "rw-", False, False),
                ("other.tmp", "rw-", False, False),
                ("crt_memfd_77_000001.tmp", "rw-", True, False),
            ):
                file = temporary / name
                if present:
                    file.touch()
                posix_file = file.as_posix()
                if sys.platform == "win32":
                    posix_file = posix_file[2:]  # native fixture, Darwin-shaped row
                row = f"mapped file 1000-2000 [ 4K] {permissions}/rwx SM=SHM {posix_file}\n"
                result = subprocess.CompletedProcess([], 0, row, "")
                anonymous = []
                with patch.object(stage.web.subprocess, "run", return_value=result), \
                     patch.object(stage.web.tempfile, "gettempdir", return_value=str(temporary)):
                    paths = stage.web.macos_mapped_files(77, anonymous=anonymous)
                self.assertEqual(bool(anonymous), allowed)
                self.assertEqual(paths, [] if allowed else [posix_file])

    def test_macos_versioned_image_and_alias_are_packaged_together(self):
        with tempfile.TemporaryDirectory() as directory:
            source, target = Path(directory) / "source", Path(directory) / "target"
            source.mkdir()
            target.mkdir()
            real = source / "libWebKit.dylib.SOVERSION"
            real.write_bytes(b"versioned Mach-O fixture")
            (source / "libWebKit.dylib").symlink_to(real.name)
            (source / "libcurl.dylib").write_bytes(b"must stay private/static")
            (source / "unrelated.dylib.dSYM").mkdir()
            stage.copy_private_libraries(source, target, "macos")
            self.assertEqual((target / real.name).read_bytes(), real.read_bytes())
            self.assertTrue((target / "libWebKit.dylib").is_symlink())
            self.assertTrue((target / real.name).samefile(target / "libWebKit.dylib"))
            self.assertFalse((target / "libcurl.dylib").exists())
            self.assertFalse((target / "unrelated.dylib.dSYM").exists())

    def test_incomplete_source_cache_fails_before_expensive_build(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            (cache / "checkout/.git").mkdir(parents=True)
            with self.assertRaisesRegex(ValueError, "self-contained"):
                stage.validate_source_cache(cache, ROOT)

    def test_macos_web_paths_are_private_and_do_not_mutate_predecessor(self):
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as directory:
            sdk = Path(directory)
            private = sdk / "libexec/crtweb"
            (private / "lib").mkdir(parents=True)
            (sdk / "lib").mkdir()
            webkit = private / "lib/libWebKit.dylib"
            webkit.touch()
            (sdk / "lib/libc.dylib").touch()
            with patch.object(stage.crt_macho, "dylib_id", return_value=None), \
                 patch.object(stage.crt_macho, "dependencies", return_value=[str(sdk / "lib/libc.dylib")]), \
                 patch.object(stage.crt_macho, "rpaths", return_value=["/old/build/lib"]), \
                 patch.object(stage, "run") as run:
                stage.portable_web_macho(sdk)
            edits = [call.args[0] for call in run.call_args_list if call.args[0][0] == "install_name_tool"]
            self.assertEqual(len(edits), 14)
            self.assertTrue(all(command[-1] != sdk / "lib/libc.dylib" for command in edits))
            commands = [command for command in edits if command[-1] == webkit]
            self.assertIn("@loader_path/../../../lib", commands[1])
            self.assertIn("@rpath/libc.dylib", commands[0])
            self.assertIn("-delete_rpath", commands[0])
            self.assertTrue(all(not ("-delete_rpath" in command and "-add_rpath" in command) for command in edits))
            with patch.object(stage.crt_macho, "dylib_id", return_value=None), \
                 patch.object(stage.crt_macho, "dependencies", return_value=["/opt/host/libcurl.dylib"]):
                with self.assertRaisesRegex(ValueError, "unpackaged"):
                    stage.portable_web_macho(sdk)

    def test_stage_metadata_tools_are_updated_without_replacing_compilers(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            asset, sdk = root / "asset", root / "sdk"
            (asset / "tools").mkdir(parents=True)
            (sdk / "tools").mkdir(parents=True)
            for name in stage.DIST_PORTING_TOOLS:
                (asset / "tools" / name).write_text("current " + name)
                (sdk / "tools" / name).write_text("old")
            (sdk / "tools/crt-c++").write_text("predecessor compiler")
            stage.install_stage_tools(asset, sdk)
            for name in stage.DIST_PORTING_TOOLS:
                self.assertEqual((sdk / "tools" / name).read_text(), "current " + name)
            self.assertEqual((sdk / "tools/crt-c++").read_text(), "predecessor compiler")
    def test_anonymous_data_is_not_a_host_library_exception(self):
        scratch = Path("/tmp").resolve()
        for path in ("/tmp/crt_shm_WK2SharedMemory.123 (deleted)", "/tmp/crt_memfd_12_000001.tmp (deleted)",
                     "/memfd:WebKit (deleted)"):
            self.assertTrue(anonymous_mapping(path, "rw-s", scratch))
            self.assertFalse(anonymous_mapping(path, "r-xs", scratch))
        for path in ("/tmp/crt_shm_WK2SharedMemory.123", "/tmp/libc.so (deleted)",
                     "/elsewhere/crt_memfd_12_000001.tmp (deleted)"):
            self.assertFalse(anonymous_mapping(path, "rw-s", scratch))

    def test_candidate_paths_do_not_rewrite_inherited_static_examples(self):
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as directory:
            sdk, work = Path(directory) / "sdk", Path(directory) / "work"
            with patch.object(stage, "set_runtime_paths") as paths, \
                 patch.object(stage, "remove_staged_absolute_elf_rpaths"), patch.object(stage, "run"), \
                 patch.object(stage, "write_json"), patch.object(stage.network, "run_gate", return_value={"passed": True}), \
                 patch.object(stage.web.jsc, "host_abi_audit", return_value={"ok": True}):
                stage.validate_candidate(sdk, work, ROOT, {})
            actual = {call.args[0].relative_to(sdk).as_posix() for call in paths.call_args_list}
            self.assertEqual(actual, {"lib/libcrtweb.so", "examples/bin/crtweb_browser"} |
                             {"libexec/crtweb/" + name for name in ("crtweb_host", "WebProcess", "NetworkProcess", "crtweb_gpu_worker")})
    def test_target_discovery_includes_freetype_and_pkg_config_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cache = root / "CMakeCache.txt"
            for name in ("FREETYPE_LIBRARY_RELEASE:FILEPATH", "pkgcfg_lib_PC_LIBPSL_psl:FILEPATH",
                         "PC_LIBPSL_INCLUDEDIR:INTERNAL"):
                cache.write_text(name + "=" + str(root / "sdk/lib") + "\n")
                self.assertTrue(stage.audit_target_discovery(cache, root / "sdk", root / "ports", root / "source"))
                cache.write_text(name + "=/usr/host/library\n")
                with self.assertRaisesRegex(ValueError, "escaped"):
                    stage.audit_target_discovery(cache, root / "sdk", root / "ports", root / "source")

    @unittest.skipUnless(os.name == "nt", "native Windows drive-path containment")
    def test_target_discovery_normalizes_crt_drive_paths_without_relaxing_boundary(self):
        with tempfile.TemporaryDirectory(prefix="web discovery spaces ") as directory:
            root = Path(directory)
            cache = root / "CMakeCache.txt"
            name = "PC_HARFBUZZ_ICU_STATIC_LIBRARY_DIRS:INTERNAL"
            def crt(path):
                native = path.as_posix()
                return "/" + native[0].lower() + native[2:]
            for parent in (root / "sdk", root / "ports", root / "source"):
                value = crt(parent / "lib")
                cache.write_text(name + "=" + value + "\n")
                self.assertEqual(stage.audit_target_discovery(cache, root / "sdk", root / "ports", root / "source")[name], value)
            for value in (crt(root / "sdk-sibling/lib"), crt(root / "ports/../host/lib"),
                          "/z/host/lib", "/usr/host/lib"):
                cache.write_text(name + "=" + value + "\n")
                with self.subTest(value=value), self.assertRaisesRegex(ValueError, "escaped"):
                    stage.audit_target_discovery(cache, root / "sdk", root / "ports", root / "source")
    def test_output_cannot_mutate_predecessor_or_work(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stage.validate_layout(root / "sdk", root / "work", root / "output")
            for sdk, work, output in ((root / "sdk", root / "work", root / "sdk/output"),
                                      (root / "sdk", root / "work", root / "work/output"),
                                      (root / "sdk/work", root / "sdk", root / "output"),
                                      (root / "sdk", root / "work", root / "sdk")):
                with self.assertRaisesRegex(ValueError, "disjoint"):
                    stage.validate_layout(sdk, work, output)
    def test_observer_cache_is_owned_data_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sdk, work = root / "sdk with spaces", root / "run"
            (sdk / "share/crt").mkdir(parents=True)
            work.mkdir()
            (sdk / "share/crt/web-fonts.conf").write_text(
                '<fontconfig><dir prefix="relative">../fonts/crtweb</dir><cachedir>~/.cache/crtweb-fontconfig</cachedir></fontconfig>')
            path, cache = observer_font_config(sdk, work)
            self.assertIn(str(sdk / "share/fonts/crtweb"), path.read_text())
            self.assertNotIn("~", path.read_text())
            self.assertTrue(allowed_cache_mapping(cache / "data.cache-8", "r--p", cache))
            self.assertFalse(allowed_cache_mapping(cache / "data.cache-8", "r-xp", cache))
            self.assertFalse(allowed_cache_mapping(root / "data.cache-8", "r--p", cache))
            self.assertFalse(allowed_cache_mapping(cache / "libc.so", "r--p", cache))
    def test_cxx_aliases_coalesce_and_mismatched_images_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "lib").mkdir()
            for name in ("libc++", "libc++abi", "libunwind"):
                for suffix in (".so", ".so.1", ".so.1.0"):
                    (root / "lib" / (name + suffix)).write_bytes(b"same runtime")
            normalize_cxx_aliases(root)
            for name in ("libc++", "libc++abi", "libunwind"):
                self.assertTrue((root / "lib" / (name + ".so")).samefile(root / "lib" / (name + ".so.1")))
            normalize_cxx_aliases(root)
            alias = root / "lib/libc++.so"
            alias.unlink()
            alias.write_bytes(b"different ABI")
            with self.assertRaisesRegex(ValueError, "aliases differ"):
                normalize_cxx_aliases(root)

    def test_linux_runtime_publisher_preserves_aliases(self):
        import create_dist
        from build_stage_05_ui import publish_tree
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            install = root / "install"
            (install / "include/c++/v1").mkdir(parents=True)
            (install / "lib").mkdir()
            (install / "lib/libc++.so.1.0").write_bytes(b"test runtime")
            (install / "lib/libc++.so.1").symlink_to("libc++.so.1.0")
            (install / "lib/libc++.so").symlink_to("libc++.so.1")
            sdk = root / "sdk"
            sdk.mkdir()
            create_dist.copy_libcxx(install, sdk, "linux")
            self.assertTrue((sdk / "lib/libc++.so").samefile(sdk / "lib/libc++.so.1"))
            publish_tree(sdk, root / "published")
            self.assertTrue((root / "published/lib/libc++.so").samefile(root / "published/lib/libc++.so.1"))
    def test_stage_registration_and_only_explicit_existing_loader_boundary(self):
        self.assertEqual(STAGE_SUCCESSORS["05-ui"], "06-web")
        for target in ("linux", "macos", "windows"):
            base = external_prerequisites_for(target, "05-ui")
            current = external_prerequisites_for(target, "06-web")
            self.assertEqual(base, [item for item in current if item["id"] != "linux-elf-loader"])
            if target == "linux":
                self.assertEqual(current[-1]["components"], ["ld-linux-x86-64.so.2", "ld-linux-aarch64.so.1"])
                for arch, loader in (("x86_64", "ld-linux-x86-64.so.2"), ("aarch64", "ld-linux-aarch64.so.1")):
                    self.assertEqual(external_prerequisites_for("linux", "06-web", arch)[-1]["components"], [loader])

    def test_old_predecessor_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            sdk = Path(directory)
            with self.assertRaisesRegex(ValueError, "05-ui"):
                stage.require_predecessor(sdk, {"stage": "04-gfx-media"})
            with self.assertRaisesRegex(ValueError, "missing"):
                stage.require_predecessor(sdk, {"stage": "05-ui"})

    def test_other_hosts_report_the_unimplemented_gate_not_missing_linux_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sdk = root / "sdk"
            sdk.mkdir()
            stage.write_json(sdk / "manifest.json", {"stage": "05-ui", "target": {"os": "freebsd", "arch": "x86_64"}})
            result = subprocess.run([sys.executable, str(ROOT / "tools/build_stage_06_web.py"),
                                     "--sdk-root", str(sdk), "--asset-root", str(root / "asset"),
                                     "--work-root", str(root / "work"), "--output", str(root / "output"),
                                     "--recipe-location", "local-test", "--source-sha256", "0" * 64],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("native Linux/x86_64", result.stderr)
            self.assertNotIn("missing lib/libc.so", result.stderr)

    def test_supported_build_host_target_pairs_are_explicit(self):
        self.assertEqual(stage.build_kind({"os": "linux", "arch": "aarch64"}, "Linux", "x86_64"),
                         "cross-linux-aarch64")
        self.assertEqual(stage.build_kind({"os": "linux", "arch": "x86_64"}, "Linux", "amd64"),
                         "native")
        with self.assertRaisesRegex(ValueError, "x86_64-host -> Linux/aarch64-target"):
            stage.build_kind({"os": "linux", "arch": "aarch64"}, "Linux", "aarch64")

    def test_linux_wrappers_select_aarch64_on_an_x86_64_build_host(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sdk = root / "sdk"
            (sdk / "include").mkdir(parents=True)
            resource = root / "resource"
            (resource / "include").mkdir(parents=True)
            arguments = root / "arguments"
            compiler = root / "compiler"
            compiler.write_text("#!/bin/sh\n"
                                "if [ \"${1:-}\" = -print-resource-dir ]; then echo \"$FAKE_RESOURCE\"; exit 0; fi\n"
                                "printf '%s\\n' \"$@\" > \"$FAKE_ARGUMENTS\"\n")
            compiler.chmod(0o755)
            bindir = root / "bin"
            bindir.mkdir()
            uname = bindir / "uname"
            uname.write_text("#!/bin/sh\nprintf '%s\\n' x86_64\n")
            uname.chmod(0o755)
            source = root / "fixture.c"
            source.write_text("int fixture;\n")
            for wrapper, compiler_variable in (("crt-cc", "CRT_HOST_CC"), ("crt-c++", "CRT_HOST_CXX")):
                env = dict(os.environ, CRT_SYSROOT=str(sdk), CRT_TARGET_OS="linux", CRT_TARGET_ARCH="aarch64",
                           FAKE_RESOURCE=str(resource), FAKE_ARGUMENTS=str(arguments),
                           PATH=str(bindir) + os.pathsep + os.environ.get("PATH", ""))
                env[compiler_variable] = str(compiler)
                subprocess.run([ROOT / "tools" / wrapper, "-c", source, "-o", root / (wrapper + ".o")],
                               env=env, check=True)
                actual = arguments.read_text().splitlines()
                self.assertIn("--target=aarch64-linux-gnu", actual)
                self.assertIn("-fuse-ld=lld", actual)

    def test_host_environment_does_not_inherit_target_flags(self):
        from unittest.mock import patch
        with patch.dict("os.environ", {"CRT_TARGET_ARCH": "aarch64", "CC": "crt-cc", "CFLAGS": "--sysroot=bad", "CPATH": "bad"}):
            env = stage.host_environment()
            self.assertNotIn("CRT_TARGET_ARCH", env)
            self.assertNotIn("CFLAGS", env)
            self.assertNotIn("CPATH", env)
            self.assertNotIn("crt-cc", env["CC"])

    def test_media_symbol_manifest_matches_public_declarations(self):
        text = "\n".join(re.sub(r"/\*.*?\*/", "", p.read_text(), flags=re.S)
                         for p in (ROOT / "libcrtmedia/include/crtmedia").glob("*.h"))
        functions = set(re.findall(r"\b(crtmedia_\w+)\s*\(", text))
        manifest = (ROOT / "libcrtmedia/cmake/crtmedia_public_symbols.txt").read_text().split()
        self.assertEqual(len(manifest), len(set(manifest)))
        self.assertEqual(set(manifest), functions)

    def test_export_list_excludes_private_and_optional_symbols(self):
        if not all(shutil.which(tool) for tool in ("clang", "ar", "cmake", "nm")):
            self.skipTest("needs host compiler and binutils")
        nm = shutil.which("llvm-nm") or shutil.which("nm")
        if platform.system() != "Linux" and not shutil.which("llvm-nm"):
            self.skipTest("export generator requires LLVM/GNU nm, not Darwin nm")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            source = work / "fixture.c"
            source.write_text("int crtmedia_runtime_version(void) { return 1; }\n"
                              "int crtmedia_private_helper(void) { return 2; }\n"
                              "int av_fake(void) { return 3; }\nint GUID_fake;\n")
            subprocess.run(["clang", "-c", str(source), "-o", str(work / "fixture.o")], check=True)
            subprocess.run(["ar", "rcs", str(work / "fixture.a"), str(work / "fixture.o")], check=True)
            subprocess.run(["cmake", "-DNM=" + nm, "-DARCHIVE=" + str(work / "fixture.a"),
                            "-DPUBLIC_SYMBOLS=" + str(ROOT / "libcrtmedia/cmake/crtmedia_public_symbols.txt"),
                            "-DOUTPUT=" + str(work / "fixture.def"), "-P", str(ROOT / "libcrtmedia/cmake/crtmedia_exports.cmake")], check=True)
            self.assertEqual((work / "fixture.def").read_text(), "EXPORTS\ncrtmedia_runtime_version\n")
            if shutil.which("lld-link"):
                subprocess.run(["clang", "--target=x86_64-w64-mingw32", "-c", str(source),
                                "-o", str(work / "fixture.obj")], check=True)
                subprocess.run(["lld-link", "/dll", "/noentry", "/nodefaultlib",
                                "/def:" + str(work / "fixture.def"), "/out:" + str(work / "fixture.dll"),
                                str(work / "fixture.obj")], check=True)
                self.assertEqual(exported_symbols(work / "fixture.dll"), ["crtmedia_runtime_version"])
                # The runtime uses Clang's MinGW/GNU driver, whose DLL auto-
                # export behavior is distinct from lld-link's MSVC mode.
                if shutil.which("ld.lld"):
                    subprocess.run(["clang", "--target=x86_64-w64-mingw32", "-fuse-ld=lld",
                                    "-shared", "-nostdlib", "-Wl,--entry,crtmedia_runtime_version", str(work / "fixture.obj"),
                                    str(work / "fixture.def"), "-o", str(work / "fixture-gnu.dll")], check=True)
                    self.assertEqual(exported_symbols(work / "fixture-gnu.dll"), ["crtmedia_runtime_version"])

    def test_invalid_pe_export_table_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / "bad.dll"
            for data in (b"", b"MZ", b"MZ" + b"\0" * 70):
                file.write_bytes(data)
                with self.assertRaises(ValueError):
                    exported_symbols(file)

    def test_packaging_retry_checks_archive_source_and_compile_configuration(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = (ROOT / "tools/build_stage_06_web.py").read_text()
            driver = root / "tools/build_stage_06_web.py"
            driver.parent.mkdir()
            alias = '         "--define", "pkgcfg_lib_PC_CURL_curl=" + (prefix / "curl-openssl/lib/libcurl.a").as_posix(),\n'
            self.assertIn(alias, script)
            driver.write_text(script.replace(alias, ""))
            file = root / "libcrtweb/src/fixture.cc"
            file.parent.mkdir(parents=True)
            file.write_text("int fixture;\n")
            archive = root / "previous.tar.xz"
            with tarfile.open(archive, "w:xz") as stream:
                stream.add(driver, arcname="tools/build_stage_06_web.py")
                stream.add(file, arcname="libcrtweb/src/fixture.cc")
            sha = stage.digest(archive)
            driver.write_text(script)
            stage.validate_packaging_retry(archive, sha, root)
            driver.write_text(script.replace(alias, alias.replace("libcurl.a", "libcurl-other.a")))
            with self.assertRaisesRegex(ValueError, "compiled inputs"):
                stage.validate_packaging_retry(archive, sha, root)
            driver.write_text(script)
            driver.write_text(script.replace('["icu", "gperf", "libpng", "sqlite-amalgamation"]',
                                             '["icu", "gperf", "libpng", "sqlite-amalgamation", "zlib"]'))
            stage.validate_packaging_retry(archive, sha, root)
            driver.write_text(script.replace('["icu", "gperf", "libpng", "sqlite-amalgamation"]', '["icu"]'))
            with self.assertRaisesRegex(ValueError, "compiled inputs"):
                stage.validate_packaging_retry(archive, sha, root)
            driver.write_text(script.replace('"dataLicense": "CC0-1.0"', '"dataLicense": "test"'))
            stage.validate_packaging_retry(archive, sha, root)
            driver.write_text(script.replace("CURL_LIBRARY_RELEASE=", "CURL_LIBRARY_DEBUG="))
            with self.assertRaisesRegex(ValueError, "compiled inputs"):
                stage.validate_packaging_retry(archive, sha, root)
            driver.write_text(script)
            file.write_text("int changed;\n")
            with self.assertRaisesRegex(ValueError, "compiled inputs"):
                stage.validate_packaging_retry(archive, sha, root)
            with self.assertRaisesRegex(ValueError, "previous source"):
                stage.validate_packaging_retry(archive, "0" * 64, root)

    def test_web_validation_requires_real_security_and_no_generators(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            files = ["include/crtweb/" + name for name in ("crtweb.h", "webview.h", "gpu_surface.h",
                     "crtweb_surface_wire.h", "crtweb_gpu_wire.h")]
            files += ["libexec/crtweb/" + name for name in ("crtweb_host", "WebProcess", "NetworkProcess", "crtweb_gpu_worker")]
            files += ["examples/web-browser/main.cc", "examples/web-browser/CMakeLists.txt", "examples/bin/crtweb_browser",
                      "share/crt/cmake/crt_upper_imports.cmake", "share/crt/web-source-correspondence.json",
                      "share/crt/web-host-tools.json", "lib/libcrtweb.so"]
            files += ["libexec/crtweb/lib/" + name for name in ("libWebKit.so", "libcrtmedia.so", "libva.so.2", "libva-drm.so.2")]
            files += ["lib/" + name + ".so.1" for name in ("libc++", "libc++abi", "libunwind")]
            for name in files:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
            for name in ("libc++", "libc++abi", "libunwind"):
                (root / "lib" / (name + ".so")).symlink_to(name + ".so.1")
            now = datetime.now(timezone.utc)
            manifest = {"target": {"os": "linux"}, "created_utc": now.isoformat(),
                        "built_from": {"stage": "05-ui", "predecessor_content_sha256": "a" * 64},
                        "web_runtime": {"development_candidate": True}}
            stage.write_json(root / "share/crt/web-source-correspondence.json",
                             {"predecessor": {"stage": "05-ui", "content_sha256": "a" * 64}})
            redistributed = {"webkit": {"provenance": "recipe.json", "notices": ["notice"]}}
            stage.write_json(root / "recipe.json", {"version": "2.54.0"})
            stage.write_json(root / "share/crt/web-sbom.spdx.json", {"spdxVersion": "SPDX-2.3", "packages": [{}]})
            evidence = {"passed": True, "version": "2.54.0", "public_release_allowed": False,
                        "checked_utc": now.isoformat(), "advisories": [{"affected": False, "fixed_versions": ["2.54.0"]}]}
            stage.write_json(root / "share/crt/web-security.json", evidence)
            validate_web_stage(root, manifest, redistributed, "")
            cross_manifest = copy.deepcopy(manifest)
            cross_manifest["target"] = {"os": "linux", "arch": "aarch64"}
            with self.assertRaisesRegex(SystemExit, "cross-build evidence"):
                validate_web_stage(root, cross_manifest, redistributed, "")
            cross_manifest["web_runtime"]["cross_build_evidence"] = "share/crt/web-cross-build.json"
            stage.write_json(root / "share/crt/web-cross-build.json", {
                "passed": True, "build_host": {"os": "linux", "arch": "x86_64"},
                "target": {"os": "linux", "arch": "aarch64"},
                "host_generators": {name: {} for name in ("gperf", "perl", "python", "ruby", "genrb", "icupkg", "pkgdata")},
                "target_artifacts": {name: {} for name in ("icu", "libc++", "JavaScriptCore", "WebCore", "WebKit")},
            })
            validate_web_stage(root, cross_manifest, redistributed, "")
            missing_identity = copy.deepcopy(manifest)
            del missing_identity["built_from"]["predecessor_content_sha256"]
            with self.assertRaisesRegex(SystemExit, "predecessor content"):
                validate_web_stage(root, missing_identity, redistributed, "")
            stage.write_json(root / "share/crt/web-source-correspondence.json",
                             {"predecessor": {"content_sha256": "b" * 64}})
            with self.assertRaisesRegex(SystemExit, "different predecessor"):
                validate_web_stage(root, manifest, redistributed, "")
            stage.write_json(root / "share/crt/web-source-correspondence.json",
                             {"predecessor": {"content_sha256": "a" * 64}})
            for key, value in (("passed", False), ("version", "2.54.1"), ("advisories", []),
                               ("public_release_allowed", True),
                               ("checked_utc", (now - timedelta(days=8)).isoformat()),
                               ("advisories", [{"affected": False, "fixed_versions": ["2.54.1"]}])):
                with self.subTest(key=key, value=value):
                    bad = dict(evidence, **{key: value})
                    stage.write_json(root / "share/crt/web-security.json", bad)
                    with self.assertRaises(SystemExit):
                        validate_web_stage(root, manifest, redistributed, "")
            stage.write_json(root / "share/crt/web-security.json", evidence)
            (root / "bin").mkdir()
            (root / "bin/gperf").touch()
            with self.assertRaisesRegex(SystemExit, "build-host"):
                validate_web_stage(root, manifest, redistributed, "")


if __name__ == "__main__":
    unittest.main()
