#!/usr/bin/env python3
"""Audit which library each Mach-O image of a CRT macOS SDK binds the CRT libc symbols to.

Mach-O two-level namespace binds an undefined symbol to the first dylib on the link line that defines it. A CRT
dylib or executable that binds a symbol the CRT libc/libm/libdl exports (malloc, pthread_*, clock_gettime, ...)
to Apple's libSystem instead gets Darwin's implementation of a function whose ABI the CRT does not share (a
64-byte pthread_mutex_t, a different CLOCK_MONOTONIC id, a second malloc registry). Two checks:

* binding: every undefined reference to a CRT-owned symbol is bound to libc/libm/libdl, except entries of
  HOST_BOUNDARY (each with its reason);
* a statically linked executable may bind pure math (symbols only libm.dylib exports) to Apple's libm: same ABI,
  no state, reported as a note;
* mixed runtime: an executable must not carry its own static libc (its own malloc) while loading CRT dylibs that
  bind to libc.dylib: that is two allocator and pthread registries in one process, except MIXED_EXECUTABLES.

Decisions: docs/bringup/crt_pal_residual_triage.md (macOS native binding audit). Run on macOS only (nm, otool).
"""

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

CRT_CORE_LIBRARIES = ("libc", "libm", "libdl")

# (image, symbol) -> why the host binding is intended.
HOST_BOUNDARY = {
    ("lib/libc.dylib", "_dlsym"):
        "libc reaches the real Darwin getifaddrs/pthread functions with dlsym(RTLD_NEXT); libdl depends on libc, "
        "so this cannot be the CRT's own dlsym",
}
HOST_BOUNDARY[("examples/bin/crtgfx_skia_gpu_window_demo", "_dlsym")] = (
    "in-tree developer SDK only: the static link resolves Skia's dlsym against a framework before libdl.a is "
    "pulled while its dlopen is the CRT's; the Metal path loads no library, so no handle crosses; not reproduced "
    "as a failure (the isolated-stage rebuild of this example binds both to the CRT libdl)")
# image -> {symbol: reason}: whole-image host bindings (symbols are listed so a new one is a failure).
HOST_BOUNDARY_IMAGES = {
    "lib/libcrtgfx.dylib": (
        {"_calloc", "_clock_gettime", "_free", "_malloc", "_memcpy", "_realloc"},
        "the macOS window backend (libcrtgfx/src/arch/macos/window_cocoa.c) deliberately uses the host allocator for "
        "buffers it hands to CoreGraphics and frees in its own release callback; no allocation crosses the public "
        "API, and crtgfx_now_ms() accepts both clock ids"),
}
# executable -> reason it may carry a static libc next to CRT dylibs.
MIXED_EXECUTABLES = {
    "bin/curl": "the curl tool links the static libc.a with the shared libcurl/mbedTLS/zlib; 40 rounds of HTTP "
                "transfers were clean, but it is a latent two-registry link (see the triage record)",
}

SKIP_PARTS = ("/relink/", "/share/crt/")
SKIP_SUFFIXES = (".a", ".o", ".h", ".txt", ".json", ".py", ".md", ".cmake", ".c", ".cc", ".cpp")
UNDEFINED = re.compile(r"\s*\(undefined\) (?:weak )?external (\S+)(?: \(from (.+)\))?$")


def run(*command: str, check: bool = False) -> str:
    return subprocess.run(command, capture_output=True, text=True, check=check).stdout


