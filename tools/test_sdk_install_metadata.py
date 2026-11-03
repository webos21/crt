#!/usr/bin/env python3
"""Tests for tools/crt_sdk_install_metadata.py: identity rule, counts, and agreement with the real archives."""

import hashlib
import json
import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import crt_sdk_install_metadata as meta
import prepare_release_assets as release


def make_sdk(root: Path, name="dev") -> Path:
    sdk = root / name
    (sdk / "share/crt/contracts").mkdir(parents=True)
    (sdk / "lib").mkdir()
    (sdk / "empty").mkdir()
    target = {"os": "linux", "arch": "x86_64"}
    (sdk / "manifest.json").write_text(json.dumps({"stage": "02-cxx", "target": target}))
    (sdk / "share/crt/contracts/development.json").write_text(json.dumps({"format": 1, "profile": "dev"}))
    (sdk / "VERSION").write_text("v1.2.3\n")
    (sdk / "lib/Zlib.a").write_bytes(b"z" * 10)
    (sdk / "lib/alib.a").write_bytes(b"a" * 7)
    (sdk / "lib/liba.so").symlink_to("alib.a")
    return sdk


class InstallMetadata(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.sdk = make_sdk(Path(self.temp.name))

    def test_counts_and_limits(self):
        data = meta.install_metadata(self.sdk, "tar.xz")
        self.assertEqual((data["files"], data["directories"], data["symlinks"], data["entries"]), (5, 6, 1, 12))
        self.assertEqual(data["uncompressed_bytes"], sum(p.stat().st_size for p in self.sdk.rglob("*") if p.is_file() and not p.is_symlink()))
        self.assertEqual((data["root"], data["version"], data["stage"], data["profile"]), ("dev", "v1.2.3", "02-cxx", "dev"))
        self.assertEqual(data["max_depth"], 5)  # dev/share/crt/contracts/development.json
        self.assertEqual(data["max_path_bytes"], len("dev/share/crt/contracts/development.json"))
        self.assertEqual(data["max_file_bytes"], max(len(b"z" * 10), len((self.sdk / "manifest.json").read_bytes()),
                                                      len((self.sdk / "share/crt/contracts/development.json").read_bytes())))

    def test_identity_is_the_case_sensitive_compact_inventory_hash(self):
        items = meta.inventory(self.sdk)
        # 'Zlib.a' sorts before 'alib.a' component-wise and case-sensitively.
        names = [item[0] for item in items]
        self.assertLess(names.index("lib/Zlib.a"), names.index("lib/alib.a"))
        self.assertIn(["lib/liba.so", "symlink", "alib.a"], items)
        compact = json.dumps(items, ensure_ascii=False, separators=(",", ":"))
        self.assertEqual(meta.install_metadata(self.sdk, "zip")["content_sha256"], hashlib.sha256(compact.encode()).hexdigest())

    def test_identity_ignores_directories_and_modes_but_not_content(self):
        before = meta.install_metadata(self.sdk, "zip")["content_sha256"]
        (self.sdk / "another-empty").mkdir()
        (self.sdk / "lib/alib.a").chmod(0o755)
        self.assertEqual(meta.install_metadata(self.sdk, "zip")["content_sha256"], before)
        (self.sdk / "lib/alib.a").write_bytes(b"changed")
        self.assertNotEqual(meta.install_metadata(self.sdk, "zip")["content_sha256"], before)

    def test_escaping_symlink_is_refused(self):
        (self.sdk / "evil").symlink_to("/etc")
        with self.assertRaises(ValueError):
            meta.install_metadata(self.sdk, "zip")

    def test_entry_count_matches_the_real_archives(self):
        out = Path(self.temp.name) / "out"
        out.mkdir()
        for suffix, os_name in ((".tar.xz", "linux"), (".zip", "windows")):
            archive = release.archive_directory(self.sdk, "dev", "v1.2.3", os_name, "x86_64", out)
            self.assertTrue(archive.name.endswith(suffix))
            if suffix == ".zip":
                with zipfile.ZipFile(archive) as z:
                    names = z.namelist()
            else:
                with tarfile.open(archive) as t:
                    names = t.getnames()
            data = meta.install_metadata(self.sdk, "zip" if suffix == ".zip" else "tar.xz")
            self.assertEqual(len(names), data["entries"], suffix)
            self.assertEqual(max(len(n.rstrip("/").encode()) for n in names), data["max_path_bytes"], suffix)


if __name__ == "__main__":
    unittest.main()
