"""Portable argument fixtures; native restricted-token proof is separate."""
from pathlib import Path
from types import SimpleNamespace
import sys
import unittest

sys.path.insert(0, str(Path(__file__).parent))
from tools.test_cli_fetch_https_windows import allowed_image, case_arguments


class SelectedTrustArguments(unittest.TestCase):
    def test_https_accounts_exact_cli_path_and_rejects_unknown_or_same_basename(self):
        selected, system = Path('private/bare/crt-cli.exe'), Path('windows')
        self.assertTrue(allowed_image(str(selected), selected, system))
        self.assertTrue(allowed_image(str(system / 'System32/conhost.exe'), selected, system))
        for image in (None, 'ambient/crt-cli.exe', 'ambient/conhost.exe', 'windows/System32/cmd.exe'):
            self.assertFalse(allowed_image(image, selected, system))

    def test_selected_package_trust_and_host_only_issuer_are_forwarded(self):
        args = SimpleNamespace(cli=Path('selected/crt-cli.exe'), publisher_cases=True,
                               test_ca_directory=Path('issuer space'),
                               trusted_ca_file=Path('package space/share/trust/ca.pem'))
        argv = case_arguments(args, Path('evidence'), Path('private work'))
        self.assertEqual(argv[argv.index('--trusted-ca-file') + 1], str(args.trusted_ca_file))
        self.assertEqual(argv[argv.index('--test-ca-directory') + 1], str(args.test_ca_directory))
        self.assertIn('--publisher-cases', argv)
        self.assertEqual(argv[argv.index('--work') + 1], str(Path('private work/cases-work')))

    def test_existing_generated_ca_mode_keeps_no_selected_trust_flags(self):
        args = SimpleNamespace(cli=Path('crt-cli.exe'), publisher_cases=False,
                               test_ca_directory=None, trusted_ca_file=None)
        argv = case_arguments(args, Path('evidence'), Path('work'))
        for flag in ('--publisher-cases', '--test-ca-directory', '--trusted-ca-file'):
            self.assertNotIn(flag, argv)


if __name__ == '__main__':
    unittest.main()
