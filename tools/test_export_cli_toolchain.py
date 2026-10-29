"""Native-provider target/argv fixtures, not consumer runtime acceptance."""
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import export_cli_toolchain as exporter


class NativePolicy(unittest.TestCase):
    def test_only_explicit_native_pairs_are_supported(self):
        self.assertEqual({'os': 'linux', 'arch': 'x86_64'}, exporter.native_target('Linux', 'x86_64'))
        self.assertEqual({'os': 'macos', 'arch': 'aarch64'}, exporter.native_target('Darwin', 'arm64'))
        for system, arch in [('Darwin', 'x86_64'), ('Linux', 'aarch64'), ('Windows', 'AMD64')]:
            with self.subTest(system=system, arch=arch), self.assertRaises(ValueError):
                exporter.native_target(system, arch)

    def test_sdk_paths_spaces_and_link_order_remain_typed_arguments(self):
        sdk = Path('/handoff/SDK space 한글/dev')
        resource = '/compiler/resource'
        captured = ['-isystem' + str(sdk / 'include'), '-isystem' + resource + '/include',
                    '-Wl,-force_symbols_not_weak_list,' + str(sdk / 'lib/libc++.notweak.exp'),
                    str(sdk / 'lib/libc++.a'), '@OBJECTS@', '@LIBRARIES@', '@SOURCE@.cpp', '-o', '@OUTPUT@']
        self.assertEqual(['-isystem@SDK@/include', '-isystem@RESOURCE@/include',
                          '-Wl,-force_symbols_not_weak_list,@SDK@/lib/libc++.notweak.exp',
                          '@SDK@/lib/libc++.a', '@OBJECTS@', '@LIBRARIES@', '@SOURCE@', '-o', '@OUTPUT@'],
                         exporter.normalize_argv(captured, sdk, resource, 'cpp'))


if __name__ == '__main__':
    unittest.main()
