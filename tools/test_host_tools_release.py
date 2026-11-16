"""Host-tools release packing/verification contracts; fixtures do not execute native programs."""
import json
from pathlib import Path
import shutil
import sys
import tarfile
import io
import unittest
sys.path.insert(0, str(Path(__file__).parent))
import create_host_tools_release as release
import test_host_tools as base

TAG = 'v0.1.0-hosttools.20261116.linux-x86_64.1'


class HostToolsRelease(unittest.TestCase):
    def setUp(self):
        base.HostTools.setUp(self)
        self.bundle = self.root / 'bundle'
        producer = base.producer
        producer.assemble(self.sdk, self.bundle, TAG, promotion='preview')
        self.set_dirty(False)
        sdk_release = {'version': 'fixture-sdk', 'assets': [{'name': 'sdk.tar.xz', 'size': 1, 'sha256': '1' * 64,
            'install': {'format': 1, 'target': self.host, 'version': 'fixture-sdk', 'content_sha256': '2' * 64}}]}
        self.sdk_release = self.root / 'release-manifest.json'
        self.sdk_release.write_text(json.dumps(sdk_release))
        self.out = self.root / 'out'

    def set_dirty(self, dirty):
        path = self.bundle / 'manifest.json'
        manifest = json.loads(path.read_text())
        manifest['assembler']['working_tree_dirty'] = dirty
        path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')

    def pack(self, out=None, **kwargs):
        return release.pack(self.bundle, self.sdk_release, out or self.out, TAG, **kwargs)

    def test_record_carries_the_six_consumer_pins_and_provenance(self):
        record = self.pack()
        pin = record['consumer_pin']
        self.assertEqual(['manifest_sha256', 'name', 'sha256', 'size', 'url', 'version'], sorted(pin))
        archive = self.out / pin['name']
        self.assertEqual(pin['size'], archive.stat().st_size)
        self.assertEqual(pin['sha256'], release.sha256_file(archive))
        self.assertEqual(pin['manifest_sha256'], release.sha256_file(self.bundle / 'manifest.json'))
        self.assertEqual('2' * 64, record['source_sdk']['content_sha256'])
        self.assertFalse(record['producer']['assembler']['working_tree_dirty'])
        self.assertEqual('not_verified', record['trust']['publisher_authentication'])
        self.assertEqual('{}  {}\n'.format(pin['sha256'], pin['name']), (self.out / release.sums_name(self.host)).read_text())

    def test_packing_twice_gives_identical_bytes(self):
        self.pack()
        self.pack(self.root / 'out2')
        name = release.archive_name(TAG)
        self.assertEqual((self.out / name).read_bytes(), (self.root / 'out2' / name).read_bytes())

    def test_round_trip_extracts_a_verified_bundle(self):
        self.pack()
        record, root = release.verify(self.out / release.archive_name(TAG), self.out / release.record_name(self.host), self.root / 'extract')
        self.assertEqual(record['bundle']['manifest_sha256'], release.sha256_file(root / 'manifest.json'))
        self.assertTrue((root / 'system/bin/printf').is_file())

    def test_refusals_before_any_output(self):
        for label, change, pattern in (
                ('local-only', lambda: self.relabel('local-only'), 'promotion preview'),
                ('dirty', lambda: self.set_dirty(True), 'dirty tree'),
                ('tag', lambda: None, 'release tag')):
            with self.subTest(label):
                shutil.rmtree(self.out, ignore_errors=True)
                change()
                with self.assertRaisesRegex(ValueError, pattern):
                    if label == 'tag':
                        release.pack(self.bundle, self.sdk_release, self.out, 'v9.9.9')
                    else:
                        self.pack()
                self.assertFalse(self.out.exists())
                self.relabel('preview'); self.set_dirty(False)
        self.pack(allow_dirty=True)
        with self.assertRaisesRegex(ValueError, 'already exists'):
            self.pack()

    def relabel(self, value):
        path = self.bundle / 'manifest.json'
        manifest = json.loads(path.read_text()); manifest['promotion'] = value
        path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')

    def test_foreign_sdk_manifest_is_rejected(self):
        data = json.loads(self.sdk_release.read_text())
        data['assets'][0]['install']['version'] = 'another'
        self.sdk_release.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'does not describe the SDK'):
            self.pack()

    def test_changed_archive_record_or_member_is_rejected(self):
        self.pack()
        archive = self.out / release.archive_name(TAG); record = self.out / release.record_name(self.host)
        original = archive.read_bytes()
        archive.write_bytes(original[:-1] + bytes([original[-1] ^ 1]))
        with self.assertRaisesRegex(ValueError, 'differs from the release record'):
            release.verify(archive, record, self.root / 'e1')
        archive.write_bytes(original)
        data = json.loads(record.read_text()); data['bundle']['manifest_sha256'] = '0' * 64
        record.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'extracted bundle differs'):
            release.verify(archive, record, self.root / 'e2')

    def hostile(self, member_name, link=None):
        self.pack()
        archive = self.out / release.archive_name(TAG)
        import lzma
        raw = lzma.decompress(archive.read_bytes())
        source = tarfile.open(fileobj=io.BytesIO(raw))
        buffer = io.BytesIO()
        with tarfile.open(fileobj=buffer, mode='w', format=tarfile.PAX_FORMAT) as target:
            for member in source.getmembers():
                target.addfile(member, source.extractfile(member) if member.isreg() else None)
            info = tarfile.TarInfo(member_name); info.mtime = 0
            if link:
                info.type, info.linkname = tarfile.SYMTYPE, link
                target.addfile(info)
            else:
                info.size = 1; target.addfile(info, io.BytesIO(b'x'))
        archive.write_bytes(lzma.compress(buffer.getvalue(), format=lzma.FORMAT_XZ))
        record = self.out / release.record_name(self.host)
        data = json.loads(record.read_text())
        data['archive']['size'] = archive.stat().st_size; data['archive']['sha256'] = release.sha256_file(archive)
        record.write_text(json.dumps(data))
        (self.out / release.sums_name(self.host)).unlink()
        return archive, record

    def test_foreign_escaping_and_extra_members_are_rejected(self):
        top = release.top_name(self.host)
        archive, record = self.hostile('../escape')
        with self.assertRaisesRegex(ValueError, 'unsafe or foreign'):
            release.verify(archive, record, self.root / 'x1')
        shutil.rmtree(self.out)
        archive, record = self.hostile(top + '/extra-file')
        with self.assertRaisesRegex(ValueError, 'inventory mismatch'):
            release.verify(archive, record, self.root / 'x2')
        shutil.rmtree(self.out)
        archive, record = self.hostile(top + '/system/escape-link', link='../../../outside')
        with self.assertRaises(Exception):
            release.verify(archive, record, self.root / 'x3')

    def test_extraction_directory_must_be_new(self):
        self.pack()
        (self.root / 'taken').mkdir()
        with self.assertRaisesRegex(ValueError, 'already exists'):
            release.verify(self.out / release.archive_name(TAG), self.out / release.record_name(self.host), self.root / 'taken')


class AssemblerPromotion(unittest.TestCase):
    def setUp(self):
        base.HostTools.setUp(self)

    def test_promotion_is_a_closed_label_and_defaults_to_local_only(self):
        self.assertEqual('local-only', base.producer.assemble(self.sdk, self.root / 'a', 'v1')['promotion'])
        self.assertEqual('preview', base.producer.assemble(self.sdk, self.root / 'b', 'v1', promotion='preview')['promotion'])
        with self.assertRaisesRegex(ValueError, 'unknown host-tools promotion'):
            base.producer.assemble(self.sdk, self.root / 'c', 'v1', promotion='production')
        self.assertFalse((self.root / 'c').exists())
        manifest = json.loads((self.root / 'b/manifest.json').read_text()); manifest['promotion'] = 'official'
        (self.root / 'b/manifest.json').write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, 'unsupported host-tools manifest'):
            base.producer.verify_bundle(self.root / 'b')


if __name__ == '__main__':
    unittest.main()
