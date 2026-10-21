#!/usr/bin/env python3
"""Notices and provenance for the runtime components every CRT SDK inherits (Tranche 10).

The Web stage already ships WebKit's and its ports' notices. The runtime underneath (libc/libm/libdl,
the shell, the imported LLVM C++ runtime) is redistributed by every SDK stage, so its notices have to
be in every SDK too. third_party/crt-runtime/components.json declares those components; this module

  * copies each declared notice file into share/licenses/<name>/,
  * generates share/licenses/crt-libc/NOTICE by grouping the distinct in-file license headers of the
    imported sources (a file without a license header is project-owned, LICENSE.md),
  * writes share/crt/dependencies/<name>/provenance.json, and
  * returns the manifest "redistributed_dependencies" entries.

It never asserts a license it did not verify: license_declared stays NOASSERTION for mixed components.
"""

import argparse
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REGISTRY = ROOT / "third_party" / "crt-runtime" / "components.json"
STAGE_ORDER = ["01-c", "02-cxx", "03-gfx-simple", "04-gfx-media", "05-ui", "06-web"]

HEADER_BLOCK = re.compile(r"\s*(/\*.*?\*/|(?://[^\n]*\n)+|(?:#[^\n]*\n)+|(?:;[^\n]*\n)+)", re.S)
# "permission" alone also matches prose such as "executable-memory permissions"; a grant says "permission to/is hereby".
LICENSE_WORDS = re.compile(r"copyright|permission\s+(?:to|is\s+hereby)|licen[sc]e|redistribut", re.I)
COPYRIGHT_LINE = re.compile(r"(?im)^[\s*/#;]*(?:copyright|\(c\)).*$")
DIGITS_YEAR = re.compile(r"\b(?:19|20)\d\d\b")
SPDX_LINE = re.compile(r"(?im)^[\s*/#;]*SPDX-License-Identifier:\s*(.+?)\s*(?:\*/)?$")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_registry(path: Path = REGISTRY) -> dict:
    registry = json.loads(path.read_text(encoding="utf-8"))
    if registry.get("schema_version") != 1:
        raise ValueError("unsupported crt-runtime registry schema")
    seen = set()
    for component in registry["components"]:
        name = component["name"]
        if name in seen:
            raise ValueError(f"duplicate runtime component {name}")
        seen.add(name)
        if component["first_stage"] not in STAGE_ORDER:
            raise ValueError(f"{name}: unknown first_stage {component['first_stage']}")
        for field in ("kind", "description", "license_declared", "license_note", "source", "notices"):
            if field not in component:
                raise ValueError(f"{name}: missing {field}")
        for notice in component["notices"]:
            dest = Path(notice["dest"])
            if dest.is_absolute() or ".." in dest.parts:
                raise ValueError(f"{name}: notice destination escapes its directory: {notice['dest']}")
    return registry


def stage_includes(component: dict, stage: str) -> bool:
    return STAGE_ORDER.index(stage) >= STAGE_ORDER.index(component["first_stage"])


def header_of(text: str) -> str:
    """The first leading comment block that carries license words (RCS-id and #include-free prologues
    often put a one-line `$OpenBSD$` comment before the real license comment)."""
    position = 0
    for _ in range(4):
        match = HEADER_BLOCK.match(text, position)
        if not match:
            return ""
        if LICENSE_WORDS.search(match.group(1)):
            return match.group(1)
        position = match.end()
    return ""


def normalise(header: str) -> str:
    body = COPYRIGHT_LINE.sub("", header)
    body = re.sub(r"[\s*/#;]+", " ", body).strip().lower()
    return DIGITS_YEAR.sub("", body)


def scan_headers(root: Path, scan: dict) -> dict:
    """Groups the distinct license headers of the files under scan['roots'].

    Returns {"groups": [...], "headerless": [...]} with deterministic order."""
    extensions = set(scan["extensions"])
    groups = {}
    headerless = []
    for base in scan["roots"]:
        directory = root / base
        if not directory.is_dir():
            raise ValueError(f"header scan root is missing: {base}")
        for path in sorted(directory.rglob("*")):
            if not path.is_file() or path.suffix not in extensions:
                continue
            relative = path.relative_to(root).as_posix()
            header = header_of(path.read_text(encoding="utf-8", errors="replace"))
            if not header:
                headerless.append(relative)
                continue
            key = hashlib.sha256(normalise(header).encode("utf-8")).hexdigest()
            group = groups.setdefault(key, {"text": header.strip(), "files": [], "copyrights": set(), "spdx": set()})
            group["files"].append(relative)
            for line in header.splitlines():
                if COPYRIGHT_LINE.match(line):
                    group["copyrights"].add(line.strip(" \t*/#;").strip())
            group["spdx"].update(SPDX_LINE.findall(header))
    ordered = sorted(groups.values(), key=lambda group: (-len(group["files"]), group["files"][0]))
    return {"groups": ordered, "headerless": headerless}


