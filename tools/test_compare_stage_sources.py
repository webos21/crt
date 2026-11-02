"""Separate source payload changes from compression/container differences."""
import io
import json
import lzma
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import compare_stage_sources as subject


class CompareSources(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='source comparison space ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def archive(self, name, members=None, *, mode=0o644, mtime=0, check=lzma.CHECK_CRC64):
        path = self.root / name
        if members is None:
            members = [('stage.json', b'{"source_commit":"same","working_tree_dirty":false}'), ('sources/main.c', b'int main(void) { return 0; }\n')]
        with lzma.open(path, 'wb', check=check) as compressed:
            with tarfile.open(fileobj=compressed, mode='w', format=tarfile.PAX_FORMAT) as archive:
                for member, payload in members:
                    info = tarfile.TarInfo(member)
                    info.mode, info.mtime, info.size = mode, mtime, len(payload)
                    archive.addfile(info, io.BytesIO(payload))
        return path

    def test_identical_bytes(self):
        report = subject.compare(self.archive('left.xz'), self.archive('right.xz'))
        self.assertEqual('identical', report['category'])
        self.assertEqual([], report['content_changes'])

    def test_compression_only_never_claims_changed_sources(self):
        report = subject.compare(self.archive('left.xz'), self.archive('right.xz', check=lzma.CHECK_SHA256))
        self.assertEqual('compression-only', report['category'])
        self.assertTrue(report['portable_members_equal'])
        self.assertFalse(report['exact_archive_equal'])
        self.assertTrue(report['uncompressed_tar_equal'])

    def test_metadata_and_order_are_visible_without_payload_changes(self):
        left = self.archive('left.xz', [('b', b'b'), ('a', b'a')])
        right = self.archive('right.xz', [('a', b'a'), ('b', b'b')], mtime=10)
        report = subject.compare(left, right)
        self.assertEqual('tar-container-only', report['category'])
        self.assertTrue(report['portable_members_equal'])
        self.assertFalse(report['member_order_equal'])
        self.assertEqual(2, len(report['metadata_changes']))

    def test_executable_permissions_are_not_normalized_away(self):
        report = subject.compare(self.archive('left.xz'), self.archive('right.xz', mode=0o755))
        self.assertEqual('permission-difference', report['category'])
        self.assertTrue(report['payload_equal'])
        self.assertFalse(report['portable_members_equal'])

    def test_changed_successor_pin_is_a_real_payload_change(self):
        def recipe(value):
            return [('stages/recipes/03-gfx-simple.json', json.dumps({'source': {'sha256': value}}).encode())]
        report = subject.compare(self.archive('left.xz', recipe('a' * 64)), self.archive('right.xz', recipe('b' * 64)))
        self.assertEqual('payload-difference', report['category'])
        self.assertEqual('/source/sha256', report['declaration_changes'][0]['fields'][0]['field'])

    def test_different_revisions_are_not_an_architecture_comparison(self):
        def members(commit):
            stage = {'format': 2, 'stage': '02-cxx', 'input_stage': '01-c', 'target': {'os': 'linux'},
                     'source_commit': commit, 'working_tree_dirty': False}
            return [('stage.json', json.dumps(stage).encode())]
        left = self.archive('left.xz', members('a' * 40))
        report = subject.compare(left, self.archive('right.xz', members('b' * 40)))
        self.assertFalse(report['same_declared_baseline'])
        self.assertTrue(subject.compare(left, left)['same_declared_baseline'])

    def test_added_removed_and_changed_members(self):
        report = subject.compare(self.archive('left.xz', [('changed', b'a'), ('removed', b'x')]),
                                 self.archive('right.xz', [('changed', b'b'), ('added', b'x')]))
        self.assertEqual(['added', 'modified', 'removed'], [item['change'] for item in report['content_changes']])

    def test_ambiguous_and_unsafe_paths_are_rejected_without_extraction(self):
        for index, members in enumerate(([('a', b'1'), ('a', b'2')], [('../escape', b'x')], [('C:/escape', b'x')])):
            with self.subTest(members=members), self.assertRaises(ValueError):
                subject.read_archive(self.archive(str(index) + '.xz', members))

    def test_special_members_are_rejected(self):
        path = self.root / 'link.xz'
        with tarfile.open(path, 'w:xz') as archive:
            info = tarfile.TarInfo('link')
            info.type, info.linkname = tarfile.SYMTYPE, '/outside'
            archive.addfile(info)
        with self.assertRaisesRegex(ValueError, 'link or special'):
            subject.read_archive(path)

    def test_cli_preserves_inputs_and_does_not_overwrite_evidence(self):
        left, right = self.archive('left.xz'), self.archive('right.xz', check=lzma.CHECK_SHA256)
        original = left.read_bytes(), right.read_bytes()
        output = self.root / 'report.json'
        self.assertEqual(1, subject.main([str(left), str(right), '--output', str(output), '--require-identical']))
        self.assertEqual('not-a-release-approval', json.loads(output.read_text())['approval'])
        with self.assertRaises(SystemExit):
            subject.main([str(left), str(right), '--output', str(output)])
        self.assertEqual(original, (left.read_bytes(), right.read_bytes()))


if __name__ == '__main__':
    unittest.main()
