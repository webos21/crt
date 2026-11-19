from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).parent))
import host_tools_replay_common as common


class ReplayCommon(unittest.TestCase):
    def test_loader_failures_signals_and_fatal_statuses_count_as_crashes(self):
        for code in (126, 127, -11, -6, 0xC0000005, 0xC0000135):
            self.assertTrue(common.crashed(code), code)
        for code in (0, 1, 2, 3, 124, 255):
            self.assertFalse(common.crashed(code), code)

    def test_cases_do_not_depend_on_path_or_absolute_paths(self):
        # The replay empties PATH, so a shell command line may only use builtins; paths are relative to the scratch directory.
        for _label, tool, argv, _code, _needle in common.CASES + common.FILE_CASES:
            text = ' '.join(argv)
            if tool == 'sh':
                self.assertNotIn('printf', text.split('-c')[-1] if '-c' in argv else text)
            self.assertNotIn(':\\', text)

    def test_run_cases_reports_each_case_and_the_alias_start_check(self):
        calls, results = [], []
        manifest = {'executable_files': ['system/bin/sh', 'bin/sh', 'usr/bin/cat', 'system/etc/mkshrc']}
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in manifest['executable_files']:
                (root / name).parent.mkdir(parents=True, exist_ok=True)
                (root / name).write_text('x')

            def launch(tool, argv):
                calls.append((Path(tool).name, tuple(argv)))
                tool_name = Path(tool).name
                table = {c[1]: c for c in common.CASES + common.FILE_CASES}
                if '--help' in argv:
                    return 0, ''
                case = next(c for c in common.CASES + common.FILE_CASES if c[1] == tool_name and list(c[2]) == list(argv))
                return case[3], case[4]
            common.run_cases(root, manifest, launch, lambda label, passed, detail='': results.append((label, passed)))
        self.assertTrue(all(passed for _label, passed in results), [r for r in results if not r[1]])
        self.assertIn(('sh', ('--help',)), calls)          # an alias under system/bin starts
        self.assertEqual(1, len([c for c in calls if c == ('cat', ('--help',))]))   # an alias under usr/bin starts
        self.assertEqual(len(common.CASES) + len(common.FILE_CASES) + 2, len(results))  # + existence + alias-start

    def test_a_crashing_alias_fails_the_start_check(self):
        import tempfile
        manifest = {'executable_files': ['system/bin/sh']}
        results = []
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); (root / 'system/bin').mkdir(parents=True); (root / 'system/bin/sh').write_text('x')
            common.run_cases(root, manifest, lambda tool, argv: (0xC0000005, 'boom') if argv == ['--help'] else (0, ''),
                             lambda label, passed, detail='': results.append((label, passed)))
        self.assertFalse(dict(results)['every executable alias starts (no loader failure, signal or fatal status)'])


class MacProfile(unittest.TestCase):
    def test_the_seatbelt_profile_is_balanced_and_denies_network_and_foreign_writes(self):
        import replay_host_tools_macos as mac
        text = mac.seatbelt_profile('/tmp/some "scratch" dir')
        depth = 0
        quoted = False
        escaped = False
        for ch in text:
            if quoted:
                if escaped:
                    escaped = False
                elif ch == '\\':
                    escaped = True
                elif ch == '"':
                    quoted = False
                continue
            if ch == '"':
                quoted = True
            elif ch == '(':
                depth += 1
            elif ch == ')':
                depth -= 1
                self.assertGreaterEqual(depth, 0, text)
        self.assertEqual(0, depth, text)
        self.assertIn('(deny network*)', text)
        self.assertIn('(deny file-write* (require-not (require-any (subpath "/dev") (subpath ', text)


if __name__ == '__main__':
    unittest.main()
