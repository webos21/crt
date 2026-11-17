"""Static rules for the SDK CI/CD workflows (`.github/workflows/sdk-*.yml`). Stdlib only: the files are checked as text.

These do not run a workflow; they stop the mistakes that a hosted run only reveals after an hour: a moving action tag, a write
permission on a workflow that must publish nothing, a script the workflow names but the repository does not have, or a trigger
path that no longer matches anything.
"""
import fnmatch
import glob
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parent.parent
WORKFLOWS = sorted((ROOT / '.github/workflows').glob('sdk-*.yml')) + sorted((ROOT / '.github/actions').glob('sdk-*/action.yml'))
PIN = re.compile(r'^\s*(?:-\s+)?uses:\s*([^\s@]+)@([0-9a-f]{40})\s+#\s*v\S+', re.M)
USES = re.compile(r'^\s*(?:-\s+)?uses:\s*(\S+)', re.M)


def tracked_files():
    done = subprocess.run(['git', '-C', str(ROOT), 'ls-files', '--cached', '--others', '--exclude-standard'], capture_output=True, text=True, check=True)
    return done.stdout.splitlines()


class WorkflowRulesTest(unittest.TestCase):
    def test_there_is_a_workflow_to_check(self):
        self.assertTrue(WORKFLOWS, 'no .github/workflows/sdk-*.yml')

    def test_every_action_is_pinned_to_a_full_commit_with_its_version_noted(self):
        for path in WORKFLOWS:
            text = path.read_text(encoding='utf-8')
            used = [name for name in USES.findall(text) if not name.startswith('./')]  # a local action is the commit's own file
            pinned = [name for name, _ in PIN.findall(text)]
            self.assertEqual(sorted(used and [item.split('@')[0] for item in used]), sorted(pinned), '%s: an action is not pinned to a 40-hex commit with a "# vX" note' % path.name)

    def test_the_integration_workflow_can_only_read_the_repository(self):
        text = (ROOT / '.github/workflows/sdk-integration.yml').read_text(encoding='utf-8')
        self.assertRegex(text, r'(?m)^permissions:\n  contents: read\n')
        self.assertNotRegex(text, r'(?m)^\s+(contents|packages|id-token|attestations):\s*write')
        self.assertNotIn('gh release', text)
        self.assertNotIn('secrets.', text)

    def test_the_ci_tag_is_a_valid_release_tag_that_is_not_a_preview_name(self):
        text = (ROOT / '.github/workflows/sdk-integration.yml').read_text(encoding='utf-8')
        match = re.search(r'SDK_CI_TAG:\s*(v0\.0\.1-ci\.\$\{\{ github\.run_id \}\})', text)
        self.assertIsNotNone(match)
        sample = match.group(1).replace('${{ github.run_id }}', '123456789')
        self.assertRegex(sample, r'^v[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z][0-9A-Za-z.]*)?$')
        self.assertNotIn('preview', sample)

    def test_scripts_named_in_run_steps_exist(self):
        for path in WORKFLOWS:
            for script in sorted(set(re.findall(r'\btools/([A-Za-z0-9_./-]+\.py)', path.read_text(encoding='utf-8')))):
                self.assertTrue((ROOT / 'tools' / script).is_file(), '%s names tools/%s which does not exist' % (path.name, script))

    def test_every_push_trigger_path_matches_a_tracked_or_new_file(self):
        text = (ROOT / '.github/workflows/sdk-integration.yml').read_text(encoding='utf-8')
        block = re.search(r'(?ms)^    paths:\n(.*?)^\S', text)
        self.assertIsNotNone(block)
        patterns = re.findall(r"^      - '([^']+)'", block.group(1), re.M)
        self.assertTrue(patterns)
        files = tracked_files()
        for pattern in patterns:
            # GitHub filter patterns treat + ? [ ] ! as operators; a literal one must be escaped (an unescaped `+` made the
            # whole workflow unparseable: "push event contained invalid paths patterns: libstdc++/**").
            self.assertNotRegex(re.sub(r'\\.', '', pattern), r'[+?\[\]!]', 'unescaped filter operator in %r' % pattern)
            regex = re.compile(fnmatch.translate(pattern.replace('\\', '').replace('**', '*')))
            self.assertTrue(any(regex.match(name) for name in files), 'trigger path %r matches no file' % pattern)

    def test_the_unit_job_runs_the_tests_it_names(self):
        text = (ROOT / '.github/workflows/sdk-integration.yml').read_text(encoding='utf-8')
        for name in re.findall(r'-p "(test_[a-z_]+\.py)"', text):
            self.assertTrue((ROOT / 'tools' / name).is_file(), name)


