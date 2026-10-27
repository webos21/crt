"""Development profile/release negative fixtures, not native ABI acceptance."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import crt_dev_profile as profile
import crt_sdk_release as release
import test_dev_sdk_external as external


class ExternalConsumerBoundary(unittest.TestCase):
    def test_failed_native_preflight_removes_a_stale_success_receipt(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            sdk, output = root / 'sdk', root / 'evidence'
            sdk.mkdir()
            (sdk / 'manifest.json').write_text(json.dumps({'target': {'os': 'unsupported', 'arch': 'foreign'}}))
            output.mkdir()
            result = output / 'result.json'
            result.write_text('{"passed": true}')
            with self.assertRaisesRegex(ValueError, 'native Linux/macOS/Windows SDK'):
                external.main(['--sdk-root', str(sdk), '--output', str(output)])
            self.assertFalse(result.exists())

    def test_foreign_target_cannot_be_executed_as_native_acceptance(self):
        from unittest.mock import patch
        with patch.object(external.sys, 'platform', 'linux'), patch.object(external.platform, 'machine', return_value='x86_64'):
            external.native_target({'target': {'os': 'linux', 'arch': 'x86_64'}})
            for target in ({'os': 'linux', 'arch': 'aarch64'}, {'os': 'windows', 'arch': 'x86_64'}):
                with self.assertRaisesRegex(ValueError, 'cross targets are compile-only'):
                    external.native_target({'target': target})

    def test_macos_only_accepts_the_native_architecture(self):
        from unittest.mock import patch
        with patch.object(external.sys, 'platform', 'darwin'), patch.object(external.platform, 'machine', return_value='arm64'):
            self.assertEqual('macos', external.native_target({'target': {'os': 'macos', 'arch': 'aarch64'}}))
            for target in ({'os': 'macos', 'arch': 'x86_64'}, {'os': 'linux', 'arch': 'aarch64'}):
                with self.assertRaisesRegex(ValueError, 'cross targets are compile-only'):
                    external.native_target({'target': target})

    def test_windows_only_accepts_the_native_architecture(self):
        from unittest.mock import patch
        with patch.object(external.sys, 'platform', 'win32'), patch.object(external.platform, 'machine', return_value='AMD64'):
            self.assertEqual('windows', external.native_target({'target': {'os': 'windows', 'arch': 'x86_64'}}))
            with self.assertRaisesRegex(ValueError, 'cross targets are compile-only'):
                external.native_target({'target': {'os': 'windows', 'arch': 'aarch64'}})
        with patch.object(external.sys, 'platform', 'win32'), patch.object(external.platform, 'machine', return_value='ARM64'):
            self.assertEqual('windows', external.native_target({'target': {'os': 'windows', 'arch': 'aarch64'}}))

    def test_windows_environment_drops_ambient_paths_and_requires_external_sdk_libs(self):
        import os
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            libraries = root / 'host sdk'
            libraries.mkdir()
            (libraries / 'kernel32.lib').write_text('fixture')
            with patch.dict(os.environ, {'SystemRoot': str(root / 'Windows'), 'CRT_WINDOWS_SDK_LIBPATH': str(libraries),
                                         'PYTHONPATH': '/ambient', 'CURL_CA_BUNDLE': '/ambient'}), \
                    patch.object(external.shutil, 'which', return_value=str(root / 'host/clang.exe')):
                env = external.windows_environment(root / 'work', [root / 'producer'])
                self.assertNotIn('PYTHONPATH', env)
                self.assertNotIn('CURL_CA_BUNDLE', env)
                self.assertEqual('windows-restricted-token', env['CRT_TEST_ISOLATION'])
                # hidden_roots() hands the environment builders resolved directories; the fixture must do the
                # same, or a temporary directory behind a link or a short name (/var -> /private/var on macOS,
                # RUNNER~1 on Windows) never compares equal to the resolved tool path.
                with self.assertRaisesRegex(ValueError, 'inside a hidden producer tree'):
                    external.windows_environment(root / 'work', [(root / 'host').resolve()])
                with self.assertRaisesRegex(ValueError, 'external native Windows SDK'):
                    external.windows_environment(root / 'work', [libraries.resolve()])

    @unittest.skipUnless(sys.platform == 'win32', 'native Windows ACL/token fixture')
    def test_windows_acl_setup_refuses_existing_directory(self):
        import crt_windows_external as windows
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            with self.assertRaises(FileExistsError):
                windows.create_work(root)
            with self.assertRaisesRegex(ValueError, 'non-root directory'):
                external.hidden_roots(Path(root.anchor))

    @unittest.skipUnless(sys.platform == 'win32', 'native Windows restricted-token process')
    def test_windows_token_denies_private_files_and_inherits_to_cmd_children(self):
        import os
        import shutil
        import crt_windows_external as windows
        work, sid = windows.create_work()
        protected, _ = windows.create_work()
        self.addCleanup(shutil.rmtree, work)
        self.addCleanup(shutil.rmtree, protected)
        secret = protected / 'secret.txt'
        secret.write_text('not accessible')
        source = protected / 'source.txt'
        source.write_text('Bionic fixture source')
        script = work / 'probe.cmd'
        script.write_text('@echo off\n'
                          'echo allowed>allowed.txt\n'
                          f'type "{secret}" >denied.txt 2>&1\n'
                          'if not errorlevel 1 exit /b 11\n'
                          f'"%COMSPEC%" /d /c type "{source}" >child-denied.txt 2>&1\n'
                          'if not errorlevel 1 exit /b 12\n'
                          'exit /b 0\n')
        # Explicit '.\': with NODEFAULTCURRENTDIRECTORYINEXEPATH=1 in the environment (set by
        # some tool launchers) cmd.exe does not look for a bare name in the current directory.
        windows.run([os.environ['COMSPEC'], '/d', '/c', r'.\probe.cmd'], work=work,
                    sid=sid, env=dict(os.environ), timeout=15)
        self.assertEqual('allowed', (work / 'allowed.txt').read_text().strip())
        self.assertNotIn('not accessible', (work / 'denied.txt').read_text(errors='replace'))
        self.assertNotIn('Bionic', (work / 'child-denied.txt').read_text(errors='replace'))

    def test_windows_ca_path_conversion_does_not_import_a_web_producer(self):
        from pathlib import PureWindowsPath
        import test_dev_sdk_consumer as consumer
        self.assertEqual('/c/CA path with spaces/ca.pem', consumer.target_path(PureWindowsPath('C:/CA path with spaces/ca.pem'), True))
        self.assertEqual('/tmp/ca.pem', consumer.target_path(Path('/tmp/ca.pem'), False))
        self.assertNotIn('build_webkit_crt_port.py', {p.name for p in external.validation_scripts()})

    def test_failed_windows_token_has_no_unrestricted_retry_or_success_receipt(self):
        import subprocess
        from unittest.mock import patch
        import crt_windows_external as windows
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            sdk, work, output = root / 'sdk', root / 'work', root / 'evidence'
            sdk.mkdir()
            work.mkdir()
            output.mkdir()
            (sdk / 'manifest.json').write_text('{"target":{"os":"windows","arch":"x86_64"}}')
            for relative in ('include/stdio.h', 'lib/libc.a', 'bin/libc.dll'):
                path = sdk / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('fixture')
            (output / 'result.json').write_text('{"passed":true}')
            with patch.object(external, 'native_target', return_value='windows'), \
                    patch.object(windows, 'create_work', return_value=(work, 'fixture-sid')), \
                    patch.object(external, 'windows_environment', return_value={}), \
                    patch.object(external, 'windows_python', return_value=Path(sys.executable)), \
                    patch.object(windows, 'run', side_effect=subprocess.CalledProcessError(1, ['restricted'])) as run, \
                    patch.object(external.subprocess, 'run') as unrestricted:
                with self.assertRaises(subprocess.CalledProcessError):
                    external.main(['--sdk-root', str(sdk), '--output', str(output)])
            self.assertEqual(1, run.call_count)
            unrestricted.assert_not_called()
            self.assertFalse((output / 'result.json').exists())

    def test_hidden_tree_cannot_contain_or_be_contained_in_the_external_work(self):
        root = Path(tempfile.gettempdir()).resolve() / 'crt-external-boundary-fixture'
        for work, hidden in ((root, root), (root / 'work', root), (root, root / 'producer')):
            with self.assertRaisesRegex(ValueError, 'disjoint'):
                external.sandbox_command('bwrap', work, [hidden], sys.executable, [])

    def test_validation_capsule_excludes_dev_producer_and_runtime_build_sources(self):
        names = {p.name for p in external.validation_scripts()}
        self.assertIn('test_dev_sdk_consumer.py', names)
        self.assertIn('crt_elf.py', names)
        self.assertIn('audit_macos_bindings.py', names)
        self.assertNotIn('build_dev_sdk.py', names)
        self.assertNotIn('crt-libcxx-build.py', names)
        self.assertTrue(all(p.parent == external.ROOT / 'tools' for p in external.validation_scripts()))

    def test_macos_sandbox_denies_reads_and_writes_with_quoted_paths(self):
        root = Path(tempfile.gettempdir()).resolve()
        hidden = [root / 'producer "quoted" path']
        work = root / 'separate work'
        command = external.macos_sandbox_command('sandbox-exec', work, hidden, 'python', ['probe.py'])
        self.assertEqual(['sandbox-exec', '-p'], command[:2])
        self.assertIn('(deny file-read* file-write* (subpath ' + json.dumps(str(hidden[0])) + '))', command[2])
        self.assertEqual(['python', 'probe.py'], command[3:])
        with self.assertRaisesRegex(ValueError, 'disjoint'):
            external.macos_sandbox_command('sandbox-exec', hidden[0] / 'work', hidden, 'python', [])

    def test_missing_macos_sandbox_has_no_unisolated_fallback(self):
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            sdk, output = root / 'sdk', root / 'evidence'
            sdk.mkdir()
            (sdk / 'manifest.json').write_text('{"target":{"os":"macos","arch":"aarch64"}}')
            with patch.object(external, 'native_target', return_value='macos'), patch.object(external.shutil, 'which', return_value=None):
                with self.assertRaisesRegex(ValueError, 'requires sandbox-exec; no unisolated fallback'):
                    external.main(['--sdk-root', str(sdk), '--output', str(output)])
            self.assertFalse((output / 'result.json').exists())

    def test_failed_macos_sandbox_removes_receipt_without_retrying_unisolated(self):
        import subprocess
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            sdk, output = root / 'sdk', root / 'evidence'
            sdk.mkdir()
            output.mkdir()
            (sdk / 'manifest.json').write_text('{"target":{"os":"macos","arch":"aarch64"}}')
            (output / 'result.json').write_text('{"passed":true}')
            with patch.object(external, 'native_target', return_value='macos'), \
                    patch.object(external.shutil, 'which', return_value='/usr/bin/sandbox-exec'), \
                    patch.object(external.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, ['sandbox-exec'])) as run:
                with self.assertRaises(subprocess.CalledProcessError):
                    external.main(['--sdk-root', str(sdk), '--output', str(output), '--work-root', str(root / 'work')])
            self.assertEqual(1, run.call_count)
            self.assertFalse((output / 'result.json').exists())

    def test_macos_environment_rejects_a_hidden_compiler_and_drops_ambient_settings(self):
        from unittest.mock import patch
        root = Path(tempfile.gettempdir()).resolve()
        with patch.object(external.shutil, 'which', return_value=str(root / 'producer/clang')):
            with self.assertRaisesRegex(ValueError, 'inside a hidden producer tree'):
                external.macos_environment(root / 'work', [root / 'producer'])
        with patch.dict(external.os.environ, {'DYLD_LIBRARY_PATH': '/ambient', 'PYTHONPATH': '/ambient'}), \
                patch.object(external.shutil, 'which', return_value=str(root / 'host/clang')):
            env = external.macos_environment(root / 'work', [root / 'producer'])
            self.assertNotIn('DYLD_LIBRARY_PATH', env)
            self.assertNotIn('PYTHONPATH', env)


class DevelopmentProfile(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="dev sdk fixture spaces ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.dev = self.root / "dev"
        self.full = self.root / "02-cxx"
        self.manifest = {"stage": "02-cxx", "target": {"os": "windows", "arch": "x86_64"},
                         "source_commit": "a" * 40, "redistributed_dependencies": []}
        self.dev.mkdir()
        for relative in ("lib/libc.a", "bin/libc.dll", "lib/crt1.o"):
            self.put(relative)
        for name in sorted(profile.PORTS):
            shared = [f"bin/{stem}.dll" for stem in profile.SHARED[name]]
            for relative in (*profile.STATIC[name], *shared):
                self.put(relative)
            self.manifest["redistributed_dependencies"].append({"name": name,
                "link_artifacts": list(profile.STATIC[name]), "runtime_artifacts": shared})
        self.data = {"format": 1, "profile": "dev", "version": "v1.0.0", "target": self.manifest["target"],
                     "producer_source_commit": "a" * 40, "base_source_commit": "a" * 40,
                     "producer_working_tree_dirty": False,
                     "compile_provenance": {"source_commit": "a" * 40, "working_tree_dirty": False},
                     "runtime_sha256": profile.runtime_digests(self.dev),
                     "consumer_compile_options": profile.consumer_options("windows"),
                     "tls_trust": {"mode": "explicit-ca-file", "bundled": False}}
        self.save()
        import shutil
        shutil.copytree(self.dev, self.full)

    def put(self, relative, content="fixture"):
        path = self.dev / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.suffix == ".dll" and content == "fixture":
            import struct
            data = bytearray(152)
            data[:2] = b"MZ"
            struct.pack_into("<I", data, 60, 128)
            data[128:132] = b"PE\0\0"
            struct.pack_into("<H", data, 150, 0x2000)
            path.write_bytes(data)
        else:
            path.write_text(content)

    def save(self):
        self.put(profile.SIDECAR, json.dumps(self.data))
        self.put("manifest.json", json.dumps(self.manifest))
        self.put("VERSION", "v1.0.0\n")

    def test_valid_profile_and_same_runtime_pair(self):
        self.assertEqual([], profile.check(self.dev, self.manifest))
        release.check_pair(self.dev, self.full, "v1.0.0")

    def test_development_fetch_can_keep_downloads_in_its_own_work_root(self):
        import build_stage_04_gfx_media as media
        from unittest.mock import patch
        downloads = self.root / "work/downloads"
        with patch.object(media, "run") as run:
            media.fetch_port_sources(self.root, self.dev, self.root / "work/run", {},
                                     ports=("curl",), download_cache=downloads)
        command = run.call_args.args[0]
        self.assertEqual(str(downloads), command[command.index("--cache") + 1])
        self.assertEqual(["--port", "curl"], command[-2:])

    def test_full_web_uses_crt_identity_not_the_upstream_webkit_commit(self):
        manifest = dict(self.manifest, stage="06-web", built_from={"source_sha256": "c" * 64})
        (self.full / "manifest.json").write_text(json.dumps(manifest))
        correspondence = self.full / "share/crt/web-source-correspondence.json"
        correspondence.parent.mkdir(parents=True, exist_ok=True)
        correspondence.write_text(json.dumps({"source_commit": "b" * 40}))
        recipe = self.full / "stages/recipes/06-web.json"
        recipe.parent.mkdir(parents=True)
        source = {"source_commit": "a" * 40, "sha256": "c" * 64}
        recipe.write_text(json.dumps({"source": source}))
        release.check_pair(self.dev, self.full, "v1.0.0")
        source["source_commit"] = "d" * 40
        recipe.write_text(json.dumps({"source": source}))
        with self.assertRaisesRegex(release.release.ReleaseError, "overlay recipe"):
            release.check_pair(self.dev, self.full, "v1.0.0")

    def test_missing_static_or_shared_port_is_rejected(self):
        for name in sorted(profile.PORTS):
            path = self.dev / profile.STATIC[name][0]
            original = path.read_bytes()
            path.unlink()
            self.assertTrue(any(name in problem for problem in profile.check(self.dev, self.manifest)))
            path.write_bytes(original)
            for stem in profile.SHARED[name]:
                path = self.dev / f"bin/{stem}.dll"
                path.unlink()
                self.assertTrue(any(stem in problem for problem in profile.check(self.dev, self.manifest)))
                self.put(f"bin/{stem}.dll")

    def test_modified_inherited_runtime_rejected(self):
        (self.dev / "bin/libc.dll").write_text("changed")
        self.assertIn("Development SDK inherited runtime digest mismatch", profile.check(self.dev, self.manifest))

    def test_upper_content_and_ambient_trust_are_rejected(self):
        self.put("include/crtweb/api.h")
        self.data["tls_trust"]["bundled"] = True
        self.save()
        problems = profile.check(self.dev, self.manifest)
        self.assertTrue(any("upper-stage" in p for p in problems))
        self.assertTrue(any("explicit CA" in p for p in problems))

    def test_stale_full_or_unknown_source_rejected(self):
        for key, value in (("base_source_commit", None), ("producer_source_commit", "b" * 40),
                           ("producer_working_tree_dirty", True)):
            old = self.data[key]
            self.data[key] = value
            self.save()
            with self.assertRaisesRegex(release.release.ReleaseError, "same known clean source"):
                release.check_pair(self.dev, self.full, "v1.0.0")
            self.data[key] = old
        self.save()
        (self.full / "bin/libc.dll").write_text("different Full runtime")
        with self.assertRaisesRegex(release.release.ReleaseError, "bytes differ"):
            release.check_pair(self.dev, self.full, "v1.0.0")

    def test_local_tag_fails_before_build_or_upload(self):
        with self.assertRaises(release.release.ReleaseError):
            release.main(["--profile", "dev", "--version", "development", "--development-sdk", str(self.dev),
                          "--full-sdk", str(self.full), "--source-assets", str(self.root / "assets"),
                          "--output-dir", str(self.root / "release")])
        self.assertFalse((self.root / "release").exists())

    def test_clean_collector_with_a_different_commit_is_rejected(self):
        from unittest.mock import patch
        with patch.object(release.subprocess, "check_output", side_effect=
                          lambda args, **kwargs: "" if "status" in args else "b" * 40):
            with self.assertRaisesRegex(release.release.ReleaseError, "collector checkout"):
                release.main(["--profile", "dev", "--version", "v1.0.0", "--development-sdk", str(self.dev),
                              "--full-sdk", str(self.full), "--source-assets", str(self.root / "assets"),
                              "--output-dir", str(self.root / "release")])
        self.assertFalse((self.root / "release").exists())

    def test_full_source_dirty_or_legacy_metadata_is_not_promoted_to_clean(self):
        import io
        import tarfile
        sources = self.root / "sources"
        sources.mkdir()
        archive = sources / "full-source.tar.xz"
        recipe = self.full / "stages/recipes/06-web.json"
        recipe.parent.mkdir(parents=True)
        for dirty in (False, True, None):
            metadata = {"source_commit": "a" * 40}
            if dirty is not None:
                metadata["working_tree_dirty"] = dirty
            payload = json.dumps(metadata).encode()
            with tarfile.open(archive, "w:xz") as stream:
                member = tarfile.TarInfo("stage.json")
                member.size = len(payload)
                stream.addfile(member, io.BytesIO(payload))
            recipe.write_text(json.dumps({"source": {"archive": archive.name, "size": archive.stat().st_size,
                "sha256": profile.digest(archive), "source_commit": "a" * 40}}))
            if dirty is False:
                release.check_full_source(self.full, sources)
            else:
                with self.assertRaisesRegex(release.release.ReleaseError, "known clean"):
                    release.check_full_source(self.full, sources)

    def test_windows_posix_soname_must_be_a_real_pe_dll_not_an_import_archive(self):
        path = self.dev / "bin/libz.dll"
        actual = path.read_bytes()
        path.unlink()
        self.put("lib/libz.so.1", "not a PE image")
        self.assertFalse(profile.is_shared_runtime(self.dev / "lib/libz.so.1", "windows"))
        (self.dev / "lib/libz.so.1").write_bytes(actual)
        self.assertTrue(profile.is_shared_runtime(self.dev / "lib/libz.so.1", "windows"))
        self.put("lib/libz.dll.a", "archive, not a runtime")
        self.assertFalse(profile.is_shared_runtime(self.dev / "lib/libz.dll.a", "windows"))

    def test_release_fixture_is_atomic_and_does_not_replace_an_existing_identity(self):
        from unittest.mock import patch
        args = ["--profile", "dev", "--version", "v1.0.0", "--development-sdk", str(self.dev),
                "--full-sdk", str(self.full), "--source-assets", str(self.root / "sources"),
                "--output-dir", str(self.root / "release")]
        git_output = lambda args, **kwargs: "" if "status" in args else "a" * 40
        with patch.object(release.subprocess, "check_output", side_effect=git_output), \
             patch.object(release, "run"), patch.object(release.release, "check_sdk", return_value=({}, [])):
            self.assertEqual(0, release.main(args))
            target = self.root / "release"
            checksums = (target / "SHA256SUMS-dev-windows-x86_64").read_text()
            self.assertIn("crt-v1.0.0-windows-x86_64-dev.zip", checksums)
            self.assertTrue((target / "release-manifest-dev-windows-x86_64.json").is_file())
            with self.assertRaisesRegex(release.release.ReleaseError, "already exists"):
                release.main(args)
            self.assertEqual(checksums, (target / "SHA256SUMS-dev-windows-x86_64").read_text())
        second = [*args[:-1], str(self.root / "failed-release")]
        with patch.object(release.subprocess, "check_output", side_effect=git_output), \
             patch.object(release, "run", side_effect=ValueError("injected verifier failure")):
            with self.assertRaisesRegex(ValueError, "injected"):
                release.main(second)
        self.assertFalse((self.root / "failed-release").exists())

    def test_in_tree_full_sdk_copies_both_windows_network_runtime_locations(self):
        import create_dist
        from unittest.mock import patch
        prefix = self.root / "ports"
        skia = self.root / "skia"
        destination = self.root / "04-gfx-media"
        destination.mkdir()
        for relative in ("include/freetype2", "include/libavformat", "include/libavcodec", "include/libswresample",
                         "include/libavutil", "include/curl", "include/mbedtls", "include/psa"):
            path = prefix / relative
            path.mkdir(parents=True)
            (path / "fixture.h").write_text("fixture")
        paths = ["include/zlib.h", "include/zconf.h", "lib/libfreetype.a", "lib/libavformat.a", "lib/libavcodec.a",
                 "lib/libswresample.a", "lib/libavutil.a", "lib/libcurl.a", "lib/libmbedtls.a", "lib/libmbedx509.a",
                 "lib/libmbedcrypto.a", "lib/libz.a", "lib/libz.so.1.3.1", "lib/libmbedtls.dll",
                 "lib/libmbedtls.dll.a", "bin/libcurl-4.dll", "lib/libcurl.dll.a"]
        for relative in paths:
            path = prefix / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture")
        (skia / "modules/skcms").mkdir(parents=True)
        (skia / "modules/skcms/fixture.h").write_text("fixture")
        (skia / "libskia.a").write_text("fixture")
        with patch("build_stage_04_gfx_media.add_redistributed_dependencies"):
            create_dist.copy_media_development_dependencies(self.root, destination, prefix, self.root,
                                                           skia, skia / "libskia.a", [])
        for relative in ("lib/libz.so.1.3.1", "lib/libmbedtls.dll", "lib/libmbedtls.dll.a", "bin/libcurl-4.dll"):
            self.assertEqual("fixture", (destination / relative).read_text())


if __name__ == "__main__":
    unittest.main()