def load_decisions(root: Path, scan_config: dict) -> list:
    """The reviewed resolutions of header findings (third_party/bionic/header_decisions.json), if configured."""
    location = scan_config.get("decisions")
    if not location:
        return []
    path = root / location
    if not path.is_file():
        raise ValueError(f"header decisions file is missing: {location}")
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema_version") != 1:
        raise ValueError(f"{location}: unsupported schema_version")
    return data["entries"]


def group_number(scan: dict, file: str) -> int:
    """1-based number of the license group that lists `file`, or 0."""
    for index, group in enumerate(scan["groups"], 1):
        if file in group["files"]:
            return index
    return 0


def resolve_findings(root: Path, findings: list, scan: dict, decisions: list) -> tuple:
    """Splits the findings into (resolved decisions, open findings).

    A finding is resolved by a decision for the same file whose SHA-256 equals the file as it is now
    (a changed file reopens it) and whose `covered_by` reference file really carries a license header
    in the scan. Everything else stays open."""
    by_file = {entry["file"]: entry for entry in decisions}
    resolved, still_open = [], []
    for finding in findings:
        entry = by_file.get(finding["file"])
        if entry is None:
            still_open.append(finding)
            continue
        current = sha256_bytes((root / finding["file"]).read_bytes())
        if current != entry["sha256"]:
            still_open.append({**finding, "issue": "changed since its header decision was recorded"})
            continue
        number = group_number(scan, entry["covered_by"]) if entry.get("covered_by") else 0
        if entry["decision"] == "covered-by-group" and not number:
            still_open.append({**finding, "issue": f"covering file {entry['covered_by']} carries no license header"})
            continue
        resolved.append({**entry, "group": number, "finding": finding["issue"]})
    return resolved, still_open


def render_notice(component: dict, scan: dict, resolved: list | None = None) -> str:
    lines = [f"{component['name']} -- third-party license headers found in the redistributed sources",
             "",
             "Project-owned code (every file below without a license header) is under the default license",
             "in LICENSE.md. Each group below is one distinct in-file license text, with the copyright",
             "lines and files it covers. This file is generated by tools/crt_runtime_notices.py; the",
             "per-file headers in the source remain authoritative.",
             "",
             f"Files with a license header: {sum(len(g['files']) for g in scan['groups'])}; "
             f"files without one (project default license): {len(scan['headerless'])}.", ""]
    for index, group in enumerate(scan["groups"], 1):
        lines.append("=" * 78)
        lines.append(f"Group {index}: {len(group['files'])} file(s)")
        if group["spdx"]:
            lines.append("SPDX-License-Identifier: " + ", ".join(sorted(group["spdx"])))
        lines.append("Copyright lines:")
        if group["copyrights"]:
            lines.extend("  " + item for item in sorted(group["copyrights"]))
        else:
            lines.append("  (none stated)")
        lines.append("Files:")
        lines.extend("  " + name for name in group["files"])
        lines.append("License text (from " + group["files"][0] + "):")
        lines.append("-" * 78)
        lines.append(group["text"])
        lines.append("")
    if resolved:
        lines.append("=" * 78)
        lines.append("Imported files without an in-file license header")
        lines.append("")
        lines.append("These files carry no license text upstream either. Each was compared with its upstream and")
        lines.append("attributed as below (engineering review; the legal review of this notice is a separate gate).")
        lines.append("")
        for entry in resolved:
            lines.append(entry["file"])
            lines.append("  License: " + entry["license"])
            if entry.get("group"):
                lines.append(f"  Same license as Group {entry['group']} (see {entry['covered_by']}).")
            for upstream in entry["upstream"]:
                lines.append(f"  Upstream {upstream['comparison']}: {upstream['repository']} {upstream['path']} @ {upstream['ref']}")
            for reason in entry["basis"]:
                lines.append("  - " + reason)
            lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def bionic_header_findings(root: Path, scan: dict) -> list:
    """Cross-checks the Bionic import manifest against the header scan.

    Returns the entries a reviewer must look at: an import with no license header, or a project-owned
    file that does carry a third-party header."""
    manifest = root / "third_party" / "bionic" / "import_manifest.json"
    if not manifest.is_file():
        return []
    headerless = set(scan["headerless"])
    findings = []
    for entry in json.loads(manifest.read_text(encoding="utf-8"))["entries"]:
        local = entry["local"]
        has_header = (root / local).is_file() and local not in headerless
        if entry["source_kind"] != "project-owned" and not has_header:
            findings.append({"file": local, "issue": "import without an in-file license header",
                             "source_kind": entry["source_kind"], "upstream_path": entry.get("upstream_path")})
        elif entry["source_kind"] == "project-owned" and has_header:
            findings.append({"file": local, "issue": "manifest says project-owned but the file carries a license header"})
    return findings


def stage_components(registry: dict, stage: str) -> list:
    return [component for component in registry["components"] if stage_includes(component, stage)]