class ReleaseWorkflowRulesTest(unittest.TestCase):
    """sdk-release.yml is the only workflow that may publish: read-only by default, one write job behind an approval environment."""
    TEXT = (ROOT / '.github/workflows/sdk-release.yml').read_text(encoding='utf-8')

    def jobs(self):
        block = self.TEXT[self.TEXT.index('\njobs:\n'):]
        return {name: body for name, body in re.findall(r'(?ms)^  ([a-z]+):\n(.*?)(?=^  [a-z]+:\n|\Z)', block)}

    def test_it_is_manual_only_and_defaults_to_a_dry_run(self):
        on = self.TEXT[self.TEXT.index('\non:\n'):self.TEXT.index('\npermissions:')]
        self.assertIn('workflow_dispatch:', on)
        for trigger in ('push:', 'pull_request', 'schedule:', 'workflow_run'):
            self.assertNotIn(trigger, on)
        self.assertRegex(on, r'(?s)dry_run:.*?default: true')

    def test_write_permission_exists_only_in_the_publish_job_behind_the_release_environment(self):
        self.assertRegex(self.TEXT, r'(?m)^permissions:\n  contents: read\n')
        jobs = self.jobs()
        self.assertEqual(sorted(jobs), ['package', 'plan', 'publish', 'verify'])
        for name, body in jobs.items():
            if name == 'publish':
                self.assertRegex(body, r'(?m)^    environment: release$')
                self.assertRegex(body, r'(?m)^      contents: write$')
                self.assertEqual(len(re.findall(r'write', re.sub(r'#.*', '', body))), 1)
            else:
                self.assertNotRegex(body, r'(?m):\s*write\b', name)
                self.assertNotIn('environment:', body)
        self.assertEqual(re.sub(r'(?m)#.*', '', self.TEXT).count('contents: write'), 1)

    def test_publish_needs_the_verified_bytes_and_never_builds(self):
        publish = self.jobs()['publish']
        self.assertRegex(publish, r'needs: \[plan, verify\]')
        for forbidden in ('cmake', 'crt-dev-release', 'apt-get'):
            self.assertNotIn(forbidden, publish)

    def test_inputs_reach_scripts_only_through_the_environment(self):
        for name, body in self.jobs().items():
            for run in re.findall(r'(?ms)^        run: [|>]-?\n(.*?)(?=^      - |\Z)', body):
                self.assertNotRegex(run, r'\$\{\{\s*(inputs|github\.event)\.', '%s interpolates an input into a script' % name)

    def test_every_job_works_on_the_requested_commit_and_writes_nothing_to_the_checkout_credentials(self):
        for name, body in self.jobs().items():
            for checkout in re.findall(r'(?s)actions/checkout@.*?(?=\n      - |\Z)', body):
                self.assertIn('ref: ${{ inputs.commit }}', checkout, name)
                self.assertIn('persist-credentials: false', checkout, name)

    def test_the_toolchain_pins_match_the_integration_workflow(self):
        integration = (ROOT / '.github/workflows/sdk-integration.yml').read_text(encoding='utf-8')
        action = (ROOT / '.github/actions/sdk-host-tools/action.yml').read_text(encoding='utf-8')
        for literal in ('llvmorg-22.1.8', 'clang-21 lld-21 llvm-21 libclang-rt-21-dev ninja-build cmake libc++-21-dev libc++abi-21-dev',
                        'ilammy/msvc-dev-cmd@0b201ec74fa43914dc39ae48a89fd1d8cb592756'):
            self.assertIn(literal, integration)
            self.assertIn(literal, action)

    def test_the_build_flags_match_the_integration_workflow(self):
        for flag in ('-DCMAKE_BUILD_TYPE=Release', '-DCRTGFX_ENABLE_SKIA=OFF', '-DCRTMEDIA_ENABLE_FFMPEG=OFF', '-DCRTMEDIA_ENABLE_CURL=OFF', '-DCRTUI_ENABLE_LVGL=OFF', '--target crt-dev-release'):
            self.assertIn(flag, self.TEXT)
            self.assertIn(flag, (ROOT / '.github/workflows/sdk-integration.yml').read_text(encoding='utf-8'))


class MatrixHostRulesTest(unittest.TestCase):
    """Every matrix host either has a pinned published CLI Preview for the consumer smoke or says, explicitly, that it has none."""
    TEXT = (ROOT / '.github/workflows/sdk-integration.yml').read_text(encoding='utf-8')

    def rows(self):
        block = re.search(r'(?ms)^        include:\n(.*?)^    steps:', self.TEXT)
        self.assertIsNotNone(block, 'no matrix include list')
        rows = {}
        for row in re.split(r'(?m)^          - host: ', block.group(1))[1:]:
            fields = dict(re.findall(r'^\s*([a-z_]+):\s*(\S+)\s*$', 'host: ' + row, re.M))
            rows[fields['host']] = fields
        return rows

    def test_a_host_without_a_pin_declares_cli_preview_none_and_a_pinned_host_does_not(self):
        import json
        pinned = set(json.loads((ROOT / '.github/ci/sdk-pins.json').read_text(encoding='utf-8'))['hosts'])
        rows = self.rows()
        self.assertTrue(rows)
        for host, fields in rows.items():
            if host in pinned:
                self.assertNotIn('cli_preview', fields, '%s has a pin, so its consumer smoke must run' % host)
            else:
                self.assertEqual(fields.get('cli_preview'), 'none', '%s has no sdk-pins.json entry and does not say so' % host)

    def test_a_pin_without_a_matrix_host_is_stale(self):
        import json
        pinned = set(json.loads((ROOT / '.github/ci/sdk-pins.json').read_text(encoding='utf-8'))['hosts'])
        self.assertLessEqual(pinned, set(self.rows()))

    def test_every_matrix_host_names_its_toolchain_inputs(self):
        for host, fields in self.rows().items():
            for key in ('runner', 'preset', 'os', 'arch'):
                self.assertIn(key, fields, '%s lacks %s' % (host, key))
            if fields['os'] == 'windows':
                for key in ('msvc_arch', 'llvm_asset'):
                    self.assertIn(key, fields, '%s lacks %s' % (host, key))
        self.assertIn('windows-aarch64', self.rows())

    def test_the_not_run_step_exists_and_the_smoke_step_is_conditional(self):
        self.assertRegex(self.TEXT, r"(?m)^        if: matrix\.cli_preview == 'none'$")
        self.assertRegex(self.TEXT, r"(?m)^        if: matrix\.cli_preview != 'none'$")


if __name__ == '__main__':
    unittest.main()
