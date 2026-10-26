"""Fresh Linux Ninja graph: native Wayland outputs must exist as build rules."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import test_crtgfx_wayland_smoke as smoke


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'needs CMake/Ninja')
class WaylandTargetScope(unittest.TestCase):
    def test_standalone_smoke_opts_its_shadow_tree_into_port_targets(self):
        with tempfile.TemporaryDirectory(prefix='crt-wayland-smoke-') as raw:
            for os_name in ('linux', 'macos', 'windows'):
                with self.subTest(os=os_name), patch.object(sys, 'argv', [
                        'smoke', '--root', str(ROOT), '--build-dir', raw,
                        '--cmake-c-compiler', 'clang', '--cmake-cxx-compiler', 'clang++',
                        '--target-os', os_name, '--target-arch', 'host']):
                    # Stop at the configure boundary: foreign execution is not
                    # valid on this host; inspect the actual driver's first call.
                    with patch.object(smoke, 'run', side_effect=RuntimeError('configure captured')) as run:
                        with self.assertRaisesRegex(RuntimeError, 'configure captured'):
                            smoke.main()
                        self.assertIn('-DCRTGFX_ENABLE_WAYLAND_PORT_TARGETS=ON', run.call_args.args[0])

    def test_actual_port_target_registration_and_cross_scanner_forwarding(self):
        # Exercise the production registration module without compiling foreign
        # runtime objects or pretending a host has a foreign platform SDK.
        targets = {'crtgfx-wayland-configure', 'crtgfx-wayland-build',
                   'crtgfx-wayland-smoke', 'crtgfx-xkbcommon-configure',
                   'crtgfx-xkbcommon-build'}
        with tempfile.TemporaryDirectory(prefix='crt-wayland-scope-') as raw:
            root = Path(raw)
            module = ROOT / 'libcrtgfx/cmake/crtgfx_wayland_port_targets.cmake'
            (root / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.20)\nproject(Scope NONE)\n'
                'add_custom_target(port-build-expat)\nadd_custom_target(port-build-libffi)\n'
                'set(Python3_EXECUTABLE "python3")\n'
                'set(CRTGFX_WAYLAND_HOST_SCANNER "${CMAKE_BINARY_DIR}/native-scanner")\n'
                'set(CRTGFX_WAYLAND_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/wayland")\n'
                'set(CRTGFX_WAYLAND_XDG_SHELL_LIBRARY "${CMAKE_BINARY_DIR}/wayland/lib/libxdg-shell-protocol.a")\n'
                'set(CRTGFX_XKBCOMMON_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/xkbcommon")\n'
                f'include("{module.as_posix()}")\n')
            for os_name, arch, opt_in in [('linux', 'x86_64', False),
                    ('linux', 'aarch64', False), ('macos', 'aarch64', False),
                    ('windows', 'x86_64', False), ('macos', 'aarch64', True),
                    ('windows', 'x86_64', True)]:
                with self.subTest(os=os_name, arch=arch, opt_in=opt_in):
                    build = root / f'{os_name}-{arch}-{opt_in}'
                    result = subprocess.run(['cmake', '-S', str(root), '-B', str(build),
                        '-G', 'Ninja', f'-DCRT_TARGET_OS={os_name}',
                        f'-DCRT_EFFECTIVE_ARCH_FOR_FLAGS={arch}',
                        *(['-DCRTGFX_ENABLE_WAYLAND_PORT_TARGETS=ON'] if opt_in else [])],
                        capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    inventory = subprocess.check_output(['ninja', '-C', str(build),
                        '-t', 'targets', 'all'], text=True, timeout=30)
                    names = {line.split(':', 1)[0] for line in inventory.splitlines()}
                    self.assertEqual(targets & names, targets if os_name == 'linux' or opt_in else set())
                    if os_name == 'linux' or opt_in:
                        commands = subprocess.check_output(['ninja', '-C', str(build), '-t',
                            'commands', 'crtgfx-wayland-build'], text=True, timeout=30)
                        commands = commands.replace('\\', '/').replace('"', '')
                        self.assertIn(f'--target-os {os_name}', commands)
                        self.assertIn(f'--target-arch {arch}', commands)
                        self.assertIn(f'--host-scanner {build.as_posix()}/native-scanner', commands)


@unittest.skipUnless(sys.platform.startswith('linux') and all(shutil.which(p) for p in
                    ('cmake', 'ninja', 'clang', 'clang++')), 'needs native Linux build tools')
class WaylandBuildGraph(unittest.TestCase):
    def test_clean_window_and_dist_include_generated_protocol_without_reconfigure(self):
        with tempfile.TemporaryDirectory(prefix='crt-wayland-graph-') as raw:
            build = Path(raw)
            configured = subprocess.run(['cmake', '--preset', 'linux-host-ninja-debug', '-B', str(build)],
                                        cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(configured.returncode, 0, configured.stdout + configured.stderr)
            cache = (build / 'CMakeCache.txt').read_text()
            if any(f'{name}:FILEPATH={name}-NOTFOUND' in cache for name in
                   ('CRTGFX_LINUX_WAYLAND_CLIENT_LIB', 'CRTGFX_LINUX_VULKAN_LIB')):
                self.skipTest('native Vulkan/Wayland development libraries unavailable')
            protocol = build / 'external/wayland/install/lib/libxdg-shell-protocol.a'
            self.assertFalse(protocol.exists())
            query = subprocess.run(['ninja', '-C', str(build), '-t', 'query', str(protocol)],
                                   capture_output=True, text=True, timeout=30)
            self.assertEqual(query.returncode, 0, query.stderr)
            self.assertIn('CUSTOM_COMMAND', query.stdout)
            commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands',
                                                'crt-gfx-simple-dist'], text=True, timeout=30)
            self.assertTrue('window_wayland_native.c' in commands, 'native backend absent from fresh graph')
            self.assertTrue('CRTGFX_HAVE_NATIVE_WAYLAND=1' in commands, 'native WSI disabled before build')
            self.assertTrue('--wayland-xdg-shell-install' in commands, 'publisher omits generated protocol')
            dry_run = subprocess.run(['ninja', '-C', str(build), '-n', 'crtgfx_window_demo'],
                                     capture_output=True, text=True, timeout=30)
            self.assertEqual(dry_run.returncode, 0, dry_run.stdout + dry_run.stderr)
            self.assertFalse((build / 'external').exists(), 'graph checks must not fetch/build')


if __name__ == '__main__':
    unittest.main()