def is_macho(path: Path) -> bool:
    try:
        with path.open("rb") as stream:
            return stream.read(4) in (b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe")
    except OSError:
        return False


def defined_symbols(nm_output: str) -> set:
    names = set()
    for line in nm_output.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[-2] in ("T", "D", "S", "B", "A"):
            names.add(parts[-1])
    return names


def undefined_bindings(nm_output: str) -> list:
    """(symbol, providing library or None) for each undefined reference of ``nm -m -u``."""
    result = []
    for line in nm_output.splitlines():
        match = UNDEFINED.match(line.strip())
        if match:
            result.append((match.group(1), match.group(2)))
    return result


def owned_symbols(sdk: Path) -> dict:
    """CRT-owned symbol -> the CRT library that exports it (libc wins when more than one does)."""
    owned = {}
    for name in reversed(CRT_CORE_LIBRARIES):
        library = sdk / "lib" / f"{name}.dylib"
        if library.is_file():
            for symbol in defined_symbols(run("nm", "-gU", str(library))):
                owned[symbol] = name
    return owned


def collect_image(path: Path, name: str, owned: dict) -> dict:
    """Inspect an SDK image or an external consumer against the SDK's symbols."""
    if not is_macho(path):
        raise ValueError(f"not a Mach-O image: {path}")
    dependencies = [line.split()[0] for line in run("otool", "-L", str(path), check=True).splitlines()[1:] if line.strip()]
    header = run("file", "-b", str(path), check=True)
    kind = "dylib" if "shared library" in header else "exe" if "executable" in header else "other"
    bound, owners = {}, {}
    for symbol, provider in undefined_bindings(run("nm", "-m", "-u", str(path), check=True)):
        if symbol in owned:
            bound.setdefault(provider or "flat", set()).add(symbol)
            owners.setdefault(symbol, owned[symbol])
    return {
        "image": name, "kind": kind, "dependencies": dependencies,
        "owns_malloc": "_malloc" in defined_symbols(run("nm", "-gU", str(path), check=True)),
        "bound": bound, "math_only": {symbol for symbol, owner in owners.items() if owner == "libm"}}


def collect(sdk: Path) -> list:
    owned = owned_symbols(sdk)
    if not owned:
        raise SystemExit(f"{sdk}: no CRT libc.dylib to compare against (is this a macOS SDK?)")
    images = []
    for root, _, files in os.walk(sdk):
        posix_root = Path(root).as_posix() + "/"
        if any(part in posix_root for part in SKIP_PARTS):
            continue
        for name in sorted(files):
            path = Path(root) / name
            if path.is_symlink() or not path.is_file() or path.suffix in SKIP_SUFFIXES or not is_macho(path):
                continue
            images.append(collect_image(path, path.relative_to(sdk).as_posix(), owned))
    return sorted(images, key=lambda item: item["image"])


def problems(images: list, host_boundary=None, host_images=None, mixed=None, notes=None) -> list:
    """Failures of the binding and mixed-runtime checks over ``collect()`` output; ``notes`` (a list) receives
    the tolerated pure-math bindings of executables."""
    host_boundary = HOST_BOUNDARY if host_boundary is None else host_boundary
    host_images = HOST_BOUNDARY_IMAGES if host_images is None else host_images
    mixed = MIXED_EXECUTABLES if mixed is None else mixed
    found = []
    for item in images:
        name = item["image"]
        for provider, symbols in sorted(item["bound"].items()):
            if provider in CRT_CORE_LIBRARIES:
                continue
            allowed = set(host_images.get(name, (set(), ""))[0])
            allowed |= {symbol for (image, symbol) in host_boundary if image == name}
            unexpected = sorted(set(symbols) - allowed)
            if item["kind"] == "exe" and unexpected and set(unexpected) <= set(item.get("math_only", ())):
                if notes is not None:
                    notes.append(f"{name}: {len(unexpected)} pure math symbol(s) bound to {provider}")
                continue
            if unexpected:
                found.append(f"{name}: {len(unexpected)} CRT libc symbol(s) bound to {provider}, not to the CRT "
                             f"libc: {', '.join(unexpected[:6])}{' ...' if len(unexpected) > 6 else ''}")
        if (item["kind"] == "exe" and item["owns_malloc"] and name not in mixed and
                any(dep.startswith("@rpath/lib") and not dep.startswith("@rpath/libc++") for dep in item["dependencies"])):
            found.append(f"{name}: carries its own static libc (malloc) and loads CRT dylibs that bind to "
                         "libc.dylib: two allocator registries in one process; link the shared libc "
                         "(CRT_CXX_RUNTIME_LINKAGE=shared or libc.dylib first)")
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("sdk", type=Path)
    parser.add_argument("--json", type=Path, help="write the per-image evidence here")
    args = parser.parse_args()
    if sys.platform != "darwin":
        raise SystemExit("audit_macos_bindings.py needs macOS (nm, otool)")
    images = collect(args.sdk.resolve())
    if args.json:
        args.json.write_text(json.dumps(
            [{**item, "bound": {k: sorted(v) for k, v in item["bound"].items()},
               "math_only": sorted(item["math_only"])} for item in images], indent=1),
            encoding="utf-8")
    notes = []
    failures = problems(images, notes=notes)
    for note in notes:
        print("NOTE", note)
    print(f"audited {len(images)} Mach-O images ({sum(1 for i in images if i['kind'] == 'dylib')} dylibs, "
          f"{sum(1 for i in images if i['kind'] == 'exe')} executables)")
    for failure in failures:
        print("FAIL", failure)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
