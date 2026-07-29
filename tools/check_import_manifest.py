#!/usr/bin/env python3
import json
import sys
from pathlib import Path


REQUIRED_ENTRY_KEYS = {
    "local",
    "component",
    "source_kind",
    "upstream_ref",
    "upstream_path",
    "status",
    "review_class",
    "license_family",
    "reason",
    "next_action",
}

SOURCE_KINDS = {
    "bionic-main",
    "bionic-legacy",
    "freebsd-upstream",
    "project-owned",
    "compiler-provided",
}

REVIEW_CLASSES = {
    "main_current",
    "bootstrap_keep",
    "main_replace_candidate",
    "project_owned_transition",
    "project_owned",
}


def fail(message):
    print(f"import_manifest: {message}", file=sys.stderr)
    return 1


def main():
    root = Path(__file__).resolve().parents[1]
    manifest_path = root / "third_party" / "bionic" / "import_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    entries = manifest.get("entries")

    if manifest.get("schema_version") != 1:
        return fail("unsupported or missing schema_version")
    if not isinstance(entries, list) or not entries:
        return fail("entries must be a non-empty list")

    seen = set()
    legacy_count = 0
    errors = []

    for index, entry in enumerate(entries):
        missing = REQUIRED_ENTRY_KEYS - set(entry)
        extra = set(entry) - REQUIRED_ENTRY_KEYS
        if missing:
            errors.append(f"entry {index} missing keys: {sorted(missing)}")
        if extra:
            errors.append(f"entry {index} has unknown keys: {sorted(extra)}")

        local = entry.get("local")
        if local in seen:
            errors.append(f"duplicate local path: {local}")
        seen.add(local)

        if not local or not (root / local).exists():
            errors.append(f"local path does not exist: {local}")

        source_kind = entry.get("source_kind")
        review_class = entry.get("review_class")
        if source_kind not in SOURCE_KINDS:
            errors.append(f"{local}: invalid source_kind {source_kind!r}")
        if review_class not in REVIEW_CLASSES:
            errors.append(f"{local}: invalid review_class {review_class!r}")

        if source_kind == "bionic-legacy":
            legacy_count += 1
            if review_class == "main_current":
                errors.append(f"{local}: legacy source cannot be main_current")
        if source_kind == "bionic-main" and review_class != "main_current":
            errors.append(f"{local}: bionic-main entries must be main_current")
        if source_kind == "project-owned" and review_class != "project_owned":
            errors.append(f"{local}: project-owned entries must be project_owned")

    if legacy_count == 0:
        errors.append("manifest must explicitly track at least one legacy exception")

    if errors:
        for error in errors:
            print(f"import_manifest: {error}", file=sys.stderr)
        return 1

    print(f"import_manifest: ok ({len(entries)} entries, {legacy_count} legacy)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
