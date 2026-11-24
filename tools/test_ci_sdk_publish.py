"""SDK release naming, approval prerequisites, and draft staging against a fake gh."""
import hashlib
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
    import hashlib, json, os, shutil, sys
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
        print(json.dumps({'databaseId': 30, 'isDraft': meta['draft'], 'isPrerelease': meta['prerelease'], 'targetCommitish': meta['target'], 'tagName': tag,
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
        (folder / 'meta.json').write_text(json.dumps({'draft': '--draft' in argv, 'prerelease': '--prerelease' in argv,
            'target': argv[i + 1], 'title': argv[argv.index('--title') + 1], 'assets': names}))
        if os.environ.get('FAKE_GH_CORRUPT'):
            (folder / names[0]).write_bytes(b'corrupted in transit')
    elif argv[:2] == ['release', 'download']:
        folder = state / 'releases' / argv[2]
        out = Path(argv[argv.index('--dir') + 1])
        out.mkdir(parents=True, exist_ok=True)
        for f in folder.iterdir():
            if f.name not in ('meta.json', 'NOTES.md'):
                shutil.copy(f, out / f.name)
    elif argv[0] == 'api':
        endpoint = argv[-1]
        if endpoint.endswith('/environments/release'):
            print(json.dumps({'protection_rules': [{'type': 'required_reviewers', 'reviewers': [{}]}]}))
        elif '/git/matching-refs/' in endpoint:
            print('[]')
        else:
            folder, = (state / 'releases').iterdir()
            meta = json.loads((folder / 'meta.json').read_text())
            if endpoint.endswith('/releases/30'):
                print(json.dumps({'id': 30, 'tag_name': folder.name, 'draft': meta['draft'], 'prerelease': meta['prerelease'],
                    'target_commitish': meta['target'], 'name': meta['title'], 'body': (folder / 'NOTES.md').read_text()}))
            elif endpoint.endswith('/releases/30/assets?per_page=100'):
                print(json.dumps([[{'id': i, 'name': name, 'size': (folder / name).stat().st_size, 'state': 'uploaded',
                    'digest': 'sha256:' + hashlib.sha256((folder / name).read_bytes()).hexdigest()}
                    for i, name in enumerate(meta['assets'], 1)]]))
            else:
                sys.exit(2)
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


class SmokePolicyTest(unittest.TestCase):
    """The consumer smoke is by a pinned, published CLI, except for exactly one stated, unpinned first host."""
    PINS = {'linux-x86_64': {}, 'linux-aarch64': {}, 'macos-aarch64': {}, 'windows-x86_64': {}}
    NOTES = 'Preview.' + chr(10) + publish.BOOTSTRAP_MARKER + chr(10)
    # No host is marked `bootstrap` now; the rule stays for a future first host, so it is exercised on a copy of a real host entry.
    FIRST = dict(publish.HOSTS['windows-arm64'], bootstrap=True)
    UNPINNED = {'linux-x86_64': {}, 'linux-aarch64': {}, 'macos-aarch64': {}, 'windows-x86_64': {}}

    def policy(self, host, pins=None, notes=''):
        return publish.smoke_policy(host, publish.HOSTS[host], self.PINS if pins is None else pins, notes)

    def test_a_pinned_host_uses_the_published_cli_whatever_the_notes_say(self):
        pins = dict(self.PINS, **{'windows-aarch64': {}})
        for host in ('linux-x64', 'linux-arm64', 'macos-arm64', 'windows-x64', 'windows-arm64'):
            self.assertEqual('pinned', self.policy(host, pins=pins), host)
            self.assertEqual('pinned', self.policy(host, pins=pins, notes=self.NOTES), host)

    def test_the_first_host_runs_without_a_cli_only_when_the_notes_say_so(self):
        self.assertEqual('none', publish.smoke_policy('windows-arm64', self.FIRST, self.UNPINNED, self.NOTES))
        for notes in ('', 'Consumer smoke: passed', publish.BOOTSTRAP_MARKER.replace('NOT RUN', 'passed'), publish.BOOTSTRAP_MARKER.lower()):
            with self.assertRaises(publish.ReleaseError, msg=notes):
                publish.smoke_policy('windows-arm64', self.FIRST, self.UNPINNED, notes)

    def test_the_exception_ends_when_the_host_gains_a_pin(self):
        pins = dict(self.PINS, **{'windows-aarch64': {}})
        with self.assertRaises(publish.ReleaseError):
            publish.smoke_policy('windows-arm64', self.FIRST, pins, self.NOTES)
        self.assertEqual('pinned', publish.smoke_policy('windows-arm64', publish.HOSTS['windows-arm64'], pins, ''))

    def test_a_host_that_is_not_marked_bootstrap_can_never_skip_the_cli(self):
        for host in ('linux-x64', 'linux-arm64', 'macos-arm64', 'windows-x64', 'windows-arm64'):
            pins = {key: value for key, value in dict(self.PINS, **{'windows-aarch64': {}}).items()
                    if key != '%s-%s' % (publish.HOSTS[host]['os'], publish.HOSTS[host]['arch'])}
            with self.assertRaises(publish.ReleaseError, msg=host):
                self.policy(host, pins=pins, notes=self.NOTES)

    def test_no_host_is_marked_bootstrap_any_more_and_the_pins_file_has_every_host(self):
        self.assertEqual([], [host for host, info in publish.HOSTS.items() if info.get('bootstrap')])
        pins = publish.load_pins_hosts()
        for host, info in publish.HOSTS.items():
            self.assertIn('%s-%s' % (info['os'], info['arch']), pins, host)

    def test_the_windows_hosts_name_their_toolchain_inputs_and_the_others_do_not_need_them(self):
        for host, info in publish.HOSTS.items():
            self.assertEqual(info['os'] == 'windows', bool(info.get('msvc_arch') and info.get('llvm_asset')), host)


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
        self.env = {'PATH': str(bin_dir) + os.pathsep + os.environ['PATH'], 'FAKE_GH_STATE': str(self.state),
                    'GITHUB_RUN_ID': '123', 'GITHUB_RUN_ATTEMPT': '1'}
        for key, value in self.env.items():
            self.addCleanup(os.environ.__setitem__, key, os.environ[key]) if key in os.environ else self.addCleanup(os.environ.pop, key, None)
            os.environ[key] = value
        self.notes = root / 'notes.md'
        self.notes.write_text((Path(__file__).resolve().parent.parent / 'docs/releases/sdk_release_notes_template.md').read_text(encoding='utf-8'), encoding='utf-8')
        self.counter = 0
        self.receipt = root / 'release-stage.json'

    def args(self, **extra):
        self.counter += 1
        values = dict(host='linux-x64', commit=COMMIT, date='20261118', serial='1', repo='o/r', release_dir=str(self.release), notes_file='docs/releases/x.md',
                      work=str(Path(self.tmp.name) / ('work%d' % self.counter)), dry_run=False, receipt=self.receipt)
        values.update(extra)
        return type('Args', (), values)()

    def run_stage(self, **extra):
        # the notes path rule is checked on the repository-relative name; the fake runs from a temp file, so patch only the lookup
        original = publish.require_notes_path
        publish.require_notes_path = lambda path: self.notes
        self.addCleanup(setattr, publish, 'require_notes_path', original)
        return publish.command_stage(self.args(**extra))

    def meta(self):
        return json.loads((self.state / 'releases' / self.info['tag'] / 'meta.json').read_text())

    def test_dry_run_stages_a_draft_without_a_receipt(self):
        self.run_stage(dry_run=True)
        meta = self.meta()
        self.assertTrue(meta['draft'])
        self.assertTrue(meta['prerelease'])
        self.assertEqual(meta['target'], COMMIT)
        self.assertEqual(sorted(meta['assets']), sorted(p.name for p in self.release.iterdir()))
        notes = (self.state / 'releases' / self.info['tag'] / 'NOTES.md').read_text()
        self.assertNotIn('{{', notes)
        self.assertIn(self.info['tag'], notes)
        self.assertFalse(self.receipt.exists())

    def test_real_staging_leaves_a_draft_and_pins_its_entire_inventory(self):
        self.run_stage()
        self.assertTrue(self.meta()['draft'])
        self.assertTrue(self.meta()['prerelease'])
        receipt = json.loads(self.receipt.read_text())
        self.assertEqual(set(receipt['draft']['assets']), set(self.meta()['assets']))
        self.assertEqual(receipt['run_id'], 123)
        self.assertEqual(receipt['draft']['assets'][self.fixture.names['sums']]['sha256'],
                         hashlib.sha256((self.release / self.fixture.names['sums']).read_bytes()).hexdigest())
        calls = (self.state / 'calls.log').read_text()
        self.assertIn('--latest=false', calls)
        self.assertEqual(calls.count('release create'), 1)
        self.assertNotIn('release edit', calls)

    def test_an_existing_release_is_never_overwritten(self):
        self.run_stage(dry_run=True)
        with self.assertRaises(publish.ReleaseError):
            self.run_stage(dry_run=True)

    def test_corrupted_bytes_are_refused_before_anything_is_uploaded(self):
        archive = self.release / self.fixture.names['archive']
        archive.write_bytes(archive.read_bytes() + b'x')
        with self.assertRaises(publish.ReleaseError):
            self.run_stage()
        self.assertFalse((self.state / 'releases' / self.info['tag']).exists())

    def test_a_commit_that_differs_from_the_manifest_is_refused(self):
        with self.assertRaises(publish.ReleaseError):
            self.run_stage(commit='c' * 40)

    def test_a_draft_that_differs_from_the_verified_files_is_never_published(self):
        os.environ['FAKE_GH_CORRUPT'] = '1'
        self.addCleanup(os.environ.pop, 'FAKE_GH_CORRUPT', None)
        with self.assertRaises(publish.release_signing.SigningError):
            self.run_stage()
        self.assertTrue(self.meta()['draft'])
        self.assertFalse(self.receipt.exists())


if __name__ == '__main__':
    unittest.main()
