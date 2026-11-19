import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
import zipfile
sys.path.insert(0, str(Path(__file__).parent))
import ci_host_tools_release as driver


class Driver(unittest.TestCase):
    def test_version_has_the_host_in_it_and_is_validated(self):
        self.assertEqual('v0.1.0-hosttools.20261116.linux-aarch64.1', driver.version_for('linux-aarch64', '20261116', '1'))
        self.assertEqual('v0.1.0-hosttools.20261119.macos-aarch64.2', driver.version_for('macos-aarch64', '20261119', '2'))
        self.assertEqual('v0.1.0-hosttools.20261119.windows-x86_64.1', driver.version_for('windows-x86_64', '20261119', '1'))
        self.assertEqual('v0.1.0-hosttools.20261119.windows-aarch64.1', driver.version_for('windows-aarch64', '20261119', '1'))
        for date, revision in (('2026-11-16', '1'), ('20261116', 'x'), ('20261116', '')):
            with self.assertRaises(ValueError):
                driver.version_for('linux-x86_64', date, revision)
        for key in ('linux-riscv64', 'macos-x86_64', 'windows-riscv64', 'x86_64'):
            with self.assertRaises(ValueError):
                driver.version_for(key, '20261116', '1')

    def test_repository_pins_exist_for_every_host(self):
        for key in driver.HOSTS:
            entry = driver.pin_for(key)
            self.assertTrue(entry['sdk_tag'].startswith('v0.0.1-preview.'), key)
            os_name, arch = key.split('-', 1)
            self.assertIn(os_name, entry['release_manifest'])
            self.assertIn(arch, entry['release_manifest'])

    def test_an_unpinned_or_malformed_host_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'pins.json'
            path.write_text(json.dumps({'hosts': {'linux-x86_64': {'release_manifest_sha256': 'abc'}}}))
            for key in driver.HOSTS:
                with self.assertRaises(ValueError):
                    driver.pin_for(key, path)

    def test_every_host_has_a_replay_driver(self):
        for os_name, script in driver.REPLAYS.items():
            self.assertTrue((Path(__file__).parent / script).is_file(), os_name)


class Extraction(unittest.TestCase):
    def test_unsafe_member_names_are_refused(self):
        for name in ('../escape', 'a/../../escape', '/abs', 'C:/Windows/x', 'C:\\x', '\\unc', ''):
            with self.assertRaises(ValueError, msg=name):
                driver.safe_member(name)
        self.assertEqual('dev/system/bin/sh', str(driver.safe_member('dev/system/bin/sh')))

    def test_a_zip_with_a_traversal_member_is_refused_and_a_clean_one_extracts(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            bad = root / 'bad.zip'
            with zipfile.ZipFile(bad, 'w') as z:
                z.writestr('../outside.txt', 'x')
            with self.assertRaises(ValueError):
                driver.extract_sdk(bad, 'zip', root / 'out-bad')
            self.assertFalse((root / 'outside.txt').exists())
            good = root / 'good.zip'
            with zipfile.ZipFile(good, 'w') as z:
                z.writestr('dev/manifest.json', '{}')
            (root / 'out-good').mkdir()
            driver.extract_sdk(good, 'zip', root / 'out-good')
            self.assertEqual('{}', (root / 'out-good/dev/manifest.json').read_text())

    def test_a_zip_symlink_member_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            archive = root / 'link.zip'
            with zipfile.ZipFile(archive, 'w') as z:
                info = zipfile.ZipInfo('dev/link')
                info.external_attr = (0o120777 << 16)
                z.writestr(info, 'target')
            with self.assertRaises(ValueError):
                driver.extract_sdk(archive, 'zip', root / 'out')

    def test_a_tar_with_a_traversal_member_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            archive = root / 'bad.tar.xz'
            with tarfile.open(archive, 'w:xz') as t:
                data = b'x'
                info = tarfile.TarInfo('../outside.txt')
                info.size = len(data)
                t.addfile(info, io.BytesIO(data))
            with self.assertRaises(ValueError):
                driver.extract_sdk(archive, 'tar.xz', root / 'out')
            self.assertFalse((root / 'outside.txt').exists())

    def test_an_unknown_archive_format_is_refused(self):
        with self.assertRaises(ValueError):
            driver.extract_sdk(Path('x'), 'rar', Path('y'))


if __name__ == '__main__':
    unittest.main()
