#!/usr/bin/env python3
"""Actual shared-CRT link/run and missing-import negative control (Linux)."""
import argparse
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
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    scratch = output / "scratch"
    scratch.mkdir(exist_ok=True)
    env = dict(os.environ, CRT_SYSROOT=str(args.sdk_root.resolve()), CRT_TARGET_OS="linux",
               CRT_CXX_RUNTIME_LINKAGE="shared", CRT_CXX_HOSTED="1", TMPDIR=str(scratch))
    command = [str(args.wrapper.resolve())]
    program = output / "shared-loader-test"
    subprocess.run(command + [str(ROOT / "libstdc++/tests/shared_loader_import_test.cc"), "-o", str(program)], env=env, check=True)
    result = subprocess.run([str(program)], env=env, check=True, capture_output=True, text=True)
    if result.stdout.strip() != "shared_loader_import_test: ok":
        raise ValueError("TLS/destructor test did not report success")
    needed = needed_libraries(program)
    if "ld-linux-x86-64.so.2" not in needed and "ld-linux-aarch64.so.1" not in needed:
        raise ValueError("target loader is not recorded as a dependency")
    if any(name in needed for name in ("libc.so.6", "libstdc++.so.6", "libpthread.so.0")):
        raise ValueError("host C/C++ runtime leaked into the test")
    missing = output / "libmissing.so"
    subprocess.run(command + ["-shared", "-fPIC", str(ROOT / "libstdc++/tests/shared_loader_unresolved.cc"),
                              "-o", str(missing)], env=env, check=True)
    negative = subprocess.run(command + [str(ROOT / "libstdc++/tests/shared_loader_import_test.cc"),
        "-Wl,--no-as-needed", str(missing), "-Wl,--as-needed", "-o", str(output / "negative")],
        env=env, capture_output=True, text=True)
    (output / "negative.log").write_text(negative.stdout + negative.stderr)
    if negative.returncode == 0 or "crt_missing_loader_import_negative_control" not in negative.stderr:
        raise ValueError("unrelated missing shared imports were silently accepted")
    if list(scratch.glob("crt-loader-needed-*")):
        raise ValueError("link-only loader stub was not reclaimed")
    (output / "result.json").write_text(json.dumps({"passed": True, "needed": needed,
        "tls_thread_destructor": "pass", "missing_import_negative_control": "pass", "stub_cleanup": "pass"}, indent=2) + "\n")
    print("shared loader import: TLS/destructor pass; unrelated missing import rejected")


if __name__ == "__main__":
    main()
