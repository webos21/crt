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
live/peak allocation byte and count counters, per-operation-kind counts,
`os_regions` (total distinct `mmap()`/`VirtualAlloc()` regions the
allocator ever mapped), plus this script's own `wall_seconds` (the
externally observed process wall time) and host identification.

**Known open issue, read before trusting `wall_seconds` at the 10^5/10^6
tiers**: on this project's Windows host, `wall_seconds` diverges sharply
from `elapsed_ns`/`process_ns` at those tiers (10-100x larger, growing
with op count) for reasons not yet root-caused -- confirmed real (not an
artifact of the shell, the fork-capable-relaunch startup mechanism, or
OS-level address-space teardown at exit) but not yet localized to before
`main()` or during process exit. See `HISTORY.md`'s 2026-09-16 entry for
the full investigation and the ruled-out-causes list. Until root-caused,
treat `elapsed_ns`/`process_ns` as the trustworthy per-run timing signal;
treat a large `wall_seconds`/`elapsed_ns` gap on any host as this same open
issue, not a new one, unless the evidence actually points elsewhere.

`windows/malloc_baseline-amd64-seed42-20260916T064313Z.jsonl` is the
original run that surfaced the issue above; it predates the `process_ns`/
`os_regions` fields the investigation itself added to this schema (both
added specifically to localize that gap -- see the same HISTORY.md entry),
so those two keys are absent from its four lines. Left as the real,
first-verification record rather than replaced by a partial re-run,
per the project owner's own call (re-running the 10^6 tier costs
~40 minutes and reruns would not add anything the first run didn't
already show). A future full run on this or another host will carry the
complete schema.

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
one (threads, pattern) case's result, with the same `latency_ns`/`live`/
`op_counts` shape `allocator-baseline/` uses, plus `threads`,
`ops_per_thread`, and `pattern`.

The same open wall-clock-vs-`elapsed_ns` issue documented above reproduces
here too, and now also correlates with thread count, not just total op
count -- e.g. `threads=32`/private measured 516ms internally against 78.2s
of external wall time on this host. Treat this as the same open issue
tranche 1 found, not a second one; see `HISTORY.md`'s 2026-09-16 tranche 3
entry.
