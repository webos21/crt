#!/usr/bin/env python3
"""Tests for tools/crt_runtime_notices.py (inherited-runtime notices, Tranche 10)."""

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import crt_runtime_notices as notices
import verify_dist

BSD = """/*
 * Copyright (c) 2001 Example Author
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met.
 */
"""


class HeaderTests(unittest.TestCase):
    def test_rcs_id_comment_does_not_hide_the_license_comment(self) -> None:
        text = "/*\t$OpenBSD: strlen.c,v 1.7 2005/08/08 espie Exp $\t*/\n\n" + BSD + "int x;\n"
        self.assertIn("Redistribution and use", notices.header_of(text))

    def test_project_file_without_a_license_comment_has_no_header(self) -> None:
        self.assertEqual("", notices.header_of("#include <stdio.h>\nint main(void) { return 0; }\n"))
        self.assertEqual("", notices.header_of("/* just a description of foo */\nint x;\n"))

    def test_descriptive_comment_mentioning_permissions_is_not_a_license_header(self) -> None:
        text = "/* Executable-memory permissions on Apple Silicon: a per-thread toggle. */\nint x;\n"
        self.assertEqual("", notices.header_of(text))
        self.assertNotEqual("", notices.header_of("/* Permission to use, copy, modify, and distribute this software. */\n"))
        self.assertNotEqual("", notices.header_of("/* Permission is hereby granted, free of charge. */\n"))

    def test_normalise_ignores_holders_and_years(self) -> None:
        other = BSD.replace("2001 Example Author", "2017 Somebody Else")
        self.assertEqual(notices.normalise(BSD), notices.normalise(other))
        self.assertNotEqual(notices.normalise(BSD), notices.normalise(BSD.replace("with or without", "only with")))


class ScanTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / "src").mkdir()
        (self.root / "src" / "a.c").write_text(BSD + "int a;\n")
        (self.root / "src" / "b.c").write_text(BSD.replace("2001 Example Author", "2009 Other") + "int b;\n")
        (self.root / "src" / "own.c").write_text("int own;\n")
        (self.root / "src" / "skip.txt").write_text(BSD)

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def test_groups_by_license_text_and_lists_holders(self) -> None:
        scan = notices.scan_headers(self.root, {"roots": ["src"], "extensions": [".c"]})
        self.assertEqual(1, len(scan["groups"]))
        self.assertEqual(["src/a.c", "src/b.c"], scan["groups"][0]["files"])
        self.assertEqual({"Copyright (c) 2001 Example Author", "Copyright (c) 2009 Other"},
                         scan["groups"][0]["copyrights"])
        self.assertEqual(["src/own.c"], scan["headerless"])

    def test_missing_scan_root_is_an_error(self) -> None:
        with self.assertRaises(ValueError):
            notices.scan_headers(self.root, {"roots": ["nope"], "extensions": [".c"]})

    def test_notice_text_mentions_every_file_and_the_default_license(self) -> None:
        scan = notices.scan_headers(self.root, {"roots": ["src"], "extensions": [".c"]})
        text = notices.render_notice({"name": "demo"}, scan)
        for name in ("src/a.c", "src/b.c", "LICENSE.md", "Redistribution and use"):
            self.assertIn(name, text)

    def test_import_manifest_cross_check_reports_unlicensed_imports(self) -> None:
        bionic = self.root / "third_party" / "bionic"
        bionic.mkdir(parents=True)
        entries = [
            {"local": "src/a.c", "source_kind": "bionic-main", "upstream_path": "a.c"},
            {"local": "src/own.c", "source_kind": "bionic-main", "upstream_path": "own.c"},
            {"local": "src/b.c", "source_kind": "project-owned"},
        ]
        (bionic / "import_manifest.json").write_text(json.dumps({"entries": entries}))
        scan = notices.scan_headers(self.root, {"roots": ["src"], "extensions": [".c"]})
        found = {item["file"]: item["issue"] for item in notices.bionic_header_findings(self.root, scan)}
        self.assertEqual({"src/own.c", "src/b.c"}, set(found))
        self.assertIn("without an in-file", found["src/own.c"])
        self.assertIn("project-owned", found["src/b.c"])


