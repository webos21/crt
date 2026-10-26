"""Fresh Linux Ninja graph: native Wayland outputs must exist as build rules."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


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
