#!/usr/bin/env python3
"""Validate libcrtweb/patches/manifest.json against the licensing gate (Web Tranche 3B).

Every WebKit file a CRT patch modifies must carry a recorded license class that CRT can redistribute
a derivative of, and that class must be what the file's header really says; every file CRT adds to
a WebKit tree must be listed under new_files as CRT-owned with the project default license and must
actually carry that header. With --tree (a WPE tarball extraction or the product source checkout) the
recorded classes and the upstream SHA-256 values are re-measured; without it only the schema and
policy are checked. Exit status is non-zero on any violation.
"""

import argparse
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import scan_webkit_headers as scanner

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "libcrtweb" / "patches" / "manifest.json"

# Classes in which CRT may modify a file and redistribute the result (LICENSE.md, Tranche 3B).
MODIFIABLE = {"BSD-2-Clause", "BSD-3-Clause", "MIT"} | scanner.LGPL
CRT_DEFAULT = "BSD-2-Clause"
CRT_HEADER_PHRASE = "the crt project"


def check(manifest: dict, tree: Path | None, repo: Path) -> list:
    problems = []
    if manifest.get("schema_version") != 2:
        problems.append("schema_version must be 2 (per-file license fields)")
    policy = manifest.get("license_policy")
    if not isinstance(policy, dict) or policy.get("crt_default_license") != CRT_DEFAULT:
        problems.append("license_policy.crt_default_license must be " + CRT_DEFAULT)
    ids = set()
    for patch in manifest.get("patches", []):
        name = patch.get("id", "?")
        if name in ids:
            problems.append(f"{name}: duplicate id")
        ids.add(name)
        for field in ("patch", "targets", "files", "reason", "remove_when"):
            if field not in patch:
                problems.append(f"{name}: missing {field}")
        if not (ROOT / "libcrtweb" / "patches" / patch.get("patch", "")).is_file():
            problems.append(f"{name}: patch file {patch.get('patch')} not found")
        for entry in patch.get("files", []):
            path = entry.get("file", "?")
            where = f"{name}: {path}"
            recorded = entry.get("license")
            if not recorded:
                problems.append(f"{where}: no recorded license class")
                continue
            if recorded == "no-license-header":
                if not entry.get("license_basis"):
                    problems.append(f"{where}: a file without a header needs a license_basis naming the "
                                    "directory-level license file")
            elif recorded not in MODIFIABLE:
                problems.append(f"{where}: license class {recorded!r} is not one CRT modifies and "
                                f"redistributes ({', '.join(sorted(MODIFIABLE))})")
            if tree is not None:
                target = tree / path
                if not target.is_file():
                    problems.append(f"{where}: not in {tree}")
                    continue
                data = target.read_bytes()
                measured = scanner.classify_header(data)
                if measured != recorded:
                    problems.append(f"{where}: header says {measured!r}, manifest records {recorded!r}")
                if hashlib.sha256(data).hexdigest() != entry.get("sha256_before"):
                    problems.append(f"{where}: not the upstream bytes (sha256_before)")
    for entry in manifest.get("new_files", []):
        path = entry.get("file", "?")
        if entry.get("origin") != "crt":
            problems.append(f"new_files: {path}: origin must be 'crt'")
        if entry.get("license") != CRT_DEFAULT:
            problems.append(f"new_files: {path}: license must be {CRT_DEFAULT}")
        source = repo / entry.get("source", path)
        if source.is_file():
            head = source.read_bytes()[:scanner.HEADER_BYTES].decode("utf-8", "replace").lower()
            if CRT_HEADER_PHRASE not in head:
                problems.append(f"new_files: {path}: the CRT license header is missing")
        else:
            problems.append(f"new_files: {path}: source {source} not found")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    parser.add_argument("--tree", type=Path, help="a WebKit tree to measure license classes and SHA-256 against")
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    problems = check(manifest, args.tree, ROOT)
    for problem in problems:
        print("patch manifest:", problem, file=sys.stderr)
    if problems:
        return 1
    files = sum(len(patch.get("files", [])) for patch in manifest["patches"])
    print(f"patch manifest ok: {len(manifest['patches'])} patches, {files} modified files, "
          f"{len(manifest.get('new_files', []))} new CRT-owned files"
          + (f", measured against {args.tree}" if args.tree else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
