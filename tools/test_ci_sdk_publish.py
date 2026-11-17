"""Tests of tools/ci_sdk_publish.py: naming, notes rendering, the approval-gate check, and the draft -> compare -> publish flow against a fake `gh`."""
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
import textwrap
import unittest

import ci_sdk_publish as publish
from test_ci_sdk_release import Fixture

COMMIT = 'b' * 40
FAKE_GH = textwrap.dedent('''\
    #!/usr/bin/env python3
    import json, os, shutil, sys
    from pathlib import Path
    state = Path(os.environ['FAKE_GH_STATE'])
    argv = sys.argv[1:]
    log = state / 'calls.log'
    with open(log, 'a') as handle:
        handle.write(' '.join(argv) + '\\n')
    if argv[:2] == ['release', 'view']:
        tag = argv[2]
        folder = state / 'releases' / tag
        if not (folder / 'meta.json').exists():
            sys.exit(1)
        meta = json.loads((folder / 'meta.json').read_text())
        print(json.dumps({'isDraft': meta['draft'], 'isPrerelease': meta['prerelease'], 'targetCommitish': meta['target'], 'tagName': tag,
                          'url': 'https://example.invalid/' + tag, 'assets': [{'name': n} for n in meta['assets']]}))
    elif argv[:2] == ['release', 'create']:
        tag, folder = argv[2], state / 'releases' / argv[2]
        folder.mkdir(parents=True)
        i = argv.index('--target')
        notes = Path(argv[argv.index('--notes-file') + 1])
        files = [a for a in argv[i + 2:] if Path(a).is_file() and a != str(notes)]
        names = []
        for f in files:
            if Path(f) == notes:
                continue
            shutil.copy(f, folder / Path(f).name)
            names.append(Path(f).name)
        shutil.copy(notes, folder / 'NOTES.md')
        (folder / 'meta.json').write_text(json.dumps({'draft': '--draft' in argv, 'prerelease': '--prerelease' in argv, 'target': argv[i + 1], 'assets': names}))
        if os.environ.get('FAKE_GH_CORRUPT'):
            (folder / names[0]).write_bytes(b'corrupted in transit')
    elif argv[:2] == ['release', 'download']:
        folder = state / 'releases' / argv[2]
        out = Path(argv[argv.index('--dir') + 1])
        out.mkdir(parents=True, exist_ok=True)
        for f in folder.iterdir():
            if f.name not in ('meta.json', 'NOTES.md'):
                shutil.copy(f, out / f.name)
    elif argv[:2] == ['release', 'edit']:
        meta_path = state / 'releases' / argv[2] / 'meta.json'
        meta = json.loads(meta_path.read_text())
        meta['draft'] = '--draft=false' not in argv
        meta_path.write_text(json.dumps(meta))
    else:
        sys.exit(2)
''')


class NamingTest(unittest.TestCase):
    def test_the_tag_is_a_valid_release_version_that_names_the_host(self):
        import ci_sdk_release
        for host in publish.HOSTS:
            info = publish.names(host, '20261118', '3')
            self.assertRegex(info['tag'], ci_sdk_release.VERSION_RE.pattern)
            self.assertTrue(info['tag'].startswith('v0.0.1-preview.20261118.'))
            self.assertTrue(info['tag'].endswith('.3'))
        self.assertEqual(publish.names('linux-x64', '20261118', '1')['tag'], 'v0.0.1-preview.20261118.linux.x64.1')

    def test_bad_requests_are_refused(self):
        for host, date, serial in (('linux-riscv', '20261118', '1'), ('linux-x64', '2026-11-18', '1'), ('linux-x64', '20261399', '1'),
                                   ('linux-x64', '20261118', '0'), ('linux-x64', '20261118', '01'), ('linux-x64', '20261118', '1.2')):
            with self.assertRaises(publish.ReleaseError, msg=(host, date, serial)):
                publish.names(host, date, serial)

    def test_only_a_full_lowercase_commit_is_accepted(self):
        publish.require_commit('a' * 40)
        for bad in ('main', 'v1.0.0', 'a' * 39, 'A' * 40, 'a' * 40 + '\n', ''):
            with self.assertRaises(publish.ReleaseError, msg=bad):
                publish.require_commit(bad)

    def test_notes_must_be_a_reviewed_path_under_docs_releases(self):
        publish.require_notes_path('docs/releases/release_notes_v0.0.1-preview.20261118.linux.x64.1.md')
        for bad in ('../x.md', '/etc/passwd', 'docs/releases/../../x.md', 'docs/releases/a b.md', 'notes.md', 'docs/releases/x.txt', ''):
            with self.assertRaises(publish.ReleaseError, msg=bad):
                publish.require_notes_path(bad)


