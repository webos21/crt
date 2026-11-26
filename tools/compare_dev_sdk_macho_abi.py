#!/usr/bin/env python3
"""Capture and compare macOS Dev SDK link surfaces and Mach-O runtime imports.

The macOS counterpart of compare_dev_sdk_elf_abi.py: the same snapshot,
comparison and evidence files, over Mach-O images and static archives. It uses
Apple's nm and otool (or llvm-nm and llvm-otool passed explicitly).
"""

import argparse
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

import crt_macho
from compare_dev_sdk_elf_abi import compare, output, write
from crt_sdk_install_metadata import content_sha256, file_digest, inventory

CPU_TYPE_ARM64 = 0x0100000c
MH_OBJECT = 1
FILE_TYPES = {MH_OBJECT: "object", 2: "execute", 6: "dylib", 8: "bundle"}
FAT_MAGICS = (b"\xca\xfe\xba\xbe", b"\xca\xfe\xba\xbf")


def architectures(path, lipo):
    return output([lipo, "-archs", path]).split()


def static_symbols(path, nm, arch=None):
    """Externally defined names and nm types of one architecture's members."""
    argv = [nm, "-P", "-g", "-U", *(["-arch", arch] if arch else []), path]
    rows = set()
    for line in output(argv).splitlines():
        if not line or line.endswith(":"):
            continue
        fields = line.split()
        if len(fields) < 2:
            raise ValueError(f"unexpected nm row in {path}: {line}")
        rows.add((fields[0], fields[1]))
    return [list(row) for row in sorted(rows)]


def parse_symbol_row(line):
    """Split one `nm -m` row into (name, qualifiers, providing library)."""
    match = re.match(r"^(?:[0-9a-fA-F]+)?\s*(\(.*?\))\s+(.*)$", line)
    if not match:
        raise ValueError(f"unexpected nm -m row: {line}")
    section, rest = match.groups()
    provider = None
    source = re.search(r" \((?:from ([^()]+)|dynamically looked up)\)$", rest)
    if source:
        provider = source.group(1) or "dynamic lookup"
        rest = rest[:source.start()]
    indirect = re.search(r" \(for (\S+)\)$", rest)
    if indirect:
        provider = "re-export of " + indirect.group(1)
        rest = rest[:indirect.start()]
    words = rest.split()
    if not words:
        raise ValueError(f"unnamed nm -m row: {line}")
    return words[-1], " ".join([section, *words[:-1]]), provider


def dynamic_symbols(path, nm):
    exports = set()
    for line in output([nm, "-m", "-g", "-U", path]).splitlines():
        if line.strip():
            name, qualifiers, provider = parse_symbol_row(line)
            exports.add((name, qualifiers, provider or ""))
    imports = set()
    for line in output([nm, "-m", "-u", path]).splitlines():
        if line.strip():
            name, qualifiers, provider = parse_symbol_row(line)
            imports.add((name, qualifiers, provider or ""))
    return [list(row) for row in sorted(exports)], [list(row) for row in sorted(imports)]


def parse_load_commands(text):
    """Version fields `crt_macho` does not expose, from `otool -l` output."""
    result = {"platform": None, "minimum_os": None, "sdk": None, "dylib_versions": {}}
    command, name, current = None, None, None
    for line in text.splitlines():
        words = line.split()
        if len(words) < 2:
            continue
        if words[0] == "cmd":
            command, name, current = words[1], None, None
        elif command == "LC_BUILD_VERSION" and words[0] in ("platform", "minos", "sdk"):
            result[{"platform": "platform", "minos": "minimum_os", "sdk": "sdk"}[words[0]]] = words[1]
        elif command and command.endswith("_DYLIB"):
            if words[0] == "name":
                name = words[1]
            elif words[:2] == ["current", "version"]:
                current = words[2]
            elif words[:2] == ["compatibility", "version"]:
                result["dylib_versions"][f"{command} {name}"] = {"current": current, "compatibility": words[2]}
    return result


