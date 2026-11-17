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
WORKFLOWS = sorted((ROOT / '.github/workflows').glob('sdk-*.yml'))
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
            used = USES.findall(text)
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


if __name__ == '__main__':
    unittest.main()