class GatesTest(unittest.TestCase):
    def test_ci_must_have_a_completed_success(self):
        self.assertFalse(publish.ci_succeeded({'workflow_runs': []}))
        self.assertFalse(publish.ci_succeeded({'workflow_runs': [{'status': 'completed', 'conclusion': 'failure'}, {'status': 'in_progress', 'conclusion': None}]}))
        self.assertTrue(publish.ci_succeeded({'workflow_runs': [{'status': 'completed', 'conclusion': 'failure'}, {'status': 'completed', 'conclusion': 'success'}]}))

    def test_the_release_environment_must_require_reviewers(self):
        self.assertFalse(publish.has_required_reviewers({'protection_rules': []}))
        self.assertFalse(publish.has_required_reviewers({'protection_rules': [{'type': 'wait_timer'}]}))
        self.assertFalse(publish.has_required_reviewers({'protection_rules': [{'type': 'required_reviewers', 'reviewers': []}]}))
        self.assertTrue(publish.has_required_reviewers({'protection_rules': [{'type': 'required_reviewers', 'reviewers': [{'type': 'User'}]}]}))


class CiGateTest(unittest.TestCase):
    OK = {'workflow_runs': [{'status': 'completed', 'conclusion': 'success'}]}
    NONE = {'workflow_runs': []}
    BAD = {'workflow_runs': [{'status': 'completed', 'conclusion': 'failure'}]}

    def gate(self, runs, files, parents):
        return publish.ci_gate('c3', lambda sha: runs[sha], lambda sha: files[sha], lambda sha: parents[sha])

    def test_a_commit_with_a_successful_run_is_its_own_gate(self):
        self.assertEqual(self.gate({'c3': self.OK}, {}, {}), 'c3')

    def test_a_docs_only_release_commit_is_covered_by_the_nearest_tested_ancestor(self):
        runs = {'c3': self.NONE, 'c2': self.NONE, 'c1': self.OK}
        files = {'c3': ['docs/releases/release_notes_x.md'], 'c2': ['README.md', 'docs/guides/a.md']}
        self.assertEqual(self.gate(runs, files, {'c3': 'c2', 'c2': 'c1'}), 'c1')

    def test_a_skipped_commit_that_changed_code_or_recipes_is_refused(self):
        for changed in (['tools/ci_sdk_publish.py'], ['porting/recipes/zlib.json'], ['docs/a.md', 'libc/x.c'], []):
            with self.assertRaises(publish.ReleaseError, msg=changed):
                self.gate({'c3': self.NONE}, {'c3': changed}, {'c3': 'c2'})

    def test_a_failed_or_unfinished_run_is_refused_not_skipped(self):
        for runs in (self.BAD, {'workflow_runs': [{'status': 'in_progress', 'conclusion': None}]}):
            with self.assertRaises(publish.ReleaseError):
                self.gate({'c3': runs}, {}, {})

    def test_the_walk_is_bounded(self):
        with self.assertRaises(publish.ReleaseError):
            publish.ci_gate('c0', lambda sha: self.NONE, lambda sha: ['docs/a.md'], lambda sha: sha + 'x', depth=5)


class NotesTest(unittest.TestCase):
    ARGS = dict(tag='v0.0.1-preview.20261118.linux.x64.1', commit=COMMIT, manifest={'sha256': 'c' * 64, 'size': 1716}, content_sha256='d' * 64,
                assets=[{'name': 'b.tar.xz', 'size': 2, 'sha256': 'e' * 64}, {'name': 'a.json', 'size': 1, 'sha256': 'f' * 64}], target='linux/x86_64')
    TEMPLATE = 'Release {{TAG}} from {{COMMIT}} for {{TARGET}}\npin {{MANIFEST_SHA256}} {{MANIFEST_BYTES}}\ncontent {{CONTENT_SHA256}}\n{{ASSET_TABLE}}\n'

    def test_tokens_are_filled_and_the_table_is_sorted(self):
        text = publish.render_notes(self.TEMPLATE, **self.ARGS)
        self.assertIn('pin %s 1716' % ('c' * 64), text)
        self.assertIn('content ' + 'd' * 64, text)
        self.assertLess(text.index('a.json'), text.index('b.tar.xz'))
        self.assertNotIn('{{', text)

    def test_a_template_without_the_pins_or_with_a_stray_token_is_refused(self):
        for template in ('no tokens', self.TEMPLATE.replace('{{MANIFEST_SHA256}}', ''), self.TEMPLATE.replace('{{CONTENT_SHA256}}', ''), self.TEMPLATE.replace('{{ASSET_TABLE}}', ''),
                         self.TEMPLATE + '{{SURPRISE}}'):
            with self.assertRaises(publish.ReleaseError, msg=template):
                publish.render_notes(template, **self.ARGS)

    def test_the_committed_template_renders(self):
        template = (Path(__file__).resolve().parent.parent / 'docs/releases/sdk_release_notes_template.md').read_text(encoding='utf-8')
        self.assertNotIn('{{', publish.render_notes(template, **self.ARGS))


