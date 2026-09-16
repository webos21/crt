# Benchmark results

Raw, machine-readable results from this project's opt-in benchmark/stress
tooling, checked into git deliberately (unlike ordinary build output under
`out/`) so results from every host this project supports accumulate in one
place for direct comparison, instead of living only in whoever's local
build tree happened to produce them.

Each subtree below is produced by its own driver script and documents its
own file format and exact reproduction command. Do not hand-edit any
`.jsonl` file here -- regenerate it with the driver script instead, so the
file always reflects a real run rather than a manually adjusted one.

## `allocator-baseline/<os>/`

[TODO.md](../TODO.md)'s "Allocator baseline validation before Upper
Runtime" tranche 1. Produced by
[`tools/run_allocator_baseline.py`](../tools/run_allocator_baseline.py),
which drives [`libc/tests/malloc_baseline_test.c`](../libc/tests/malloc_baseline_test.c)
(a deterministic, seeded allocator stress workload -- see that file's own
top comment for the workload itself) at the 10^3/10^4/10^5/10^6
operation-count tiers, stopping before a tier that would exceed a declared
wall-clock time budget. One `os` subdirectory per host this project
targets (`windows`/`linux`/`macos`); filenames encode arch, seed, and
start time (`malloc_baseline-<arch>-seed<seed>-<UTC timestamp>.jsonl`) so
repeat runs never collide.

To reproduce, from a configured CMake build directory:

```sh
cmake --build out/<preset> --target malloc_baseline_test
python tools/run_allocator_baseline.py --build-dir out/<preset>
```

Add `--seed <n>` to replay a specific prior run's exact operation sequence,
or `--time-budget-seconds <n>` to change how long a tier may run before
later tiers are skipped (default: 60s). Each line of the resulting
`.jsonl` file is one tier's own result: workload parameters (seed, op
count, host arch/os), the binary's own in-process `elapsed_ns`/`process_ns`
(`CLOCK_MONOTONIC`, bracketing the whole workload and the whole of
`main()` respectively), throughput, an approximate latency distribution,
live/peak allocation byte and count counters (`bytes`/`peak_bytes`: the
callers' own requested sizes; `usable_bytes`/`peak_usable_bytes`: the
allocator's own `malloc_usable_size()` view, which can exceed the request
via rounding -- tranche 4), per-operation-kind counts, `os_regions` (total
distinct `mmap()`/`VirtualAlloc()` regions the allocator ever mapped, a
proxy for "does freed memory get reused or does the region count keep
growing" -- also tranche 4), plus this script's own `wall_seconds` (the
externally observed process wall time), `host_peak_rss_bytes` (the child's
own peak resident memory, sampled via `tools/host_rss.py` -- tranche 4's
"measure ... outside the CRT ABI"; `None` if this host/run could not
determine it), and host identification.

**Resolved reporting-overhead note for older files:** the large gap between
`wall_seconds` and `elapsed_ns`/`process_ns` in the checked-in Windows and
macOS runs was not allocator or process-boundary time. Percentile reporting
sorted every latency sample with CRT's original insertion-sort `qsort()`, an
`O(N^2)` step executed after `process_ns` was captured. Linux/aarch64
validation root-caused this and replaced `qsort()` with allocation-free
heapsort. Current Linux files therefore have wall and in-process timing that
closely agree; retain the older raw files as historical correctness data, but
do not use their `wall_seconds` field for allocator comparisons.