class RegistryTests(unittest.TestCase):
    def test_real_registry_is_well_formed_and_staged(self) -> None:
        registry = notices.load_registry()
        names = [c["name"] for c in registry["components"]]
        self.assertEqual(len(names), len(set(names)))
        in_c = {c["name"] for c in notices.stage_components(registry, "01-c")}
        self.assertEqual({"crt-libc", "mksh", "toybox", "onetrue-awk"}, in_c)
        self.assertIn("llvm-runtimes", {c["name"] for c in notices.stage_components(registry, "02-cxx")})
        self.assertNotIn("llvm-runtimes", in_c)
        self.assertIn("llvm-runtimes", {c["name"] for c in notices.stage_components(registry, "06-web")})

    def test_destination_cannot_escape(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "components.json"
            path.write_text(json.dumps({"schema_version": 1, "components": [{
                "name": "x", "first_stage": "01-c", "kind": "crt-runtime", "description": "d",
                "license_declared": "NOASSERTION", "license_note": "n", "source": {},
                "notices": [{"path": "LICENSE.md", "dest": "../evil"}]}]}))
            with self.assertRaises(ValueError):
                notices.load_registry(path)

    def test_real_notices_install_once_at_the_first_stage(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            dest = Path(tmp)
            entries = [{"name": "keep", "kind": "other"}]
            for stage in notices.STAGE_ORDER:
                entries = notices.add_runtime_dependencies(notices.ROOT, dest, stage, entries)
                registry = notices.load_registry()
                self.assertEqual([], notices.missing_runtime_dependencies(registry, stage, entries), stage)
            self.assertEqual("keep", entries[0]["name"])
            runtime = [e for e in entries if e["kind"] == "crt-runtime"]
            self.assertEqual({"crt-libc", "mksh", "toybox", "onetrue-awk", "llvm-runtimes"}, {e["name"] for e in runtime})
            for entry in runtime:
                self.assertTrue(entry["notices"], entry["name"])
                for relative in entry["notices"] + [entry["provenance"]]:
                    self.assertTrue((dest / relative).is_file(), relative)
            again = notices.add_runtime_dependencies(notices.ROOT, dest, "01-c", entries)
            self.assertEqual(len(entries), len(again))

    def test_a_stage_that_lost_a_component_is_reported(self) -> None:
        registry = notices.load_registry()
        self.assertEqual({"crt-libc", "mksh", "toybox", "onetrue-awk"},
                         set(notices.missing_runtime_dependencies(registry, "01-c", [])))
        self.assertIn("llvm-runtimes", notices.missing_runtime_dependencies(registry, "05-ui", []))

    def test_toybox_license_is_the_0bsd_text(self) -> None:
        text = (notices.ROOT / "shell/toybox/src/LICENSE").read_text()
        self.assertIn("Permission to use, copy, modify, and/or distribute this software for any", text)
        self.assertIn("with or without fee is hereby granted", text)

    def test_llvm_license_texts_carry_the_exception(self) -> None:
        for name in ("llvm-project", "libcxx", "libcxxabi", "libunwind", "compiler-rt"):
            text = (notices.ROOT / "libstdc++/third_party/llvm-license" / f"{name}.LICENSE.TXT").read_text()
            self.assertIn("Apache License v2.0 with LLVM Exceptions", text, name)
            self.assertIn("LLVM Exceptions to the Apache 2.0 License", text, name)

    def test_libc_scan_findings_are_all_resolved_by_recorded_decisions(self) -> None:
        registry = notices.load_registry()
        libc = next(c for c in registry["components"] if c["name"] == "crt-libc")
        scan = notices.scan_headers(notices.ROOT, libc["header_scan"])
        raw = notices.bionic_header_findings(notices.ROOT, scan)
        self.assertEqual({"libc/src/gdtoa/gdtoa_fltrnds.h", "libm/src/freebsd/s_signgam.c",
                          "libm/src/freebsd/s_llrint.c", "libm/src/freebsd/s_llrintf.c"},
                         {item["file"] for item in raw})
        decided, still_open = notices.resolve_findings(
            notices.ROOT, raw, scan, notices.load_decisions(notices.ROOT, libc["header_scan"]))
        self.assertEqual([], still_open, "a new import without a license header needs a recorded decision")
        self.assertEqual(4, len(decided))
        for entry in decided:
            self.assertTrue(entry["upstream"] and entry["basis"], entry["file"])
        text = notices.render_notice(libc, scan, decided)
        self.assertIn("Imported files without an in-file license header", text)
        self.assertIn("libm/src/freebsd/s_signgam.c", text)

    def test_gdtoa_support_is_a_bionic_derived_apache_file_with_a_modification_notice(self) -> None:
        source = (notices.ROOT / "libc" / "src" / "gdtoa" / "gdtoa_support.c").read_text(encoding="utf-8")
        self.assertIn("Licensed under the Apache License, Version 2.0", source)
        self.assertIn("Modified by the CRT project", source)
        manifest = json.loads((notices.ROOT / "third_party" / "bionic" / "import_manifest.json").read_text(encoding="utf-8"))
        entry = next(e for e in manifest["entries"] if e["local"] == "libc/src/gdtoa/gdtoa_support.c")
        self.assertEqual("bionic-main", entry["source_kind"])
        self.assertTrue(entry["upstream_path"].endswith("gdtoa_support.cpp"))

    def test_a_changed_file_reopens_its_decision(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "src").mkdir()
            # newline="\n": the decision is bound to the file's exact bytes, which a Windows text-mode write would change
            (root / "src" / "ref.c").write_text(BSD + "int r;\n", newline="\n")
            (root / "src" / "frag.c").write_text("int frag;\n", newline="\n")
            scan = notices.scan_headers(root, {"roots": ["src"], "extensions": [".c"]})
            finding = {"file": "src/frag.c", "issue": "import without an in-file license header"}
            decision = {"file": "src/frag.c", "sha256": notices.sha256_bytes(b"int frag;\n"), "decision": "covered-by-group",
                        "covered_by": "src/ref.c", "license": "BSD", "upstream": [], "basis": []}
            decided, still_open = notices.resolve_findings(root, [finding], scan, [decision])
            self.assertEqual(([1], []), ([item["group"] for item in decided], still_open))
            (root / "src" / "frag.c").write_text("int frag; /* edited */\n", newline="\n")
            decided, still_open = notices.resolve_findings(root, [finding], scan, [decision])
            self.assertEqual([], decided)
            self.assertIn("changed since", still_open[0]["issue"])
            decision["covered_by"] = "src/missing.c"
            (root / "src" / "frag.c").write_text("int frag;\n", newline="\n")
            decided, still_open = notices.resolve_findings(root, [finding], scan, [decision])
            self.assertIn("carries no license header", still_open[0]["issue"])


class VerifyDistRuntimeTests(unittest.TestCase):
    def entry(self, name: str, notices_: list | None = None) -> dict:
        return {"name": name, "kind": "crt-runtime", "notices": ["share/licenses/x"] if notices_ is None else notices_}

    def test_first_stage_needs_libc_and_shell_notices(self) -> None:
        full = {n: self.entry(n) for n in ("crt-libc", "mksh", "toybox", "onetrue-awk")}
        verify_dist.validate_runtime_components("01-c", full)
        for missing in full:
            partial = {k: v for k, v in full.items() if k != missing}
            with self.assertRaises(SystemExit, msg=missing):
                verify_dist.validate_runtime_components("01-c", partial)

    def test_later_stage_also_needs_the_llvm_runtime_and_real_notices(self) -> None:
        deps = {n: self.entry(n) for n in ("crt-libc", "mksh", "toybox", "onetrue-awk")}
        with self.assertRaises(SystemExit):
            verify_dist.validate_runtime_components("05-ui", deps)
        deps["llvm-runtimes"] = self.entry("llvm-runtimes", [])
        with self.assertRaises(SystemExit):
            verify_dist.validate_runtime_components("05-ui", deps)
        deps["llvm-runtimes"] = self.entry("llvm-runtimes")
        verify_dist.validate_runtime_components("05-ui", deps)
        verify_dist.validate_runtime_components("01-c", {k: v for k, v in deps.items() if k != "llvm-runtimes"})

    def test_registry_and_verifier_agree_on_first_stages(self) -> None:
        registry = notices.load_registry()
        expected = {}
        for component in registry["components"]:
            expected.setdefault(component["first_stage"], set()).add(component["name"])
        self.assertEqual(expected, {k: set(v) for k, v in verify_dist.RUNTIME_COMPONENTS_BY_STAGE.items()})


if __name__ == "__main__":
    unittest.main()
