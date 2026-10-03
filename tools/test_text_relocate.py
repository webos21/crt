#!/usr/bin/env python3
"""Tests for tools/crt_text_relocate.py and verify_dist.validate_text_paths()."""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from crt_text_relocate import relocate_text_prefixes
from verify_dist import validate_text_paths

CURL_CONFIG = """#!/bin/sh
prefix='{old}'
# Used in 'libdir'
exec_prefix="${{prefix}}"
includedir="${{prefix}}/include"
case "$1" in
  --cc)
    echo '{old}/tools/crt-cc'
    ;;
  --prefix)
    echo "$prefix"
    ;;
  --static-libs)
    echo "${{exec_prefix}}/lib/libcurl.a -L{old}/lib -lz"
    ;;
  --configure)
    echo " '--with-mbedtls={old}' '--prefix={old}'"
    ;;
esac
"""

LIBCURL_PC = """prefix={old}
exec_prefix=${{prefix}}
libdir=${{exec_prefix}}/lib
includedir=${{prefix}}/include
Name: libcurl
Libs: -L${{libdir}} -lcurl
Libs.private: -L{old}/lib -lmbedtls
Cflags: -I${{includedir}}
"""

FREETYPE_PC = """prefix={old}
exec_prefix={old}
libdir={old}/lib
Name: FreeType 2
Libs: -L{old}/lib -lfreetype
Cflags: -I{old}/include/freetype2
"""

LIBCURL_LA = """# libcurl.la - a libtool library file
dlname=''
libdir='{old}/lib'
"""


def populate(root: Path, old: str) -> None:
    for relative, template in (("bin/curl-config", CURL_CONFIG),
                               ("lib/pkgconfig/libcurl.pc", LIBCURL_PC),
                               ("lib/pkgconfig/freetype2.pc", FREETYPE_PC),
                               ("lib/libcurl.la", LIBCURL_LA)):
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(template.format(old=old), encoding="utf-8")
    os.chmod(root / "bin/curl-config", 0o755)
    (root / "lib").joinpath("libcurl.a").write_bytes(b"!<arch>\n\0\0binary " + old.encode())


class TextRelocateTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = Path(tempfile.mkdtemp(prefix="crt text reloc "))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.staged = self.tmp / "crt-stage-04-abc" / "sdk"
        self.staged.mkdir(parents=True)
        populate(self.staged, str(self.staged))

    def test_unrelocated_tree_fails_verification(self) -> None:
        with self.assertRaises(SystemExit) as caught:
            validate_text_paths(self.staged)
        message = str(caught.exception)
        for needle in ("libcurl.la", "libcurl.pc", "freetype2.pc", "curl-config"):
            self.assertIn(needle, message)

    def test_relocation_makes_tree_pass_and_removes_the_old_path(self) -> None:
        leftover = relocate_text_prefixes(self.staged, [self.staged])
        self.assertEqual(leftover, [])
        validate_text_paths(self.staged)
        self.assertFalse((self.staged / "lib/libcurl.la").exists())
        for path in self.staged.rglob("*"):
            if path.is_file() and path.suffix != ".a":
                self.assertNotIn("crt-stage-04-abc", path.read_text(encoding="utf-8"), path)
        self.assertEqual(
            (self.staged / "lib/pkgconfig/libcurl.pc").read_text(encoding="utf-8").splitlines()[0],
            "prefix=${pcfiledir}/../..")
        self.assertIn("-L${prefix}/lib -lmbedtls",
                      (self.staged / "lib/pkgconfig/libcurl.pc").read_text(encoding="utf-8"))
        self.assertIn("exec_prefix=${prefix}",
                      (self.staged / "lib/pkgconfig/freetype2.pc").read_text(encoding="utf-8"))
        # Binary files are never touched.
        self.assertIn(b"crt-stage-04-abc", (self.staged / "lib/libcurl.a").read_bytes())
        self.assertTrue(os.access(self.staged / "bin/curl-config", os.X_OK))

    @unittest.skipIf(sys.platform == "win32", "needs a POSIX sh")
    def test_curl_config_follows_the_script_after_the_tree_moves(self) -> None:
        relocate_text_prefixes(self.staged, [self.staged])
        moved = self.tmp / "final location" / "sdk"
        moved.parent.mkdir()
        shutil.move(str(self.staged), str(moved))
        script = str(moved / "bin/curl-config")
        for option, expected in (("--prefix", str(moved.resolve())),
                                 ("--cc", str(moved.resolve()) + "/tools/crt-cc"),
                                 ("--static-libs", None),
                                 ("--configure", None)):
            result = subprocess.run(["sh", script, option], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            output = result.stdout.strip()
            self.assertNotIn("crt-stage-04-abc", output)
            if expected is not None:
                self.assertEqual(os.path.realpath(output), os.path.realpath(expected))
            else:
                self.assertIn(os.path.realpath(moved).replace("/private", ""),
                              output.replace("/private", ""))

    def test_unknown_text_file_is_reported_not_guessed(self) -> None:
        note = self.staged / "share" / "notes.txt"
        note.parent.mkdir()
        note.write_text(f"built in {self.staged}\n", encoding="utf-8")
        leftover = relocate_text_prefixes(self.staged, [self.staged])
        self.assertEqual(leftover, [note])
        self.assertIn(str(self.staged), note.read_text(encoding="utf-8"))

    def test_real_symlinked_spelling_is_covered(self) -> None:
        link = self.tmp / "alias"
        try:
            link.symlink_to(self.staged.parent, target_is_directory=True)
        except (OSError, NotImplementedError):
            self.skipTest("symlinks unavailable")
        (self.staged / "lib/pkgconfig/zlib.pc").write_text(
            f"prefix={link / 'sdk'}\nlibdir=${{prefix}}/lib\n", encoding="utf-8")
        leftover = relocate_text_prefixes(self.staged, [link / "sdk"])
        self.assertEqual(leftover, [])
        self.assertTrue((self.staged / "lib/pkgconfig/zlib.pc").read_text(
            encoding="utf-8").startswith("prefix=${pcfiledir}/../.."))


if __name__ == "__main__":
    unittest.main()
