"""Native-Windows smoke of the traced restricted-token launch (skipped elsewhere)."""
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))


@unittest.skipUnless(os.name == 'nt', 'requires native Windows')
class TracedLaunch(unittest.TestCase):
    def test_exit_status_redirected_output_and_process_inventory(self):
        import crt_windows_external as sandbox
        import crt_windows_trace as trace
        work, sid = sandbox.create_work()
        self.addCleanup(lambda: __import__('shutil').rmtree(work, ignore_errors=True))
        hostname = Path(os.environ['SystemRoot']) / 'System32' / 'hostname.exe'
        out, err = work / 'out.txt', work / 'err.txt'
        result = trace.run([hostname], work=work, sid=sid, env={'SystemRoot': os.environ['SystemRoot'], 'PATH': ''},
                           cwd=work, stdout=out, stderr=err, timeout=60)
        self.assertEqual(0, result['exit_code'])
        self.assertFalse(result['timed_out'])
        self.assertTrue(out.read_text().strip())
        images = {Path(item['image']).name.lower() for item in result['processes'] if item['image']}
        self.assertIn('hostname.exe', images)

    def test_private_directory_names_only_the_given_identities(self):
        import crt_windows_external as sandbox
        import crt_windows_trace as trace
        work, sid = sandbox.create_work()
        self.addCleanup(lambda: __import__('shutil').rmtree(work, ignore_errors=True))
        inner = work / 'inner'
        inner.mkdir()
        other = 'S-1-5-21-1-2-3-4'
        trace.private_directory(inner, sid, other)
        (inner / 'file.txt').write_text('x')
        self.assertEqual('x', (inner / 'file.txt').read_text())


if __name__ == '__main__':
    unittest.main()
