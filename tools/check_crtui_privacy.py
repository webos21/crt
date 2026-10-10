#!/usr/bin/env python3
"""Prove that LVGL is a private dependency of crtui (docs/acceptance/crtui_acceptance.md,
Tranche 3).

Checks, each independently:

  headers   no installed crtui header mentions LVGL (an `lvgl` include or any
            `lv_` identifier), and every function a header declares is exported
  exports   the shared library exports only crtui_* symbols -- no lv_* (LVGL's
            API) and nothing else LVGL bundles -- and exports every declared
            crtui_* function
  objects   the sample application's object files reference no lv_* symbol, so
            the application "calls no lv_* symbol" in the literal sense
  sources   the sample application's source includes no LVGL header and names no
            lv_* identifier

Exports are read with llvm-readobj (Windows PE), llvm-nm -D (ELF) or llvm-nm -g
(Mach-O), from the LLVM tools directory given with --llvm-bin.
"""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

LV_NAME = re.compile(r"^_?lv_", re.IGNORECASE)
DECLARED = re.compile(r"^CRTUI_API\s+\w+\s+(crtui_\w+)\s*\(", re.MULTILINE)
LV_IDENT = re.compile(r"\blv_[a-z0-9_]+|\bLV_[A-Z0-9_]+|lvgl", re.IGNORECASE)


def tool(llvm_bin: Path, name: str) -> str:
    for candidate in (llvm_bin / name, llvm_bin / f"{name}.exe"):
        if candidate.is_file():
            return str(candidate)
    # Apple's /usr/bin/clang is a shim: the LLVM binutils live in the Xcode
    # toolchain, reachable through PATH or `xcrun`, not next to the compiler.
    found = shutil.which(name)
    if found is None and sys.platform == "darwin":
        result = subprocess.run(["xcrun", "--find", name], capture_output=True, text=True)
        found = result.stdout.strip() if result.returncode == 0 else None
    if found is None:
        # Debian/Ubuntu install only versioned names (llvm-nm-21) in /usr/bin
        # and unversioned ones under /usr/lib/llvm-N/bin.
        versioned = sorted(Path("/usr/bin").glob(f"{name}-[0-9]*"), reverse=True)
        versioned += sorted(Path("/usr/lib").glob(f"llvm-*/bin/{name}"), reverse=True)
        found = str(versioned[0]) if versioned else None
    if found:
        return found
    raise SystemExit(f"{name} not found in {llvm_bin}")


def run(command: list[str]) -> str:
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def exported_symbols(library: Path, llvm_bin: Path) -> list[str]:
    magic = library.read_bytes()[:4]
    if magic[:2] == b"MZ":
        text = run([tool(llvm_bin, "llvm-readobj"), "--coff-exports", str(library)])
        return [line.split("Name:", 1)[1].strip() for line in text.splitlines() if "Name:" in line]
    if magic == b"\x7fELF":
        text = run([tool(llvm_bin, "llvm-nm"), "-D", "--defined-only", str(library)])
    else:  # Mach-O
        text = run([tool(llvm_bin, "llvm-nm"), "-g", "--defined-only", str(library)])
    names = []
    for line in text.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            names.append(parts[-1].split("@")[0].lstrip("_") if magic != b"\x7fELF" else parts[-1].split("@")[0])
    return names


def object_symbols(path: Path, llvm_bin: Path) -> list[str]:
    text = run([tool(llvm_bin, "llvm-nm"), str(path)])
    names = []
    for line in text.splitlines():
        parts = line.split()
        if parts:
            names.append(parts[-1])
    return names


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--llvm-bin", required=True, type=Path)
    parser.add_argument("--headers", required=True, type=Path, help="directory containing crtui/*.h")
    parser.add_argument("--shared", type=Path, help="the shared crtui library to inspect")
    parser.add_argument("--object", action="append", default=[], type=Path,
                        help="an object file of the sample application (repeatable)")
    parser.add_argument("--example-source", type=Path)
    args = parser.parse_args()

    failures: list[str] = []
    declared: set[str] = set()

    header_dir = args.headers / "crtui"
    headers = sorted(header_dir.glob("*.h"))
    if not headers:
        raise SystemExit(f"no crtui headers under {header_dir}")
    for header in headers:
        text = header.read_text(encoding="utf-8")
        # Comments may *explain* that LVGL is private, so only code is scanned for
        # an actual LVGL include; identifiers are scanned with comments removed.
        code = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
        code = re.sub(r"//[^\n]*", "", code)
        if re.search(r"#\s*include\s*[<\"][^>\"]*lv", code, re.IGNORECASE) or re.search(r"\blv_[a-z0-9_]+", code):
            failures.append(f"{header.name}: LVGL leaks into the public header")
        declared.update(DECLARED.findall(text))
    if len(declared) < 30:
        failures.append(f"only {len(declared)} CRTUI_API functions found in the headers")

    export_count = 0
    lvgl_exports = 0
    if args.shared is not None:
        names = exported_symbols(args.shared, args.llvm_bin)
        export_count = len(names)
        lvgl_exports = sum(1 for n in names if LV_NAME.match(n))
        extras = sorted(n for n in names if not n.lstrip("_").startswith("crtui_"))
        if lvgl_exports:
            failures.append(f"{args.shared.name} exports {lvgl_exports} lv_* symbols")
        if extras:
            failures.append(f"{args.shared.name} exports non-crtui symbols: {extras[:8]}")
        missing = sorted(d for d in declared if d not in {n.lstrip('_') for n in names})
        if missing:
            failures.append(f"declared but not exported: {missing[:8]}")

    for obj in args.object:
        leaked = [s for s in object_symbols(obj, args.llvm_bin) if LV_NAME.match(s)]
        if leaked:
            failures.append(f"{obj.name} references lv_* symbols: {leaked[:5]}")

    if args.example_source is not None:
        source = args.example_source.read_text(encoding="utf-8")
        hits = sorted(set(LV_IDENT.findall(source)))
        if hits:
            failures.append(f"{args.example_source.name} names LVGL: {hits[:5]}")

    if failures:
        for failure in failures:
            print(f"crtui_privacy: FAIL {failure}", file=sys.stderr)
        raise SystemExit(1)
    print(f"crtui_privacy: ok declared={len(declared)} exports={export_count} lvgl_exports={lvgl_exports} "
          f"objects={len(args.object)} headers={len(headers)}")


if __name__ == "__main__":
    main()
