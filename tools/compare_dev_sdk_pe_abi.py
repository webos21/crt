#!/usr/bin/env python3
"""Capture and compare Windows Dev SDK link surfaces and PE runtime imports (R05-01).

The Windows counterpart of compare_dev_sdk_elf_abi.py, which it reuses for the static-symbol capture and the diff.
Every PE image records its named exports (ordinal-only exports included), its normal and delay-load imports as
(dll, symbol) pairs and its machine type; every static or import archive and COFF object records its externally
defined names. All images, objects and archive members must be of the SDK's target machine.
"""

import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

from compare_dev_sdk_elf_abi import compare, output, static_symbols, write
from crt_pe import exported_symbols
from crt_sdk_install_metadata import content_sha256, file_digest, inventory

MACHINES = {"x86_64": (0x8664, "IMAGE_FILE_MACHINE_AMD64"), "aarch64": (0xAA64, "IMAGE_FILE_MACHINE_ARM64")}
TOOLS = Path(__file__).resolve().parent


def pe_machine(data: bytes) -> int:
    if len(data) < 0x40:
        raise ValueError("truncated DOS header")
    offset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[offset:offset + 4] != b"PE\0\0":
        raise ValueError("invalid PE signature")
    return struct.unpack_from("<H", data, offset + 4)[0]


def parse_imports(text: str) -> list[list[str]]:
    """(dll, symbol) pairs from `llvm-readobj --coff-imports`, normal and delay-load blocks alike."""
    rows, library = set(), None
    for raw in text.splitlines():
        line = raw.strip()
        if line in ("Import {", "DelayImport {"):
            library = None
        elif line.startswith("Name:") and library is None:
            library = line.split(":", 1)[1].strip().lower()
        elif line.startswith("Symbol:") and library is not None:
            value = line.split(":", 1)[1].strip()
            name, _, hint = value.rpartition("(")
            # An import by ordinal prints no name, only "(12)": keep that number, the only identity it has.
            rows.add((library, name.strip() or "ordinal:" + hint.rstrip(")")))
    return [list(row) for row in sorted(rows)]


def parse_machines(text: str) -> set[str]:
    """Every machine named by `llvm-readobj --file-headers` over an archive or object."""
    return {line.split(":", 1)[1].strip().split(" ")[0] for line in text.splitlines()
            if line.strip().startswith("Machine:")}


def capture(root: Path, readobj: str, nm: str) -> dict:
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    profile = json.loads((root / "share/crt/contracts/development.json").read_text(encoding="utf-8"))
    target = manifest["target"]
    if manifest["stage"] != "02-cxx" or profile["profile"] != "dev" or target["os"] != "windows":
        raise ValueError("expected a Windows Development SDK")
    number, name = MACHINES[target["arch"]]
    rows = inventory(root)
    binaries, aliases = {}, {}
    for relative, kind, _ in rows:
        path = root / relative
        if kind == "symlink":
            continue
        with path.open("rb") as stream:
            header = stream.read(8)
        if header == b"!<arch>\n":
            machines = parse_machines(output([readobj, "--file-headers", path]))
            if machines - {name, "IMAGE_FILE_MACHINE_UNKNOWN"}:
                raise ValueError(f"foreign object in archive: {relative}: {sorted(machines)}")
            binaries[relative] = {"sha256": file_digest(path), "static_symbols": static_symbols(path, nm)}
        elif header[:2] == b"MZ":
            if pe_machine(path.read_bytes()) != number:
                raise ValueError(f"foreign PE image: {relative}")
            binaries[relative] = {"sha256": file_digest(path),
                                  "exports": [[symbol] for symbol in sorted(exported_symbols(path))],
                                  "imports": parse_imports(output([readobj, "--coff-imports", path]))}
        elif path.suffix in (".o", ".obj") and len(header) >= 2:
            if struct.unpack_from("<H", header)[0] != number:
                raise ValueError(f"foreign or unknown COFF object: {relative}")
            binaries[relative] = {"sha256": file_digest(path), "static_symbols": static_symbols(path, nm)}
    for relative, kind, value in rows:
        if kind == "symlink":
            resolved = (root / relative).resolve().relative_to(root).as_posix()
            if resolved in binaries:
                aliases[relative] = value
    if not binaries:
        raise ValueError("SDK contains no PE artifacts")
    return {"target": target, "target_triple": manifest["target_triple"], "version": profile["version"],
            "producer_source_commit": profile["producer_source_commit"], "content_sha256": content_sha256(rows),
            "binaries": binaries, "aliases": aliases}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("baseline", "candidate", "output"):
        parser.add_argument("--" + argument, required=True, type=Path)
    parser.add_argument("--readobj", default="llvm-readobj")
    parser.add_argument("--nm", default="llvm-nm")
    parser.add_argument("--baseline-predates-profile", action="store_true",
                        help="skip the current Dev profile verification of the BASELINE only, for a published SDK that "
                             "predates the profile's newer capability records; the candidate is always verified and the "
                             "skip is recorded in verification.json")
    args = parser.parse_args()
    readobj, nm = (shutil.which(tool) for tool in (args.readobj, args.nm))
    if not readobj or not nm:
        parser.error("llvm-readobj and llvm-nm are required")
    old, new = (path.resolve() for path in (args.baseline, args.candidate))
    destination = args.output.resolve()
    if old == new or any(destination.is_relative_to(path) for path in (old, new)):
        parser.error("use distinct SDKs and an evidence directory outside both")
    destination.mkdir(parents=True)
    snapshots, verification = [], {}
    for label, path in (("baseline", old), ("candidate", new)):
        if label == "baseline" and args.baseline_predates_profile:
            verification[label] = {"profile_verified": False, "reason": "baseline predates the current Dev profile"}
        else:
            subprocess.run([sys.executable, TOOLS / "verify_dist.py", "--dist", path, "--stage", "02-cxx",
                            "--profile", "dev"], check=True, timeout=300)
            verification[label] = {"profile_verified": True}
        state = capture(path, readobj, nm)
        write(destination / (label + ".json"), state)
        snapshots.append(state)
    earlier, current = snapshots
    if any(earlier[key] != current[key] for key in ("target", "target_triple")):
        raise ValueError("compare SDKs for the same native target")
    delta = compare(earlier, current)
    write(destination / "comparison.json", delta)
    write(destination / "verification.json", verification)
    print(f"PE baseline: {len(earlier['binaries'])} -> {len(current['binaries'])} images and archives")
    print(", ".join(f"{key}={len(rows)}" for key, rows in delta.items()))
    print("See comparison.json for ABI review.")


if __name__ == "__main__":
    main()
