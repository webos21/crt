#!/usr/bin/env python3
"""Opt-in host-side runner for libc/tests/malloc_contention_baseline_test.c.

TODO.md's "Allocator baseline validation before Upper Runtime", tranche 3
("Scale contention deliberately"): drive the in-tree
malloc_contention_baseline_test binary across the 1/8/16/32-thread sweep
tranche 3 calls for, in both the "private" (each thread only ever touches
its own allocations) and "shared" (all threads draw from and return to one
mutex-guarded pool, so a pointer routinely gets freed/reallocated by a
different thread than allocated it) access patterns -- see that test's own
top comment for why both patterns matter and what "shared" actually
exercises given this allocator's single global heap_lock.

Usage:
    python tools/run_allocator_contention_baseline.py --build-dir out/<preset>

Each (threads, pattern) combination's result (the binary's own JSON line,
plus this script's own wall_seconds field) is appended to a JSON-lines
file under --out-dir. By default that is
benchmark/allocator-contention/<os>/ at the repo root (windows/linux/
macos), checked into git for the same reason
tools/run_allocator_baseline.py's own matching default is -- see
benchmark/README.md.
"""

import argparse
import json
import platform
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

DEFAULT_SEED = 42
# Kept modest deliberately: at 32 threads this is already 160,000 total
# operations for the "shared" pattern, which -- unlike "private" -- fully
# serializes on g_shared_mutex (see the test's own top comment), so wall
# time grows with total ops regardless of thread count. Raise this
# explicitly for a heavier run; do not raise the default itself without a
# reason, given tranche 1's own not-yet-root-caused wall-clock-vs-
# elapsed_ns anomaly at large op counts (HISTORY.md, 2026-09-16).
DEFAULT_OPS_PER_THREAD = 5_000
THREAD_COUNTS = (1, 8, 16, 32)
PATTERNS = ("private", "shared")


def benchmark_os_name() -> str:
    system = platform.system().lower()
    return "macos" if system == "darwin" else system


def default_out_dir() -> Path:
    repo_root = Path(__file__).resolve().parent.parent
    return repo_root / "benchmark" / "allocator-contention" / benchmark_os_name()


def find_binary(build_dir: Path) -> Path:
    candidates = (
        build_dir / "libc" / "tests" / "malloc_contention_baseline_test.exe",
        build_dir / "libc" / "tests" / "malloc_contention_baseline_test",
    )
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise SystemExit(
        f"malloc_contention_baseline_test was not found under {build_dir}/libc/tests -- "
        "build it first, e.g. `cmake --build {build_dir} --target malloc_contention_baseline_test`"
    )


def run_case(binary: Path, threads: int, ops: int, seed: int, pattern: str) -> tuple[dict, float]:
    start = time.monotonic()
    result = subprocess.run(
        [str(binary), "--threads", str(threads), "--ops", str(ops), "--seed", str(seed), "--pattern", pattern, "--json"],
        capture_output=True,
        text=True,
        check=False,
    )
    wall_seconds = time.monotonic() - start

    if result.returncode != 0:
        raise RuntimeError(
            f"malloc_contention_baseline_test --threads {threads} --pattern {pattern} failed "
            f"(exit {result.returncode}): {result.stderr.strip()}"
        )

    line = result.stdout.strip().splitlines()[-1] if result.stdout.strip() else ""
    try:
        record = json.loads(line)
    except json.JSONDecodeError as exc:
        raise RuntimeError(
            f"malloc_contention_baseline_test --threads {threads} --pattern {pattern} did not print valid JSON: {exc}\n"
            f"stdout: {result.stdout!r}\nstderr: {result.stderr!r}"
        ) from exc

    return record, wall_seconds


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", required=True, type=Path, help="a configured CMake build directory, e.g. out/<preset>")
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED, help="deterministic seed passed to every case (default: %(default)s)")
    parser.add_argument(
        "--ops-per-thread",
        type=int,
        default=DEFAULT_OPS_PER_THREAD,
        help="operations each thread performs in every case (default: %(default)s)",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="default: benchmark/allocator-contention/<os>/ at the repo root (see benchmark/README.md)",
    )
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    binary = find_binary(build_dir)
    out_dir = (args.out_dir or default_out_dir()).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    started_at = datetime.now(timezone.utc)
    arch = platform.machine().lower()
    out_path = out_dir / f"malloc_contention_baseline-{arch}-seed{args.seed}-{started_at.strftime('%Y%m%dT%H%M%SZ')}.jsonl"

    print(f"malloc_contention_baseline binary: {binary}")
    print(f"host: {platform.system()} {platform.machine()}")
    print(f"seed: {args.seed}  ops per thread: {args.ops_per_thread}")
    print(f"results: {out_path}")
    print()

    records = []
    with out_path.open("w", encoding="utf-8") as out_file:
        for pattern in PATTERNS:
            for threads in THREAD_COUNTS:
                print(f"--- threads={threads} pattern={pattern} ---")
                try:
                    record, wall_seconds = run_case(binary, threads, args.ops_per_thread, args.seed, pattern)
                except RuntimeError as exc:
                    print(f"FAILED: {exc}", file=sys.stderr)
                    out_file.write(
                        json.dumps({"threads": threads, "pattern": pattern, "seed": args.seed, "error": str(exc)}) + "\n"
                    )
                    return 1

                record["wall_seconds"] = wall_seconds
                record["host_system"] = platform.system()
                record["host_machine"] = platform.machine()
                record["started_at"] = started_at.isoformat()
                records.append(record)
                out_file.write(json.dumps(record) + "\n")
                out_file.flush()

                print(
                    f"elapsed_ns={record['elapsed_ns']} wall_seconds={wall_seconds:.3f} "
                    f"throughput_ops_per_sec={record['throughput_ops_per_sec']:.1f} "
                    f"p50_ns={record['latency_ns']['p50']} p99_ns={record['latency_ns']['p99']} "
                    f"peak_live_bytes={record['live']['peak_bytes']}"
                )

    print(f"\nWrote {len(records)} case result(s) to {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
