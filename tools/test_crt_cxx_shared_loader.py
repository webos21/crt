#!/usr/bin/env python3
"""Actual shared-CRT link/run and missing-import negative control (Linux)."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

from crt_elf import needed_libraries

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--wrapper", type=Path, default=ROOT / "tools/crt-c++")
    parser.add_argument("--loader", action="append", default=[], type=Path,
                        help="also execute the same ELF with this interpreter (repeatable)")
    parser.add_argument("--host-library", type=Path,
                        help="also test with this GNU-libc-linked host DSO in the global scope")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    scratch = output / "scratch"
    scratch.mkdir(exist_ok=True)
    env = dict(os.environ, CRT_SYSROOT=str(args.sdk_root.resolve()), CRT_TARGET_OS="linux",
               CRT_CXX_RUNTIME_LINKAGE="shared", CRT_CXX_HOSTED="1", TMPDIR=str(scratch))
    command = [str(args.wrapper.resolve()), "-std=c++17", "-fexceptions", "-frtti"]
    library = output / "libcrt_tls_regression.so"
    subprocess.run(command + ["-shared", "-fPIC", str(ROOT / "libstdc++/tests/shared_loader_tls.cc"),
                              "-Wl,-soname," + library.name, "-o", str(library)], env=env, check=True)
    program = output / "shared-loader-test"
    inputs = [str(ROOT / "libstdc++/tests/shared_loader_import_test.cc"), str(library), "-Wl,-rpath,$ORIGIN"]
    subprocess.run(command + inputs + ["-o", str(program)], env=env, check=True)
    executions = []
    for loader in [None, *args.loader]:
        argv = [str(program)] if loader is None else [str(loader.resolve()), "--library-path",
            os.pathsep.join((str(args.sdk_root.resolve() / "lib"), str(output))), str(program)]
        result = subprocess.run(argv, env=env, check=True, capture_output=True, text=True, timeout=30)
        if result.stdout.strip() != "shared_loader_import_test: ok":
            raise ValueError("TLS/destructor/exception/calloc test did not report success")
        executions.append({"loader": str(loader.resolve()) if loader else "PT_INTERP",
            "loader_sha256": hashlib.sha256(loader.read_bytes()).hexdigest() if loader else None,
            "stdout": result.stdout.strip(), "passed": True})
    needed = needed_libraries(program)
    if any(name.startswith("ld-linux-") for name in needed):
        raise ValueError("explicit loader dependency regresses glibc 2.39 startup")
    if any(name in needed for name in ("libc.so.6", "libstdc++.so.6", "libpthread.so.0")):
        raise ValueError("host C/C++ runtime leaked into the test")
    for image in (library, args.sdk_root / "lib/libc++abi.so", args.sdk_root / "lib/libc++.so.1"):
        symbols = subprocess.check_output(["readelf", "--dyn-syms", "--wide", str(image)], text=True)
        if any("UND" in line and "__tls_get_addr" in line for line in symbols.splitlines()):
            raise ValueError(f"GD TLS import remains; rebuild with TLSDESC: {image}")
        if any("UND" in line and "__cxa_thread_atexit_impl" in line for line in symbols.splitlines()):
            raise ValueError(f"host thread-exit registry can capture CRT destructors: {image}")
    relocations = subprocess.check_output(["readelf", "--relocs", "--wide", str(library)], text=True)
    if "TLSDESC" not in relocations:
        raise ValueError("the shared-library fixture did not exercise a TLS descriptor")
    if args.host_library:
        mixed = output / "shared-loader-host-test"
        subprocess.run(command + inputs + ["-Wl,--no-as-needed", str(args.host_library.resolve()),
            "-Wl,--as-needed", "-o", str(mixed)], env=env, check=True)
        result = subprocess.run([str(mixed)], env=env, check=True, capture_output=True, text=True, timeout=30)
        if result.stdout.strip() != "shared_loader_import_test: ok":
            raise ValueError("host DSO captured CRT's thread-local destructor registration")
        executions.append({"loader": "PT_INTERP", "host_library": str(args.host_library.resolve()),
            "stdout": result.stdout.strip(), "passed": True})
    missing = output / "libmissing.so"
    subprocess.run(command + ["-shared", "-fPIC", str(ROOT / "libstdc++/tests/shared_loader_unresolved.cc"),
                              "-o", str(missing)], env=env, check=True)
    negative = subprocess.run(command + inputs + [
        "-Wl,--no-as-needed", str(missing), "-Wl,--as-needed", "-o", str(output / "negative")],
        env=env, capture_output=True, text=True)
    (output / "negative.log").write_text(negative.stdout + negative.stderr)
    if negative.returncode == 0 or "crt_missing_loader_import_negative_control" not in negative.stderr:
        raise ValueError("unrelated missing shared imports were silently accepted")
    if list(scratch.glob("crt-loader-needed-*")):
        raise ValueError("link-only loader stub was not reclaimed")
    (output / "result.json").write_text(json.dumps({"passed": True, "needed": needed,
        "executions": executions, "calloc_tls_thread_destructor_exception": "pass",
        "missing_import_negative_control": "pass", "stub_cleanup": "pass"}, indent=2) + "\n")
    print("shared loader: calloc/TLSDESC/thread/destructor/exception pass; unrelated missing import rejected")


if __name__ == "__main__":
    main()
