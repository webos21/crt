#!/usr/bin/env python3
"""Capture and compare Linux Dev SDK link surfaces and ELF runtime imports."""

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

from crt_elf import archive_machine_ids, interpreter, machine_id, needed_libraries, runtime_paths
from crt_sdk_install_metadata import content_sha256, file_digest, inventory

MACHINES = {"x86_64": 62, "aarch64": 183}
TOOLS = Path(__file__).resolve().parent


def output(argv):
    return subprocess.check_output(argv, text=True, encoding="utf-8", timeout=60)


def static_symbols(path, nm):
    """Retain externally defined names and nm types, including weak symbols."""
    rows = set()
    for line in output([nm, "--format=posix", "--extern-only", "--defined-only", path]).splitlines():
        if not line or line.endswith(":"):
            continue
        name, kind, *_ = line.split()
        rows.add((name, kind))
    return [list(row) for row in sorted(rows)]


def dynamic_symbols(path, readelf):
    exports, imports = set(), set()
    for line in output([readelf, "--dyn-syms", "--wide", path]).splitlines():
        if not re.match(r"^\s*\d+:\s", line):
            continue
        fields = line.split(maxsplit=7)
        if len(fields) != 8:
            if fields[-1] != "UND" or fields[4] != "LOCAL":
                raise ValueError(f"unexpected dynsym row in {path}: {line}")
            continue
        _, _, size, kind, binding, visibility, section, name = fields
        if binding == "LOCAL":
            continue
        # readelf adds a numeric version-index in parentheses to some names.
        # Keep the version itself, but discard its image-specific table index.
        name = re.sub(r" \(\d+\)$", "", name)
        if section == "UND":
            imports.add((name, kind, binding, visibility))
        elif visibility in {"DEFAULT", "PROTECTED"}:
            # Object and TLS widths are part of the link ABI. Code size is not.
            exports.add((name, kind, binding, visibility, int(size) if kind in {"OBJECT", "TLS"} else None))
    return [list(row) for row in sorted(exports)], [list(row) for row in sorted(imports)]


def capture(root, readelf, nm):
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    profile = json.loads((root / "share/crt/contracts/development.json").read_text(encoding="utf-8"))
    target = manifest["target"]
    if manifest["stage"] != "02-cxx" or profile["profile"] != "dev" or target["os"] != "linux":
        raise ValueError("expected a Linux Development SDK")
    expected = MACHINES[target["arch"]]
    rows = inventory(root)
    binaries, aliases = {}, {}
    for relative, kind, _ in rows:
        path = root / relative
        if kind == "symlink":
            continue
        with path.open("rb") as stream:
            header = stream.read(20)
        if header[:8] == b"!<arch>\n" or path.suffix == ".a":
            if archive_machine_ids(path) != {expected}:
                raise ValueError(f"foreign object in archive: {relative}")
            binaries[relative] = {"sha256": file_digest(path), "static_symbols": static_symbols(path, nm)}
        elif header.startswith(b"\x7fELF"):
            if machine_id(path) != expected:
                raise ValueError(f"foreign ELF image: {relative}")
            elf_type = int.from_bytes(header[16:18], "little" if header[5] == 1 else "big")
            image = {"sha256": file_digest(path), "elf_type": elf_type}
            if elf_type == 1:  # Relocatable startup objects and loader import inputs.
                image["static_symbols"] = static_symbols(path, nm)
            elif elf_type in {2, 3}:  # Executable and shared objects.
                image["exports"], image["imports"] = dynamic_symbols(path, readelf)
                image["needed"] = needed_libraries(path)
                image["interpreter"] = interpreter(path)
                image["runtime_paths"] = runtime_paths(path)
                dynamic = output([readelf, "--dynamic", "--wide", path])
                match = re.search(r"\(SONAME\).*?\[(.*?)\]", dynamic)
                image["soname"] = match.group(1) if match else None
            else:
                raise ValueError(f"unexpected ELF type {elf_type}: {relative}")
            binaries[relative] = image
    for relative, kind, value in rows:
        if kind == "symlink":
            resolved = (root / relative).resolve().relative_to(root).as_posix()
            if resolved in binaries:
                aliases[relative] = value
    if not binaries:
        raise ValueError("SDK contains no ELF artifacts")
    return {
        "target": target, "target_triple": manifest["target_triple"],
        "version": profile["version"], "producer_source_commit": profile["producer_source_commit"],
        "content_sha256": content_sha256(rows), "binaries": binaries, "aliases": aliases,
    }


def compare(old, new):
    before, after = old["binaries"], new["binaries"]
    changes = {}
    set_fields = {"exports", "imports", "static_symbols"}
    for path in sorted(set(before) & set(after)):
        left, right = before[path], after[path]
        delta = {}
        for field in sorted((set(left) | set(right)) - {"sha256"}):
            if left.get(field) == right.get(field):
                continue
            if field in set_fields:
                old_set = {tuple(row) for row in left.get(field, [])}
                new_set = {tuple(row) for row in right.get(field, [])}
                delta[field] = {"removed": sorted(old_set - new_set), "added": sorted(new_set - old_set)}
            else:
                delta[field] = {"before": left.get(field), "after": right.get(field)}
        if delta:
            changes[path] = delta
    aliases = {name: {"before": old["aliases"].get(name), "after": new["aliases"].get(name)}
               for name in sorted(set(old["aliases"]) | set(new["aliases"]))
               if old["aliases"].get(name) != new["aliases"].get(name)}
    return {"added_images": sorted(set(after) - set(before)), "removed_images": sorted(set(before) - set(after)),
            "changed_metadata": changes, "changed_aliases": aliases,
            "changed_binary_bytes": sorted(path for path in set(before) & set(after)
                                           if before[path]["sha256"] != after[path]["sha256"])}


def write(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("baseline", "candidate", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--readelf", default="readelf")
    parser.add_argument("--nm", default="llvm-nm")
    args = parser.parse_args()
    readelf, nm = (shutil.which(tool) for tool in (args.readelf, args.nm))
    if not readelf or not nm:
        parser.error("GNU readelf and LLVM nm are required")
    old, new = (path.resolve() for path in (args.baseline, args.candidate))
    destination = args.output.resolve()
    if old == new or any(destination.is_relative_to(path) for path in (old, new)):
        parser.error("use distinct SDKs and an evidence directory outside both")
    destination.mkdir(parents=True)
    snapshots = []
    for label, path in (("baseline", old), ("candidate", new)):
        subprocess.run([sys.executable, TOOLS / "verify_dist.py", "--dist", path, "--stage", "02-cxx", "--profile", "dev"],
                       check=True, timeout=120)
        state = capture(path, readelf, nm)
        write(destination / (label + ".json"), state)
        snapshots.append(state)
    earlier, current = snapshots
    if any(earlier[key] != current[key] for key in ("target", "target_triple")):
        raise ValueError("compare SDKs for the same native target")
    delta = compare(earlier, current)
    write(destination / "comparison.json", delta)
    print(f"ELF baseline: {len(earlier['binaries'])} -> {len(current['binaries'])} images and archives")
    print(", ".join(f"{key}={len(rows)}" for key, rows in delta.items()))
    print("See comparison.json for ABI review.")


if __name__ == "__main__":
    main()
