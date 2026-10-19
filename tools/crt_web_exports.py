#!/usr/bin/env python3
"""Public Web C ABI and private dependency export checks for SDK packaging."""
from pathlib import Path
import re
import shutil
import subprocess

from crt_pe import exported_symbols
from crt_elf import needed_libraries


def names(library: Path, target_os: str) -> set[str]:
    if target_os == "windows":
        return set(exported_symbols(library))
    nm = shutil.which("llvm-nm") or shutil.which("nm")
    if nm is None:
        raise ValueError("SDK ABI verification needs external LLVM/GNU nm")
    flags = ["-gU"] if target_os == "macos" else ["-D", "--defined-only"]
    result = subprocess.run([nm, *flags, str(library)], check=True, capture_output=True, text=True)
    result_names = set()
    for line in result.stdout.splitlines():
        columns = line.split()
        if len(columns) < 2 or columns[-2] not in ("T", "W", "D", "B", "R", "V", "i", "S", "A"):
            continue
        name = columns[-1].split("@", 1)[0]
        result_names.add(name[1:] if target_os == "macos" and name.startswith("_") else name)
    return result_names


def audit_exports(dist: Path, target_os: str) -> dict:
    include = dist / "include/crtweb"
    code = {file: re.sub(r"/\*.*?\*/|//[^\n]*", "", file.read_text(), flags=re.S)
            for file in include.glob("*.h")}
    forbidden = re.compile(r"\b(WK[A-Z]\w*|WPE\w*|WebKit\w*|JS[A-Z]\w*|GObject|GLib)\b")
    leaks = sorted({str(file.relative_to(dist)) + ": " + value for file, text in code.items()
                    for value in forbidden.findall(text)})
    owned = set(re.findall(r"\b(crtweb_\w+)\s*\(", code[include / "crtweb.h"]))
    library = dist / ("bin/libcrtweb.dll" if target_os == "windows" else
                      "lib/libcrtweb.dylib" if target_os == "macos" else "lib/libcrtweb.so")
    exported = names(library, target_os)
    result = {"header_leaks": leaks, "foreign_exports": sorted(exported - owned),
              "missing_exports": sorted(owned - exported), "public_functions": len(owned)}
    if target_os == "windows":
        from crt_pe import imported_libraries as pe_needed
        # Windows links WebKit's archive privately into libcrtweb.dll; no
        # libWebKit DLL/export-all boundary is introduced by packaging.
        result["private_curl_exports"] = sorted(name for name in exported if name.startswith("curl_"))
        result["colliding_curl_needed"] = [name for name in pe_needed(library) if "curl" in name.lower()]
    elif target_os in ("linux", "macos"):
        from crt_macho import dependencies
        private = dist / ("libexec/crtweb/lib/libWebKit.dylib" if target_os == "macos" else "libexec/crtweb/lib/libWebKit.so")
        result["private_curl_exports"] = sorted(name for name in names(private, target_os) if name.startswith("curl_"))
        needed = dependencies(private) if target_os == "macos" else needed_libraries(private)
        result["colliding_curl_needed"] = [name for name in needed if Path(name).name.startswith("libcurl")]
    result["passed"] = bool(owned) and not any(result.get(key) for key in (
        "header_leaks", "foreign_exports", "missing_exports", "private_curl_exports", "colliding_curl_needed"))
    return result
