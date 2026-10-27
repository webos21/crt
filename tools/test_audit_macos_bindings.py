#!/usr/bin/env python3
"""Tests for tools/audit_macos_bindings.py (the pure decision logic; the real nm/otool run is done by verify_dist)."""

import sys
import subprocess
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audit_macos_bindings as audit


def image(name, kind="dylib", bound=None, math=(), owns_malloc=False, deps=()):
    return {"image": name, "kind": kind, "dependencies": list(deps), "owns_malloc": owns_malloc,
            "bound": {k: set(v) for k, v in (bound or {}).items()}, "math_only": set(math)}


class Parse(unittest.TestCase):
    def test_external_consumer_is_audited_against_sdk_symbols(self) -> None:
        def output(*command, **kwargs):
            self.assertTrue(kwargs.get('check'))
            if command[0] == 'otool':
                return 'consumer:\n\t@rpath/libc.dylib (compatibility version 0.0.0)\n'
            if command[0] == 'file':
                return 'Mach-O 64-bit executable arm64'
            if '-u' in command:
                return '                 (undefined) external _strlen (from libc)\n'
            return ''
        with patch.object(audit, 'is_macho', return_value=True), patch.object(audit, 'run', side_effect=output):
            item = audit.collect_image(Path('/external/consumer'), 'consumer-shared', {'_strlen': 'libc'})
        self.assertEqual('exe', item['kind'])
        self.assertEqual({'libc': {'_strlen'}}, item['bound'])
        self.assertFalse(item['owns_malloc'])
        self.assertEqual([], audit.problems([item]))

    def test_failed_image_tool_does_not_produce_empty_success(self) -> None:
        with patch.object(audit, 'is_macho', return_value=True), \
                patch.object(audit, 'run', side_effect=subprocess.CalledProcessError(1, ['otool'])):
            with self.assertRaises(subprocess.CalledProcessError):
                audit.collect_image(Path('/external/consumer'), 'consumer-shared', {'_strlen': 'libc'})

    def test_nm_undefined_lines(self) -> None:
        text = ("                 (undefined) external _malloc (from libc)\n"
                "                 (undefined) external _pthread_mutex_lock (from libSystem)\n"
                "                 (undefined) weak external _objc_x (from libobjc)\n"
                "                 (undefined) external _dyn (dynamically looked up)\n")
        self.assertEqual([("_malloc", "libc"), ("_pthread_mutex_lock", "libSystem"), ("_objc_x", "libobjc")],
                         audit.undefined_bindings(text)[:3])

    def test_defined_symbols_only_exports(self) -> None:
        text = "0000000000001000 T _malloc\n0000000000002000 D _environ\n                 U _printf\n"
        self.assertEqual({"_malloc", "_environ"}, audit.defined_symbols(text))


class Decide(unittest.TestCase):
    def test_binding_to_the_crt_libraries_is_clean(self) -> None:
        self.assertEqual([], audit.problems([image("lib/libx.dylib", bound={"libc": {"_malloc"}, "libm": {"_sin"}})]))

    def test_a_dylib_binding_pthread_to_libsystem_fails(self) -> None:
        found = audit.problems([image("lib/libcrtgfx_gpu.dylib", bound={"libSystem": {"_pthread_mutex_lock", "_malloc"}})])
        self.assertEqual(1, len(found))
        self.assertIn("libcrtgfx_gpu.dylib", found[0])
        self.assertIn("_pthread_mutex_lock", found[0])

    def test_documented_host_boundary_passes_but_a_new_symbol_fails(self) -> None:
        self.assertEqual([], audit.problems([image("lib/libc.dylib", bound={"libSystem": {"_dlsym"}})]))
        found = audit.problems([image("lib/libc.dylib", bound={"libSystem": {"_dlsym", "_pthread_create"}})])
        self.assertEqual(1, len(found))
        self.assertIn("_pthread_create", found[0])
        self.assertNotIn("_dlsym", found[0])

    def test_a_window_backend_allowlist_is_per_symbol(self) -> None:
        allowed = {"_calloc", "_clock_gettime", "_free", "_malloc", "_memcpy", "_realloc"}
        self.assertEqual([], audit.problems([image("lib/libcrtgfx.dylib", bound={"libSystem": allowed})]))
        self.assertTrue(audit.problems([image("lib/libcrtgfx.dylib", bound={"libSystem": allowed | {"_pthread_mutex_init"}})]))

    def test_pure_math_of_an_executable_is_a_note_not_a_failure(self) -> None:
        notes = []
        exe = image("bin/tool", kind="exe", bound={"libSystem": {"_acos", "_sin"}}, math={"_acos", "_sin"})
        self.assertEqual([], audit.problems([exe], notes=notes))
        self.assertEqual(1, len(notes))
        # the same bindings in a dylib are a failure, and a non-math symbol is never tolerated
        self.assertTrue(audit.problems([image("lib/libx.dylib", bound={"libSystem": {"_acos"}}, math={"_acos"})]))
        self.assertTrue(audit.problems([image("bin/tool", kind="exe", bound={"libSystem": {"_acos", "_malloc"}},
                                              math={"_acos"})]))

    def test_static_libc_next_to_crt_dylibs_is_a_mixed_runtime(self) -> None:
        mixed = image("bin/newtool", kind="exe", owns_malloc=True, deps=["@rpath/libz.1.dylib", "/usr/lib/libSystem.B.dylib"])
        found = audit.problems([mixed])
        self.assertEqual(1, len(found))
        self.assertIn("two allocator registries", found[0])
        self.assertEqual([], audit.problems([image("bin/curl", kind="exe", owns_malloc=True, deps=["@rpath/libcurl.4.dylib"])]))
        # a static program, a shared-libc program, or one that only loads libc++ is fine
        self.assertEqual([], audit.problems([image("bin/a", kind="exe", owns_malloc=True, deps=["/usr/lib/libSystem.B.dylib"])]))
        self.assertEqual([], audit.problems([image("bin/b", kind="exe", owns_malloc=False, deps=["@rpath/libc.dylib", "@rpath/libz.1.dylib"])]))
        self.assertEqual([], audit.problems([image("bin/c", kind="exe", owns_malloc=True, deps=["@rpath/libc++.1.dylib"])]))


if __name__ == "__main__":
    unittest.main()
