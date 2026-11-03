"""Portable launch-accounting tests; actual Windows token/link proof is native."""
import os
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parent))
from replay_host_environment_windows import allowed_image


class ExactImageAccounting(unittest.TestCase):
    def test_matching_basename_outside_selected_bundle_is_rejected(self):
        selected = Path('selected/system/bin/mksh.exe').resolve()
        with patch.dict(os.environ, {'SystemRoot': str(Path('windows').resolve())}):
            self.assertTrue(allowed_image(str(selected), {selected}))
            self.assertFalse(allowed_image(str(Path('ambient/mksh.exe').resolve()), {selected}))
            self.assertFalse(allowed_image(None, {selected}))

    def test_only_os_console_host_is_an_additional_permitted_image(self):
        system = Path('windows').resolve()
        with patch.dict(os.environ, {'SystemRoot': str(system)}):
            self.assertTrue(allowed_image(str(system / 'System32/conhost.exe'), set()))
            self.assertFalse(allowed_image(str(system / 'System32/cmd.exe'), set()))
            self.assertFalse(allowed_image(str(Path('other/conhost.exe').resolve()), set()))


if __name__ == '__main__':
    unittest.main()
