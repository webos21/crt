#!/usr/bin/env python3
"""Tests for the Tranche 3B source and licensing gates (run from tools/)."""

import copy
import hashlib
import io
import json
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import check_webkit_patch_manifest as checker
import fetch_webkit_commit as commit_source
import scan_webkit_headers as scanner

ROOT = Path(__file__).resolve().parent.parent
APPLE_BSD = b"""/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 */"""
LGPL21 = b"""/* This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version. */"""


class ClassifierTests(unittest.TestCase):
    def test_standard_texts(self):
        self.assertEqual(scanner.classify_header(APPLE_BSD), "BSD-2-Clause")
        self.assertEqual(scanner.classify_header(APPLE_BSD.replace(b"/*", b"/* Neither the name of X may be used to endorse or promote")), "BSD-3-Clause")
        self.assertEqual(scanner.classify_header(LGPL21), "LGPL-2.1+")
        self.assertEqual(scanner.classify_header(b"// SPDX-License-Identifier: LGPL-2.1-or-later\n"), "LGPL-2.1+")
        self.assertEqual(scanner.classify_header(b"# SPDX-License-Identifier: MIT\n"), "MIT")

    def test_unknown_is_never_guessed(self):
        self.assertEqual(scanner.classify_header(b"// Copyright 2024 Somebody\nint x;\n"), "unclassified-with-copyright")
        self.assertEqual(scanner.classify_header(b"int main() { return 0; }\n"), "no-license-header")
        self.assertEqual(scanner.classify_header(b"\x00\x01\x02binary"), "binary")

    def test_gpl_is_flagged_not_lgpl(self):
        gpl = b"This program is free software; you can redistribute it under the terms of the GNU General Public License"
        self.assertEqual(scanner.classify_header(gpl), "GPL")
        self.assertEqual(scanner.classify_header(gpl + b" with a special exception"), "GPL-with-exception")

    def test_components(self):
        self.assertEqual(scanner.component("Source/WTF/wtf/Foo.h"), "WTF")
        self.assertEqual(scanner.component("Source/ThirdParty/skia/a.h"), "ThirdParty/skia")


class RecipeTests(unittest.TestCase):
    def setUp(self):
        self.recipe = json.loads((ROOT / "libcrtweb/third_party/webkit/recipe.json").read_text())

    def test_product_source_is_the_signed_tags_commit(self):
        source = commit_source.product_source(self.recipe)
        self.assertEqual(source["commit"], self.recipe["source"]["expected_commit"])
        self.assertEqual(len(source["tree"]), 40)
        self.assertGreater(source["tree_entries"], 400000)

    def test_a_different_commit_is_refused(self):
        recipe = copy.deepcopy(self.recipe)
        recipe["product_source"]["commit"] = "0" * 40
        with self.assertRaises(SystemExit):
            commit_source.product_source(recipe)

    def test_blob_id_is_git_compatible(self):
        # `printf 'hello\n' | git hash-object --stdin`
        self.assertEqual(commit_source.blob_sha1(b"hello\n"), "ce013625030ba8dba906f756967f9e9ca394464a")

    def test_correspondence_classes(self):
        data = b"line1\nline2\n"
        tree = [("100644", "blob", commit_source.blob_sha1(data), "Source/a.txt"),
                ("100644", "blob", commit_source.blob_sha1(b"other\n"), "Source/b.txt"),
                ("100644", "blob", commit_source.blob_sha1(b"x"), "Source/only_commit.txt")]
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / "a.tar.xz"
            with tarfile.open(archive, "w:xz") as tar:
                for name, content in (("a.txt", data), ("b.txt", b"changed\n"),
                                      ("c.txt", b"new\n")):
                    info = tarfile.TarInfo("root/Source/" + name)
                    info.size = len(content)
                    tar.addfile(info, io.BytesIO(content))
            result = commit_source.correspondence(archive, tree)
        self.assertEqual(result["identical_to_commit"], 1)
        self.assertEqual(result["content_differs_from_commit"], ["Source/b.txt"])
        self.assertEqual(result["only_in_archive"], ["Source/c.txt"])
        self.assertEqual(result["only_in_commit_under_Source"], 1)

    def test_crlf_only_difference_is_recognised(self):
        data = b"a\nb\n"
        tree = [("100644", "blob", commit_source.blob_sha1(data), "x.bat")]
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / "a.tar.xz"
            with tarfile.open(archive, "w:xz") as tar:
                info = tarfile.TarInfo("root/x.bat")
                content = data.replace(b"\n", b"\r\n")
                info.size = len(content)
                tar.addfile(info, io.BytesIO(content))
            result = commit_source.correspondence(archive, tree)
        self.assertEqual(result["same_after_crlf_to_lf"], 1)
        self.assertEqual(result["content_differs_from_commit"], [])


class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads((ROOT / "libcrtweb/patches/manifest.json").read_text())

    def test_the_committed_manifest_passes(self):
        self.assertEqual(checker.check(self.manifest, None, ROOT), [])

    def test_a_file_without_a_license_is_refused(self):
        broken = copy.deepcopy(self.manifest)
        del broken["patches"][0]["files"][0]["license"]
        self.assertTrue(any("no recorded license" in p for p in checker.check(broken, None, ROOT)))

    def test_a_copyleft_class_is_refused(self):
        broken = copy.deepcopy(self.manifest)
        broken["patches"][0]["files"][0]["license"] = "GPL"
        self.assertTrue(any("not one CRT modifies" in p for p in checker.check(broken, None, ROOT)))

    def test_header_less_file_needs_a_basis(self):
        broken = copy.deepcopy(self.manifest)
        entry = broken["patches"][0]["files"][0]
        entry["license"] = "no-license-header"
        self.assertTrue(any("license_basis" in p for p in checker.check(broken, None, ROOT)))

    def test_new_file_must_be_crt_owned_with_the_header(self):
        broken = copy.deepcopy(self.manifest)
        broken["new_files"] = [{"file": "Source/WebKit/crt/Foo.cpp", "origin": "upstream", "license": "LGPL-2",
                                "source": "README.md"}]
        problems = checker.check(broken, None, ROOT)
        self.assertTrue(any("origin" in p for p in problems))
        self.assertTrue(any("license must be" in p for p in problems))

    def test_tree_measurement_catches_a_wrong_class(self):
        with tempfile.TemporaryDirectory() as temp:
            tree = Path(temp)
            for patch in self.manifest["patches"]:
                for entry in patch["files"]:
                    target = tree / entry["file"]
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(LGPL21)
            problems = checker.check(self.manifest, tree, ROOT)
        self.assertTrue(any("header says" in p for p in problems))
        self.assertTrue(any("sha256_before" in p for p in problems))


if __name__ == "__main__":
    unittest.main()
