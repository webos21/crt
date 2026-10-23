"""Development profile/release negative fixtures, not native ABI acceptance."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import crt_dev_profile as profile
import crt_sdk_release as release


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
