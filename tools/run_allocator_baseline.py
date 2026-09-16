#!/usr/bin/env python3
"""Opt-in host-side runner for libc/tests/malloc_baseline_test.c.

TODO.md's "Allocator baseline validation before Upper Runtime", tranche 1
("Build a deterministic workload and result format"): drive the in-tree
malloc_baseline_test binary at the 10^3/10^4/10^5/10^6 operation-count
tiers tranche 1 calls for, stopping before a tier whose predecessor already
exceeded the declared wall-clock time budget ("where the lower tiers
finish within the declared time budget, 10^6" -- this script is what
decides that, not the binary itself, since only the host side can bound
wall time without also polluting the binary's own in-process elapsed_ns
measurement). Not wired into ctest: this is the long/opt-in half tranche 3
calls for, kept separate from the bounded correctness variant CTest itself
runs (malloc_baseline_test with no arguments).

This script also samples each tier's own peak host-observed resident
memory (tranche 4, "measure fragmentation and resident memory outside the
CRT ABI") via tools/host_rss.py -- see that module's own top comment for
why this lives here, on the host side, rather than in the Bionic-facing
binary itself. The result's `live.bytes`/`live.usable_bytes` (both from
the binary's own accounting) are the allocator's internal view of
requested vs. allocator-visible-usable size; `host_peak_rss_bytes` is the
OS's own outside view of the whole process, including but not limited to
the heap -- comparing the two across tiers is what actually answers
"retained arenas that get reused" vs. "continually growing resident
memory" (this workload's `os_regions` field, from tranche 1, is the
allocator-internal half of that same question).

Usage:
    python tools/run_allocator_baseline.py --build-dir out/<preset>

Each tier's result (the binary's own JSON line, plus this script's own
wall_seconds/tier fields) is appended to a JSON-lines file under --out-dir.
By default that is benchmark/allocator-baseline/<os>/ at the repo root
(windows/linux/macos) -- checked into git deliberately, not under out/, so
results from each host this project supports accumulate in one place for
direct comparison rather than living only in one contributor's local build
tree. See benchmark/README.md. Pass --out-dir to redirect a one-off run
elsewhere instead. Every file is named by seed/arch/start time so repeat
runs never clobber each other.
"""

import argparse
import json
import platform
import sys
from datetime import datetime, timezone
from pathlib import Path

from host_rss import run_with_peak_rss

DEFAULT_SEED = 42
DEFAULT_TIME_BUDGET_SECONDS = 60.0
TIERS = (1_000, 10_000, 100_000, 1_000_000)


def benchmark_os_name() -> str:
    system = platform.system().lower()
    return "macos" if system == "darwin" else system


def default_out_dir() -> Path:
    # tools/ is always a direct child of the repo root in this project.
    repo_root = Path(__file__).resolve().parent.parent
    return repo_root / "benchmark" / "allocator-baseline" / benchmark_os_name()


def find_binary(build_dir: Path) -> Path:
    candidates = (
        build_dir / "libc" / "tests" / "malloc_baseline_test.exe",
        build_dir / "libc" / "tests" / "malloc_baseline_test",
    )
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise SystemExit(
        f"malloc_baseline_test was not found under {build_dir}/libc/tests -- "
        "build it first, e.g. `cmake --build {build_dir} --target malloc_baseline_test`"
    )


def run_tier(binary: Path, ops: int, seed: int) -> tuple[dict, float]:
    args = [str(binary), "--ops", str(ops), "--seed", str(seed), "--json"]
    returncode, stdout, stderr, wall_seconds, peak_rss_bytes = run_with_peak_rss(args)

    if returncode != 0:
        raise RuntimeError(f"malloc_baseline_test --ops {ops} --seed {seed} failed (exit {returncode}): {stderr.strip()}")

    line = stdout.strip().splitlines()[-1] if stdout.strip() else ""
    try:
        record = json.loads(line)
    except json.JSONDecodeError as exc:
        raise RuntimeError(
            f"malloc_baseline_test --ops {ops} --seed {seed} did not print valid JSON: {exc}\n"
            f"stdout: {stdout!r}\nstderr: {stderr!r}"
        ) from exc

    record["host_peak_rss_bytes"] = peak_rss_bytes
    return record, wall_seconds


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", required=True, type=Path, help="a configured CMake build directory, e.g. out/<preset>")
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED, help="deterministic seed passed to every tier (default: %(default)s)")
    parser.add_argument(
        "--time-budget-seconds",
        type=float,
        default=DEFAULT_TIME_BUDGET_SECONDS,
        help="stop before attempting a larger tier once a tier's own wall time exceeds this (default: %(default)s)",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="default: benchmark/allocator-baseline/<os>/ at the repo root (see benchmark/README.md)",
    )
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    binary = find_binary(build_dir)
    out_dir = (args.out_dir or default_out_dir()).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    started_at = datetime.now(timezone.utc)
    arch = platform.machine().lower()
    out_path = out_dir / f"malloc_baseline-{arch}-seed{args.seed}-{started_at.strftime('%Y%m%dT%H%M%SZ')}.jsonl"

    print(f"malloc_baseline binary: {binary}")
    print(f"host: {platform.system()} {platform.machine()}")
    print(f"seed: {args.seed}  time budget per tier: {args.time_budget_seconds}s")
    print(f"results: {out_path}")
    print()

    records = []
    with out_path.open("w", encoding="utf-8") as out_file:
        for ops in TIERS:
            print(f"--- ops={ops} ---")
            try:
                record, wall_seconds = run_tier(binary, ops, args.seed)
            except RuntimeError as exc:
                print(f"FAILED: {exc}", file=sys.stderr)
                out_file.write(json.dumps({"tier_ops_requested": ops, "seed": args.seed, "error": str(exc)}) + "\n")
                return 1

            record["tier_ops_requested"] = ops
            record["wall_seconds"] = wall_seconds
            record["host_system"] = platform.system()
            record["host_machine"] = platform.machine()
            record["started_at"] = started_at.isoformat()
            records.append(record)
            out_file.write(json.dumps(record) + "\n")
            out_file.flush()

            print(
                f"elapsed_ns={record['elapsed_ns']} process_ns={record.get('process_ns')} "
                f"wall_seconds={wall_seconds:.3f} "
                f"throughput_ops_per_sec={record['throughput_ops_per_sec']:.1f} "
                f"p50_ns={record['latency_ns']['p50']} p99_ns={record['latency_ns']['p99']} "
                f"peak_live_bytes={record['live']['peak_bytes']} "
                f"peak_usable_bytes={record['live'].get('peak_usable_bytes')} "
                f"os_regions={record.get('os_regions')} "
                f"host_peak_rss_bytes={record.get('host_peak_rss_bytes')}"
            )

            if wall_seconds > args.time_budget_seconds:
                print(
                    f"\nStopping before the next tier: ops={ops} took {wall_seconds:.1f}s, "
                    f"over the {args.time_budget_seconds}s budget."
                )
                break

    print(f"\nWrote {len(records)} tier result(s) to {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