@unittest.skipIf(sys.platform == 'win32', 'the fake gh is a POSIX script')
class PublishFlowTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        root = Path(self.tmp.name)
        self.info = publish.names('linux-x64', '20261118', '1')
        self.fixture = Fixture(root / 'fx', 'linux', 'x86_64', version=self.info['tag'], commit=COMMIT).build()
        self.release = self.fixture.assets
        self.state = root / 'state'
        (self.state / 'releases').mkdir(parents=True)
        bin_dir = root / 'bin'
        bin_dir.mkdir()
        gh = bin_dir / 'gh'
        gh.write_text(FAKE_GH)
        gh.chmod(gh.stat().st_mode | stat.S_IXUSR)
        self.env = {'PATH': str(bin_dir) + os.pathsep + os.environ['PATH'], 'FAKE_GH_STATE': str(self.state)}
        for key, value in self.env.items():
            self.addCleanup(os.environ.__setitem__, key, os.environ[key]) if key in os.environ else self.addCleanup(os.environ.pop, key, None)
            os.environ[key] = value
        self.notes = root / 'notes.md'
        self.notes.write_text((Path(__file__).resolve().parent.parent / 'docs/releases/sdk_release_notes_template.md').read_text(encoding='utf-8'), encoding='utf-8')
        self.counter = 0

    def args(self, **extra):
        self.counter += 1
        values = dict(host='linux-x64', commit=COMMIT, date='20261118', serial='1', repo='o/r', release_dir=str(self.release), notes_file='docs/releases/x.md',
                      work=str(Path(self.tmp.name) / ('work%d' % self.counter)), draft_only=False)
        values.update(extra)
        return type('Args', (), values)()

    def run_publish(self, **extra):
        # the notes path rule is checked on the repository-relative name; the fake runs from a temp file, so patch only the lookup
        original = publish.require_notes_path
        publish.require_notes_path = lambda path: self.notes
        self.addCleanup(setattr, publish, 'require_notes_path', original)
        return publish.command_publish(self.args(**extra))

    def meta(self):
        return json.loads((self.state / 'releases' / self.info['tag'] / 'meta.json').read_text())

    def test_draft_only_stages_and_compares_but_leaves_a_draft(self):
        self.run_publish(draft_only=True)
        meta = self.meta()
        self.assertTrue(meta['draft'])
        self.assertTrue(meta['prerelease'])
        self.assertEqual(meta['target'], COMMIT)
        self.assertEqual(sorted(meta['assets']), sorted(p.name for p in self.release.iterdir()))
        notes = (self.state / 'releases' / self.info['tag'] / 'NOTES.md').read_text()
        self.assertNotIn('{{', notes)
        self.assertIn(self.info['tag'], notes)

    def test_a_full_run_ends_as_a_non_draft_prerelease_that_is_not_latest(self):
        self.run_publish()
        self.assertFalse(self.meta()['draft'])
        self.assertTrue(self.meta()['prerelease'])
        calls = (self.state / 'calls.log').read_text()
        self.assertIn('--latest=false', calls)
        self.assertEqual(calls.count('release create'), 1)

    def test_an_existing_release_is_never_overwritten(self):
        self.run_publish(draft_only=True)
        with self.assertRaises(publish.ReleaseError):
            self.run_publish(draft_only=True)

    def test_corrupted_bytes_are_refused_before_anything_is_uploaded(self):
        archive = self.release / self.fixture.names['archive']
        archive.write_bytes(archive.read_bytes() + b'x')
        with self.assertRaises(publish.ReleaseError):
            self.run_publish()
        self.assertFalse((self.state / 'releases' / self.info['tag']).exists())

    def test_a_commit_that_differs_from_the_manifest_is_refused(self):
        with self.assertRaises(publish.ReleaseError):
            self.run_publish(commit='c' * 40)

    def test_a_draft_that_differs_from_the_verified_files_is_never_published(self):
        os.environ['FAKE_GH_CORRUPT'] = '1'
        self.addCleanup(os.environ.pop, 'FAKE_GH_CORRUPT', None)
        with self.assertRaises(publish.ReleaseError):
            self.run_publish()
        self.assertTrue(self.meta()['draft'])


if __name__ == '__main__':
    unittest.main()
