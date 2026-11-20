"""PE closure policy fixtures; these synthetic images are never executed."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parent))
import create_host_tools as producer
import crt_host_tools_pe as pe


class PeClosureTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        for name in ('system/bin', 'bin', 'usr/bin'): (self.root / name).mkdir(parents=True)
        self.host = {'os': 'windows', 'arch': 'x86_64'}
        self.tools = {'awk': 'system/bin/awk.exe'}
        self.aliases = [{'path': 'bin/awk.exe', 'kind': 'file', 'role': 'awk'}]
        self.dependencies = {'system/bin/awk.exe': ['libfixture.dll', 'KERNEL32.dll'],
                             'system/bin/libfixture.dll': ['KERNEL32.dll']}
        self.image('system/bin/awk.exe'); self.image('bin/awk.exe')
        self.image('system/bin/libfixture.dll', dll=True)
        pe.copy_alias_runtimes(self.root, self.aliases, self.tools, self.dependencies, ['KERNEL32.dll'])

    def image(self, name, dll=False, machine=0x8664):
        data = bytearray(128); data[:2] = b'MZ'; struct.pack_into('<I', data, 60, 64)
        data[64:68] = b'PE\0\0'; struct.pack_into('<H', data, 68, machine)
        struct.pack_into('<H', data, 86, 0x2000 if dll else 0)
        (self.root / name).write_bytes(data)

    def manifest(self):
        return {'host': self.host, 'tools': self.tools, 'aliases': self.aliases,
                'executable_files': ['bin/awk.exe', 'system/bin/awk.exe'], 'dependencies': self.dependencies,
                'external_prerequisites': [{'components': ['KERNEL32.dll']}], 'inventory': producer.payload_inventory(self.root)}

    def verify(self, manifest=None):
        with patch.object(pe, 'imported_libraries', side_effect=lambda path:
                          ['KERNEL32.dll'] if path.suffix == '.dll' else ['libfixture.dll', 'KERNEL32.dll']):
            return pe.verify_closure(self.root, manifest or self.manifest(), producer.native_image)

    def test_pe_imports_are_flat_dll_names_and_os_names_are_case_insensitive(self):
        self.assertIsNone(pe.private_dll_name('kernel32.DLL', ['KERNEL32.dll']))
        self.assertEqual(pe.private_dll_name('libfixture.DLL', ['KERNEL32.dll']), 'libfixture.DLL')
        for name in ('../lib.dll', '/lib.dll', 'C:lib.dll', 'C:\\lib.dll', 'dir\\lib.dll',
                     'lib.dll ', 'NUL.dll', 'COM1.dll', 'lib.exe', 'lib.dll\0x'):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'non-flat'):
                pe.private_dll_name(name, ['KERNEL32.dll'])

    def test_aliases_receive_real_byte_identical_app_local_copies(self):
        path = self.root / 'bin/libfixture.dll'
        self.assertTrue(path.is_file()); self.assertFalse(path.is_symlink())
        self.assertEqual(path.read_bytes(), (self.root / 'system/bin/libfixture.dll').read_bytes())
        pe.copy_alias_runtimes(self.root, self.aliases, self.tools, self.dependencies, ['KERNEL32.dll'])
        self.verify()

    def test_conflicting_alias_dll_is_not_overwritten(self):
        path = self.root / 'bin/libfixture.dll'; path.write_bytes(b'collision')
        with self.assertRaisesRegex(ValueError, 'conflicting'):
            pe.copy_alias_runtimes(self.root, self.aliases, self.tools, self.dependencies, ['KERNEL32.dll'])
        self.assertEqual(path.read_bytes(), b'collision')

    def test_missing_adjacent_copy_fails_even_with_rehashed_inventory(self):
        (self.root / 'bin/libfixture.dll').unlink()
        with self.assertRaisesRegex(ValueError, 'missing beside'):
            self.verify()

    def test_changed_adjacent_copy_fails_even_with_rehashed_inventory(self):
        (self.root / 'bin/libfixture.dll').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'differs from canonical'):
            self.verify()

    def test_wrong_machine_and_exe_disguised_as_dll_fail(self):
        self.image('system/bin/libfixture.dll', dll=True, machine=0xaa64)
        with self.assertRaisesRegex(ValueError, 'non-native'):
            self.verify()
        self.image('system/bin/libfixture.dll', dll=False)
        with self.assertRaisesRegex(ValueError, 'not a DLL'):
            self.verify()

    def test_dependency_graph_is_rederived(self):
        manifest = self.manifest(); manifest['dependencies'] = {}
        with self.assertRaisesRegex(ValueError, 'graph mismatch'):
            self.verify(manifest)

    def test_os_runtime_shadow_is_rejected(self):
        self.image('system/bin/KERNEL32.dll', dll=True)
        with self.assertRaisesRegex(ValueError, 'OS runtime'):
            self.verify()

    def test_alias_dispatcher_must_match_primary(self):
        self.image('bin/awk.exe', machine=0xaa64)
        with self.assertRaisesRegex(ValueError, 'differs from its primary'):
            self.verify()


if __name__ == '__main__':
    unittest.main()