`windows/malloc_baseline-amd64-seed42-20260916T064313Z.jsonl` is the
original run that surfaced the issue above; it predates the `process_ns`/
`os_regions` fields the investigation itself added to this schema, and
also predates `usable_bytes`/`peak_usable_bytes`/`host_peak_rss_bytes`
(tranche 4), so none of those keys are present in its four lines. Left as
the real, first-verification record rather than replaced by a partial
re-run, per the project owner's own call (re-running the 10^6 tier costs
~40 minutes and reruns would not add anything the first run didn't
already show).
`windows/malloc_baseline-amd64-seed42-20260916T081617Z.jsonl` is a
tranche-4 follow-up run (1K/10K/100K tiers only, stopped before 10^6 by
`--time-budget-seconds 20`) carrying the complete current schema,
including a second real, unexplained host-memory anomaly: host-observed
peak RSS stayed flat (~4.04MB) across all three tiers despite
`live.peak_bytes` growing 3x between the 10K and 100K tiers -- see
`HISTORY.md`'s matching 2026-09-16 tranche 4 entry for the full
investigation, including the known-good control that ruled out a bug in
the sampling method itself.
`macos/malloc_baseline-arm64-seed42-20260916T080822Z.jsonl` was captured
between the original Windows timing investigation and the tranche-4 schema
extension, so it intentionally lacks `usable_bytes`,
`peak_usable_bytes`, and `host_peak_rss_bytes`. Treat it as the macOS
correctness/timing record; regenerate a fresh macOS run before using it
for RSS or fragmentation-envelope decisions.
`macos/malloc_baseline-arm64-seed42-20260916T100728Z.jsonl` is that
current-schema refresh (1K/10K/100K tiers, stopped before 10^6 by
`--time-budget-seconds 20`). It supersedes the older macOS file for
usable-byte and host-RSS comparison; the older file remains the original
full-tier timing/correctness record.
`linux/malloc_baseline-aarch64-seed42-20260916T110130Z.jsonl` is the first
Linux/aarch64 current-schema run after the qsort reporting fix. It completes
all four 1K/10K/100K/1M tiers; its wall and in-process times agree closely.

## `allocator-contention/<os>/`

Tranche 3, "scale contention deliberately." Produced by
[`tools/run_allocator_contention_baseline.py`](../tools/run_allocator_contention_baseline.py),
which drives [`libc/tests/malloc_contention_baseline_test.c`](../libc/tests/malloc_contention_baseline_test.c)
across 1/8/16/32 threads in both the "private" (each thread only ever
touches its own allocations) and "shared" (all threads draw from and
return to one mutex-guarded live-set table, so a pointer is routinely
freed or reallocated by a different thread than allocated it) access
patterns -- see that test's own top comment for why both patterns matter
given this allocator's single global `heap_lock`.

To reproduce:

```sh
cmake --build out/<preset> --target malloc_contention_baseline_test
python tools/run_allocator_contention_baseline.py --build-dir out/<preset>
```

Add `--ops-per-thread <n>` to run a heavier sweep than the modest default
(5000 -- kept low deliberately given the wall-clock anomaly below can make
larger op counts take a very long time to actually finish). Each line is
one (threads, pattern) case's result, with the same `latency_ns`/`live`
(including `peak_usable_bytes`, tranche 4)/`op_counts`/`host_peak_rss_bytes`
shape `allocator-baseline/` uses, plus `threads`, `ops_per_thread`, and
`pattern`.

The older Windows/macOS contention files also include the quadratic qsort
reporting overhead described above. Their `elapsed_ns` workload measurements
and correctness results remain useful; their `wall_seconds` values do not.
The current macOS contention file was also captured before the tranche-4
schema extension, so it does not include `peak_usable_bytes` or
`host_peak_rss_bytes`; rerun it for cross-host RSS/fragmentation
comparison.
`macos/malloc_contention_baseline-arm64-seed42-20260916T100757Z.jsonl`
is the current-schema macOS contention refresh. It supersedes the older
macOS contention file for usable-byte and host-RSS comparison. Some later
shared cases have `host_peak_rss_bytes: null` on POSIX because
`tools/host_rss.py` uses `RUSAGE_CHILDREN` deltas; if a later child does
not exceed the already-observed cumulative child peak RSS, there is no new
per-child peak value to report.
`linux/malloc_contention_baseline-aarch64-seed42-20260916T110201Z.jsonl`
is the Linux/aarch64 current-schema 1/8/16/32-thread private/shared sweep
after the qsort fix.
