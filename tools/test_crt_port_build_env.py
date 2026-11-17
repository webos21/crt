"""The loader search path crt-port-build.py gives a port's configure must not let the port prefix shadow the build host's libraries."""
import importlib.util
import os
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location('crt_port_build', Path(__file__).resolve().parent / 'crt-port-build.py')
port_build = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(port_build)

LIB = '/work/sdk/dev/lib'


class RuntimeLibrarySearchTest(unittest.TestCase):
    def test_macos_never_uses_the_variable_that_shadows_system_libraries(self):
        env = {'HOME': '/Users/runner', 'DYLD_LIBRARY_PATH': '/ambient'}
        port_build.apply_runtime_library_search(env, 'macos', LIB)
        self.assertNotIn('DYLD_LIBRARY_PATH', env)
        parts = env['DYLD_FALLBACK_LIBRARY_PATH'].split(os.pathsep)
        self.assertEqual(parts[0], LIB)
        # dyld's own defaults stay reachable, because setting the variable replaces them
        self.assertEqual(parts[1:], ['/Users/runner/lib', '/usr/local/lib', '/usr/lib'])

    def test_an_existing_fallback_list_is_kept_without_re_adding_defaults_and_without_duplicates(self):
        env = {'DYLD_FALLBACK_LIBRARY_PATH': os.pathsep.join(['/opt/x', LIB])}
        port_build.apply_runtime_library_search(env, 'macos', LIB)
        self.assertEqual(env['DYLD_FALLBACK_LIBRARY_PATH'].split(os.pathsep), [LIB, '/opt/x'])
        port_build.apply_runtime_library_search(env, 'macos', LIB)
        self.assertEqual(env['DYLD_FALLBACK_LIBRARY_PATH'].split(os.pathsep), [LIB, '/opt/x'])

    def test_macos_without_a_home_adds_only_the_absolute_defaults(self):
        env = {}
        port_build.apply_runtime_library_search(env, 'macos', LIB)
        self.assertEqual(env['DYLD_FALLBACK_LIBRARY_PATH'].split(os.pathsep), [LIB, '/usr/local/lib', '/usr/lib'])

    def test_linux_keeps_its_ld_library_path_in_front(self):
        env = {'LD_LIBRARY_PATH': '/ambient'}
        port_build.apply_runtime_library_search(env, 'linux', LIB)
        self.assertEqual(env['LD_LIBRARY_PATH'], LIB + os.pathsep + '/ambient')
        self.assertEqual(port_build.apply_runtime_library_search({}, 'linux', LIB)['LD_LIBRARY_PATH'], LIB)

    def test_windows_is_untouched(self):
        env = {'PATH': 'x'}
        port_build.apply_runtime_library_search(env, 'windows', LIB)
        self.assertEqual(env, {'PATH': 'x'})


if __name__ == '__main__':
    unittest.main()