def capture(root, nm, otool, lipo):
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    profile = json.loads((root / "share/crt/contracts/development.json").read_text(encoding="utf-8"))
    target = manifest["target"]
    if manifest["stage"] != "02-cxx" or profile["profile"] != "dev" or target != {"os": "macos", "arch": "aarch64"}:
        raise ValueError("expected a macOS/arm64 Development SDK")
    rows = inventory(root)
    binaries, aliases = {}, {}
    for relative, kind, _ in rows:
        path = root / relative
        if kind == "symlink":
            continue
        with path.open("rb") as stream:
            header = stream.read(16)
        magic = header[:4]
        if header[:8] == b"!<arch>\n" or path.suffix == ".a":
            archs = architectures(path, lipo)
            if "arm64" not in archs:
                raise ValueError(f"archive has no arm64 members: {relative}")
            binaries[relative] = {"sha256": file_digest(path), "architectures": sorted(archs),
                                  "static_symbols": static_symbols(path, nm, "arm64")}
        # A Java class file shares the fat magic; its second word is a large
        # version number, while a universal binary lists only a few slices.
        elif magic in FAT_MAGICS and int.from_bytes(header[4:8], "big") < 32:
            raise ValueError(f"unexpected universal image: {relative}")
        elif len(header) >= 16 and struct.unpack("<I", magic)[0] == crt_macho.MH_MAGIC_64:
            cpu, _, filetype = struct.unpack_from("<III", header, 4)
            if cpu != CPU_TYPE_ARM64:
                raise ValueError(f"foreign Mach-O image: {relative}")
            if filetype not in FILE_TYPES:
                raise ValueError(f"unexpected Mach-O type {filetype}: {relative}")
            commands = parse_load_commands(output([otool, "-l", path]))
            image = {"sha256": file_digest(path), "file_type": FILE_TYPES[filetype], **commands}
            if filetype == MH_OBJECT:  # Relocatable startup objects.
                image["static_symbols"] = static_symbols(path, nm)
            else:
                image["exports"], image["imports"] = dynamic_symbols(path, nm)
                image["needed"] = crt_macho.dependencies(path)
                image["install_name"] = crt_macho.dylib_id(path)
                image["runtime_paths"] = crt_macho.rpaths(path)
            binaries[relative] = image
        elif magic in (b"\xce\xfa\xed\xfe", b"\xfe\xed\xfa\xce", b"\xfe\xed\xfa\xcf"):
            raise ValueError(f"unexpected 32-bit or big-endian Mach-O: {relative}")
    for relative, kind, value in rows:
        if kind == "symlink":
            resolved = (root / relative).resolve().relative_to(root).as_posix()
            if resolved in binaries:
                aliases[relative] = value
    if not binaries:
        raise ValueError("SDK contains no Mach-O artifacts")
    return {
        "target": target, "target_triple": manifest["target_triple"],
        "version": profile["version"], "producer_source_commit": profile["producer_source_commit"],
        "content_sha256": content_sha256(rows), "binaries": binaries, "aliases": aliases,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("baseline", "candidate", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--nm", default="nm")
    parser.add_argument("--otool", default="otool")
    parser.add_argument("--lipo", default="lipo")
    parser.add_argument("--baseline-predates-profile", action="store_true",
                        help="skip the current Dev profile verification of the BASELINE only, for a published SDK that "
                             "predates the profile's newer capability records; the candidate is always verified and the "
                             "skip is recorded in verification.json")
    args = parser.parse_args()
    nm, otool, lipo = (shutil.which(tool) for tool in (args.nm, args.otool, args.lipo))
    if not nm or not otool or not lipo:
        parser.error("nm, otool and lipo are required")
    old, new = (path.resolve() for path in (args.baseline, args.candidate))
    destination = args.output.resolve()
    if old == new or any(destination.is_relative_to(path) for path in (old, new)):
        parser.error("use distinct SDKs and an evidence directory outside both")
    destination.mkdir(parents=True)
    snapshots = []
    verification = {}
    for label, path in (("baseline", old), ("candidate", new)):
        if label == "baseline" and args.baseline_predates_profile:
            # The baseline keeps the verification it passed when it was published; only its Mach-O surface is compared.
            verification[label] = {"profile_verified": False, "reason": "baseline predates the current Dev profile"}
        else:
            subprocess.run([sys.executable, Path(__file__).resolve().parent / "verify_dist.py",
                            "--dist", path, "--stage", "02-cxx", "--profile", "dev"], check=True, timeout=120)
            verification[label] = {"profile_verified": True}
        state = capture(path, nm, otool, lipo)
        write(destination / (label + ".json"), state)
        snapshots.append(state)
    earlier, current = snapshots
    if any(earlier[key] != current[key] for key in ("target", "target_triple")):
        raise ValueError("compare SDKs for the same native target")
    delta = compare(earlier, current)
    write(destination / "comparison.json", delta)
    write(destination / "verification.json", verification)
    print(f"Mach-O baseline: {len(earlier['binaries'])} -> {len(current['binaries'])} images and archives")
    print(", ".join(f"{key}={len(rows)}" for key, rows in delta.items()))
    print("See comparison.json for ABI review.")


if __name__ == "__main__":
    main()
