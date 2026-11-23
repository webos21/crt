#!/usr/bin/env python3
"""Guards the libc++ Windows wide/narrow boundary patches (CRT TODO PAL-06).

crt_win32_wide.h converts this CRT's UTF-32 `wchar_t` paths to and from the UTF-16 every Win32 API uses: a character above U+FFFF is two
UTF-16 units, so a scratch buffer needs narrow_capacity(len), and a count returned by narrow()/widen() must be the length that is used.
A per-element truncation here once made create_directory() of a name with U+1F600 produce a directory ending in U+F600.
"""
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent
RECIPE = json.loads((ROOT / 'libstdc++/third_party/libcxx/recipe.json').read_text(encoding='utf-8'))
PATCHES = RECIPE['patches']
HEADER = next(e for e in RECIPE['extra_files'] if e['path'] == 'src/filesystem/crt_win32_wide.h')['content']


class WideBoundaryPatches(unittest.TestCase):
    def test_every_narrow_call_has_a_worst_case_buffer(self):
        calls = buffers = 0
        for patch in PATCHES:
            text = patch['replace']
            calls += len(re.findall(r'__crt_win32_wide::narrow\(', text))
            buffers += len(re.findall(r'new char16_t\[__crt_win32_wide::narrow_capacity\(', text))
            self.assertIsNone(re.search(r'new char16_t\[[\w.()]+ \+ 1\]', text), 'a narrow() scratch buffer sized len + 1 in ' + patch['file'])
        self.assertGreater(calls, 0)
        self.assertEqual(calls, buffers)

    def test_every_widen_result_is_used_as_the_length(self):
        for patch in PATCHES:
            for line in patch['replace'].splitlines():
                if '__crt_win32_wide::widen(' in line and not line.lstrip().startswith('//'):
                    self.assertRegex(line, r'(size_t \w+ = |= )__crt_win32_wide::widen\(', 'a discarded widen() count in ' + patch['file'] + ': ' + line.strip())

    def test_the_win32_length_is_the_unit_count_not_the_element_count(self):
        path_cpp = next(p['replace'] for p in PATCHES if '__wide_to_char' in p['replace'])
        self.assertIn('static_cast<int>(units)', path_cpp)
        self.assertNotIn('str.size(), out, outlen', path_cpp)

    def test_char_to_wide_returns_the_element_count_for_the_measure_and_convert_calls(self):
        path_cpp = next(p['replace'] for p in PATCHES if '__char_to_wide' in p['replace'])
        self.assertIn('widen(real.get(), static_cast<size_t>(ret), nullptr)', path_cpp)
        self.assertIn('return count;', path_cpp)
        self.assertNotIn('return ret;\n}\n', path_cpp.split('size_t __char_to_wide')[1])

    def test_helper_splits_and_combines_surrogate_pairs_and_reports_counts(self):
        self.assertIn('inline size_t narrow_capacity(size_t len) { return 2 * len + 1; }', HEADER)
        self.assertIn('inline size_t narrow(const wchar_t* src, size_t len, char16_t* dst)', HEADER)
        self.assertIn('inline size_t widen(const char16_t* src, size_t len, wchar_t* dst)', HEADER)
        self.assertIn('0x10000', HEADER)
        self.assertIn('0xD800', HEADER)
        self.assertNotIn('static_cast<wchar_t>(src[i]);\n}', HEADER)   # the old one-for-one widen loop


if __name__ == '__main__':
    unittest.main()
