"""Tests of tools/ci_sdk_release.py: a produced Dev SDK asset set is accepted only when every claim is true of the bytes."""
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import tarfile
import tempfile
import unittest
import zipfile

import ci_sdk_release as verifier

VERSION = 'v0.0.1-ci.7'
COMMIT = 'a' * 40


def digest(data):
    return hashlib.sha256(data).hexdigest()


class Fixture:
    """A small but structurally faithful asset set; install metadata is derived from the extracted tree, as the producer does."""

    def __init__(self, folder, os_name='linux', arch='x86_64', version=VERSION, commit=COMMIT):
        self.dir, self.os, self.arch, self.version, self.commit = Path(folder), os_name, arch, version, commit
        self.names = verifier.expected_names(version, os_name, arch)
        self.tree = self.dir / 'tree' / 'dev'
        (self.tree / 'share/crt/contracts').mkdir(parents=True)
        (self.tree / 'lib').mkdir()
        (self.tree / 'VERSION').write_text(version + '\n')
        (self.tree / 'manifest.json').write_text(json.dumps({'stage': '02-cxx', 'target': {'os': os_name, 'arch': arch}}))
        (self.tree / 'share/crt/contracts/development.json').write_text(json.dumps({'profile': 'dev'}))
        (self.tree / 'lib/libc.a').write_bytes(b'archive bytes')
        if os_name != 'windows':
            os.symlink('libc.a', self.tree / 'lib/libc.alias')
        self.assets = self.dir / 'assets'
        self.assets.mkdir()

    def build(self, mutate_manifest=None, mutate_contract=None):
        archive = self.assets / self.names['archive']
        if archive.suffix == '.zip':
            with zipfile.ZipFile(archive, 'w') as bundle:
                for path in sorted(self.tree.rglob('*')):
                    bundle.write(path, 'dev/' + path.relative_to(self.tree).as_posix())
        else:
            with tarfile.open(archive, 'w:xz') as bundle:
                bundle.add(self.tree, arcname='dev')
        install = dict(verifier.inventory(self.tree), format=1, archive_format='zip' if archive.suffix == '.zip' else 'tar.xz', root='dev',
                       version=self.version, stage='02-cxx', profile='dev', target={'os': self.os, 'arch': self.arch})
        contract = {'schema': 'crt.experimental.native-toolchain', 'format': 1, 'sdk_content_sha256': install['content_sha256'],
                    'producer_source_commit': self.commit, 'policy': 'portable'}
        if mutate_contract:
            mutate_contract(contract)
        (self.assets / self.names['contract']).write_text(json.dumps(contract))
        for name in self.names['sources']:
            (self.assets / name).write_bytes(('source ' + name).encode())
        entries = []
        for name in [self.names['archive'], self.names['contract']] + self.names['sources']:
            data = (self.assets / name).read_bytes()
            entries.append({'name': name, 'size': len(data), 'sha256': digest(data)})
        entries[0]['install'] = install
        entries[0]['contract'] = self.names['contract']
        manifest = {'profile': 'dev', 'version': self.version, 'target': {'os': self.os, 'arch': self.arch}, 'source_commit': self.commit, 'assets': entries}
        if mutate_manifest:
            mutate_manifest(manifest)
        (self.assets / self.names['manifest']).write_text(json.dumps(manifest, indent=2) + '\n')
        (self.assets / self.names['sums']).write_text(''.join('%s  %s\n' % (entry['sha256'], entry['name']) for entry in entries))
        return self

    def verify(self, **kwargs):
        kwargs.setdefault('commit', self.commit)
        return verifier.verify(self.assets, self.os, self.arch, self.version, **kwargs)


class VerifyTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.fixture = Fixture(self.tmp.name)

    def problems(self, **kwargs):
        return list(self.fixture.verify(**kwargs)['problems'])

    def assertFails(self, needle, **kwargs):
        problems = self.problems(**kwargs)
        self.assertTrue(any(needle in item for item in problems), (needle, problems))

    def test_a_faithful_asset_set_is_accepted_and_reports_the_manifest_pin(self):
        self.fixture.build()
        result = self.fixture.verify()
        self.assertEqual(list(result['problems']), [])
        manifest = self.fixture.assets / self.fixture.names['manifest']
        self.assertEqual(result['manifest']['sha256'], digest(manifest.read_bytes()))
        self.assertEqual(result['manifest']['size'], manifest.stat().st_size)
        self.assertEqual(len(result['content_sha256']), 64)
        self.assertEqual(len(result['assets']), 4)

    def test_a_trailing_newline_does_not_make_a_version_or_commit_valid(self):
        self.fixture.build()
        self.assertTrue(verifier.verify(self.fixture.assets, 'linux', 'x86_64', VERSION + '\n', COMMIT)['problems'])
        self.assertTrue(verifier.verify(self.fixture.assets, 'linux', 'x86_64', VERSION, COMMIT + '\n')['problems'])

    def test_a_windows_zip_is_accepted(self):
        with tempfile.TemporaryDirectory() as folder:
            fixture = Fixture(folder, 'windows', 'x86_64').build()
            self.assertEqual(list(fixture.verify()['problems']), [])

    def test_a_changed_archive_byte_fails_every_digest(self):
        self.fixture.build()
        archive = self.fixture.assets / self.fixture.names['archive']
        data = bytearray(archive.read_bytes())
        data[len(data) // 2] ^= 1
        archive.write_bytes(bytes(data))
        self.assertFails('manifest SHA-256 differs')
        self.assertFails('sums file SHA-256 differs')

    def test_a_corrupt_compressed_stream_is_a_problem_not_a_crash(self):
        # Tar headers carry mtimes, so a flipped byte lands in different places on different runs; every placement must be
        # reported, including the ones that break the xz structure itself (this crashed with lzma.LZMAError on a hosted run).
        self.fixture.build()
        archive = self.fixture.assets / self.fixture.names['archive']
        original = archive.read_bytes()
        for position in range(12, len(original) - 12, max(1, len(original) // 40)):
            data = bytearray(original)
            data[position] ^= 0xFF
            archive.write_bytes(bytes(data))
            problems = list(self.fixture.verify()['problems'])
            self.assertTrue(any('manifest SHA-256 differs' in item for item in problems), (position, problems))
        archive.write_bytes(original[:len(original) // 2])
        self.assertFails('manifest size differs')

    def test_a_missing_and_an_extra_file_fail(self):
        self.fixture.build()
        (self.fixture.assets / self.fixture.names['sources'][0]).unlink()
        (self.fixture.assets / 'notes.txt').write_text('extra')
        self.assertFails('missing')
        self.assertFails('unexpected')

    def test_the_wrong_commit_version_and_target_fail(self):
        self.fixture.build()
        self.assertFails('not the expected commit', commit='b' * 40)
        self.assertFails('is not a full lowercase SHA-1', commit='abc')
        with tempfile.TemporaryDirectory() as folder:
            fixture = Fixture(folder).build(mutate_manifest=lambda m: m.update(target={'os': 'linux', 'arch': 'aarch64'}))
            self.assertTrue(any('manifest target' in item for item in fixture.verify()['problems']))

    def test_unsupported_target_and_bad_version_fail_before_any_work(self):
        self.fixture.build()
        self.assertTrue(verifier.verify(self.fixture.assets, 'freebsd', 'x86_64', VERSION)['problems'])
        self.assertTrue(verifier.verify(self.fixture.assets, 'linux', 'x86_64', 'development')['problems'])

    def test_install_metadata_must_be_true_of_the_extracted_tree(self):
        def lie(manifest):
            manifest['assets'][0]['install']['files'] += 1
            manifest['assets'][0]['install']['content_sha256'] = '0' * 64
        self.fixture.build(mutate_manifest=lie)
        self.assertFails('install.files')
        self.assertFails('install.content_sha256')

    def test_the_contract_must_be_portable_and_bound_to_the_content_and_commit(self):
        with tempfile.TemporaryDirectory() as folder:
            fixture = Fixture(folder).build(mutate_contract=lambda c: c.update(external_os_sdk='/opt/sdk'))
            self.assertTrue(any('machine path' in item for item in fixture.verify()['problems']))
        with tempfile.TemporaryDirectory() as folder:
            fixture = Fixture(folder).build(mutate_contract=lambda c: c.update(sdk_content_sha256='1' * 64))
            self.assertTrue(any('content identity' in item for item in fixture.verify()['problems']))
        with tempfile.TemporaryDirectory() as folder:
            fixture = Fixture(folder).build(mutate_contract=lambda c: c.update(producer_source_commit='c' * 40))
            self.assertTrue(any('producer_source_commit' in item for item in fixture.verify()['problems']))
        with tempfile.TemporaryDirectory() as folder:
            fixture = Fixture(folder).build(mutate_contract=lambda c: c.update(note='built in /home/runner/work/crt'))
            problems = verifier.verify(fixture.assets, 'linux', 'x86_64', VERSION, COMMIT, forbidden=['/home/runner/work/crt'])['problems']
            self.assertTrue(any('build-host path' in item for item in problems))

    def test_a_manifest_with_the_wrong_asset_list_fails(self):
        self.fixture.build(mutate_manifest=lambda m: m['assets'].pop())
        self.assertFails('manifest assets are')

    def test_a_symlink_escaping_the_root_is_refused_without_extracting(self):
        self.fixture.build()
        archive = self.fixture.assets / self.fixture.names['archive']
        with tarfile.open(archive, 'w:xz') as bundle:
            info = tarfile.TarInfo('dev')
            info.type = tarfile.DIRTYPE
            bundle.addfile(info)
            link = tarfile.TarInfo('dev/escape')
            link.type, link.linkname = tarfile.SYMTYPE, '../../etc/passwd'
            bundle.addfile(link)
        self.assertFails('points outside the SDK')

    def test_a_member_outside_the_root_or_with_parent_components_is_refused(self):
        self.fixture.build()
        archive = self.fixture.assets / self.fixture.names['archive']
        for name in ('../evil', 'other/file', '/abs'):
            with tarfile.open(archive, 'w:xz') as bundle:
                data = b'x'
                info = tarfile.TarInfo(name)
                info.size = len(data)
                bundle.addfile(info, io.BytesIO(data))
            self.assertFails('unsafe member path' if name != 'other/file' else 'outside the dev/ root')

    def test_a_hard_link_or_device_member_is_refused(self):
        self.fixture.build()
        archive = self.fixture.assets / self.fixture.names['archive']
        with tarfile.open(archive, 'w:xz') as bundle:
            link = tarfile.TarInfo('dev/hard')
            link.type, link.linkname = tarfile.LNKTYPE, 'dev/VERSION'
            bundle.addfile(link)
        self.assertFails('hard link')

    def test_the_version_file_inside_the_archive_must_match(self):
        self.fixture.build()
        (self.fixture.tree / 'VERSION').write_text('v9.9.9\n')
        # rebuild the archive from the changed tree but keep the original release identity
        self.fixture.build()
        self.assertFails('VERSION file differs')


class SumsTest(unittest.TestCase):
    def test_a_malformed_or_duplicated_sums_file_is_reported(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'sums'
            path.write_text('%s  a\n%s  a\nnot a line\n%s  ../b\n' % ('1' * 64, '2' * 64, '3' * 64))
            problems = verifier.Problems()
            entries = verifier.parse_sums(path, problems)
            self.assertEqual(entries, {'a': '1' * 64})
            self.assertEqual(len(problems), 3)


@unittest.skipUnless(os.environ.get('CRT_SDK_VERIFY_REAL_ASSETS'), 'set CRT_SDK_VERIFY_REAL_ASSETS=<dir> to check published bytes')
class RealBytesTest(unittest.TestCase):
    def test_the_published_linux_preview_verifies_and_its_manifest_pin_is_the_documented_one(self):
        result = verifier.verify(os.environ['CRT_SDK_VERIFY_REAL_ASSETS'], 'linux', 'x86_64', 'v0.0.1-preview.20261110.4',
                                 '448fa32349ae697f589f0ad8fc60cfdf7a7e195a')
        self.assertEqual(list(result['problems']), [])
        self.assertEqual(result['manifest']['sha256'], '983dbcf143b1fe227bf519f07ec153ce5baf50e190382222995770affe43b2c3')
        self.assertEqual(result['content_sha256'], '22107183e5127d44e979f328fa78aaf2a436db802aa9f8821a864a39d492ca81')


if __name__ == '__main__':
    unittest.main()
