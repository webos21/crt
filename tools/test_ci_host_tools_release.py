import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).parent))
import ci_host_tools_release as driver


class Driver(unittest.TestCase):
    def test_version_has_the_host_in_it_and_is_validated(self):
        self.assertEqual('v0.1.0-hosttools.20261116.linux-aarch64.1', driver.version_for('aarch64', '20261116', '1'))
        for date, revision in (('2026-11-16', '1'), ('20261116', 'x'), ('20261116', '')):
            with self.assertRaises(ValueError):
                driver.version_for('x86_64', date, revision)

    def test_repository_pins_exist_for_both_linux_hosts(self):
        for arch in ('x86_64', 'aarch64'):
            entry = driver.pin_for(arch)
            self.assertTrue(entry['sdk_tag'].startswith('v0.0.1-preview.'))
            self.assertIn(arch, entry['release_manifest'])

    def test_an_unpinned_or_malformed_host_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'pins.json'
            path.write_text(json.dumps({'hosts': {'linux-x86_64': {'release_manifest_sha256': 'abc'}}}))
            for arch in ('x86_64', 'aarch64'):
                with self.assertRaises(ValueError):
                    driver.pin_for(arch, path)


if __name__ == '__main__':
    unittest.main()
