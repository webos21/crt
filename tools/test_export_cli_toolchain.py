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
        captured = ['-isystem' + str(sdk) + '/include', '-isystem' + resource + '/include',
                    '-Wl,-force_symbols_not_weak_list,' + str(sdk) + '/lib/libc++.notweak.exp',
                    str(sdk) + '/lib/libc++.a', '@OBJECTS@', '@LIBRARIES@', '@SOURCE@.cpp', '-o', '@OUTPUT@']
        self.assertEqual(['-isystem@SDK@/include', '-isystem@RESOURCE@/include',
                          '-Wl,-force_symbols_not_weak_list,@SDK@/lib/libc++.notweak.exp',
                          '@SDK@/lib/libc++.a', '@OBJECTS@', '@LIBRARIES@', '@SOURCE@', '-o', '@OUTPUT@'],
                         exporter.normalize_argv(captured, sdk, resource, 'cpp'))

    def test_linux_shared_executables_find_a_project_library_beside_them(self):
        link = ['-nostdlib', '-Wl,-rpath,@SDK@/lib', '@OBJECTS@', '@LIBRARIES@', '-o', '@OUTPUT@']
        fixed = exporter.project_local_shared_rpath(link, 'executable-shared', 'link', 'linux')
        self.assertEqual(['-nostdlib', '-Wl,-rpath,@SDK@/lib', '-Wl,-rpath,$ORIGIN', '@OBJECTS@', '@LIBRARIES@',
                          '-o', '@OUTPUT@'], fixed)
        # idempotent, and the C vector that has no rpath at all gets the entry too
        self.assertEqual(fixed, exporter.project_local_shared_rpath(fixed, 'executable-shared', 'link', 'linux'))
        self.assertIn('-Wl,-rpath,$ORIGIN', exporter.project_local_shared_rpath(
            ['@OBJECTS@', '@LIBRARIES@'], 'executable-shared', 'link', 'linux'))

    def test_other_modes_operations_and_hosts_are_unchanged(self):
        link = ['-Wl,-rpath,@SDK@/lib', '@OBJECTS@', '@LIBRARIES@']
        for mode, operation, target_os in [('executable-static', 'link', 'linux'), ('static-library', 'link', 'linux'),
                                           ('shared-library', 'link', 'linux'), ('executable-shared', 'compile', 'linux'),
                                           ('executable-shared', 'link', 'macos')]:
            with self.subTest(mode=mode, operation=operation, target_os=target_os):
                self.assertEqual(link, exporter.project_local_shared_rpath(link, mode, operation, target_os))

    def test_wrapper_temporary_files_are_not_replayable(self):
        sdk = Path('/handoff/SDK/dev')
        stale = ['-Wl,--no-as-needed', '/tmp/crt-loader-needed-x86_64.93Tr2x/loader.so', '-Wl,--as-needed']
        self.assertEqual(['/tmp/crt-loader-needed-x86_64.93Tr2x/loader.so'], exporter.unreplayable_paths(stale, sdk))
        shipped = ['-Wl,--no-as-needed', '@SDK@/lib/crt-loader-import.so', '@OBJECTS@', '-L@SDK@/lib', '-o', '@OUTPUT@']
        self.assertEqual([], exporter.unreplayable_paths(shipped, sdk))
        for temporary in ('/var/folders/ab/cd/T/x.o', '-Wl,-rpath,/private/tmp/y', '-I/tmp/inc', '--out=/var/tmp/o'):
            with self.subTest(temporary=temporary):
                self.assertTrue(exporter.unreplayable_paths([temporary], sdk))


if __name__ == '__main__':
    unittest.main()