def install_component(root: Path, destination: Path, component: dict) -> dict:
    """Copies one component's notices and writes its provenance; returns its manifest entry."""
    name = component["name"]
    licenses = Path("share/licenses") / name
    notices = []
    recorded = []
    for notice in component["notices"]:
        source = root / notice["path"]
        if not source.is_file():
            raise ValueError(f"{name}: notice source is missing: {notice['path']}")
        data = source.read_bytes()
        if "sha256" in notice and notice["sha256"] != sha256_bytes(data):
            raise ValueError(f"{name}: {notice['path']} differs from its pinned SHA-256")
        target = licenses / notice["dest"]
        (destination / target).parent.mkdir(parents=True, exist_ok=True)
        (destination / target).write_bytes(data)
        notices.append(target.as_posix())
        recorded.append({"path": notice["path"], "dest": target.as_posix(), "sha256": sha256_bytes(data)})
    provenance = {"name": name, "version": component.get("version", "repository"),
                  "kind": component["kind"], "description": component["description"],
                  "license_declared": component["license_declared"], "license_note": component["license_note"],
                  "source": component["source"], "first_stage": component["first_stage"], "notices": recorded}
    scan_findings = []
    if "header_scan" in component:
        scan = scan_headers(root, component["header_scan"])
        scan_findings = bionic_header_findings(root, scan)
        decided, scan_findings = resolve_findings(root, scan_findings, scan, load_decisions(root, component["header_scan"]))
        text = render_notice(component, scan, decided)
        target = licenses / "NOTICE"
        (destination / target).parent.mkdir(parents=True, exist_ok=True)
        (destination / target).write_text(text, encoding="utf-8", newline="\n")
        notices.append(target.as_posix())
        provenance["header_scan"] = {
            "roots": component["header_scan"]["roots"], "groups": len(scan["groups"]),
            "files_with_header": sum(len(g["files"]) for g in scan["groups"]),
            "files_without_header": len(scan["headerless"]),
            "notice_sha256": sha256_bytes(text.encode("utf-8")),
            "review_findings": scan_findings,
            "review_decisions": [{"file": item["file"], "decision": item["decision"], "group": item["group"],
                                  "sha256": item["sha256"]} for item in decided]}
    provenance_path = Path("share/crt/dependencies") / name / "provenance.json"
    (destination / provenance_path).parent.mkdir(parents=True, exist_ok=True)
    (destination / provenance_path).write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8", newline="\n")
    return {"name": name, "kind": component["kind"], "headers": [], "link_artifacts": [], "runtime_artifacts": [],
            "notices": notices, "provenance": provenance_path.as_posix()}


def add_runtime_dependencies(root: Path, destination: Path, stage: str, dependencies: list) -> list:
    """Adds the components that first appear in this stage to a manifest dependency list.

    Later stages inherit the earlier entries and notice files from their predecessor SDK, so a
    component is installed exactly once, at its first_stage (an isolated later stage has no libc or
    shell sources to read). Installing the same stage again replaces its own entries."""
    registry = load_registry(root / "third_party" / "crt-runtime" / "components.json")
    introduced = [component for component in registry["components"] if component["first_stage"] == stage]
    names = {component["name"] for component in introduced}
    result = [item for item in dependencies if item.get("name") not in names]
    for component in introduced:
        result.append(install_component(root, destination, component))
    return result


def missing_runtime_dependencies(registry: dict, stage: str, dependencies: list) -> list:
    """Names of runtime components the stage must declare but its manifest does not."""
    declared = {item.get("name") for item in dependencies if item.get("kind") == "crt-runtime"}
    return [component["name"] for component in stage_components(registry, stage) if component["name"] not in declared]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--stage", default="06-web", choices=STAGE_ORDER)
    parser.add_argument("--dest", type=Path, help="install notices into this directory (default: report only)")
    args = parser.parse_args()
    registry = load_registry(args.root / "third_party" / "crt-runtime" / "components.json")
    for component in stage_components(registry, args.stage):
        if "header_scan" in component:
            scan = scan_headers(args.root, component["header_scan"])
            decided, findings = resolve_findings(args.root, bionic_header_findings(args.root, scan), scan,
                                                 load_decisions(args.root, component["header_scan"]))
            print(f"{component['name']}: {len(scan['groups'])} license header groups, "
                  f"{sum(len(g['files']) for g in scan['groups'])} headed / {len(scan['headerless'])} project-default files, "
                  f"{len(decided)} finding(s) resolved by a recorded decision, {len(findings)} open")
            for finding in findings:
                print("  review:", json.dumps(finding))
        else:
            print(f"{component['name']}: {len(component['notices'])} notice file(s)")
    if args.dest:
        if args.dest.exists():
            shutil.rmtree(args.dest)
        entries = []
        for stage in STAGE_ORDER[:STAGE_ORDER.index(args.stage) + 1]:
            entries = add_runtime_dependencies(args.root, args.dest, stage, entries)
        print("installed under", args.dest)
    return 0


if __name__ == "__main__":
    sys.exit(main())
