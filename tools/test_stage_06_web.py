#!/usr/bin/env python3
"""Fast guards for the Web SDK boundary, host generators and advisory parser."""
import copy
from datetime import datetime, timezone, timedelta
import json
import io
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
            stage.write_json(sdk / "manifest.json", {"stage": "05-ui", "target": {"os": "windows", "arch": "x86_64"}})
            result = subprocess.run([sys.executable, str(ROOT / "tools/build_stage_06_web.py"),
                                     "--sdk-root", str(sdk), "--asset-root", str(root / "asset"),
                                     "--work-root", str(root / "work"), "--output", str(root / "output"),
                                     "--recipe-location", "local-test", "--source-sha256", "0" * 64],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("native only", result.stderr)
            self.assertNotIn("missing lib/libc.so", result.stderr)

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
