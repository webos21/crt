# Allocator Baseline Decision Record

This document records the current allocator baseline before the Upper Runtime
work adds heavier allocation pressure from hardware decode, zero-copy graphics
paths, WebRTC, QuickJS, and larger C++ workloads.

The purpose of this pass is measurement and risk control, not allocator
replacement. The current bootstrap allocator remains the default unless a
reproducible correctness issue, nonlinear scaling blocker, unbounded memory
growth, or real upper-runtime workload failure exceeds the envelope below.

## Inputs

Current checked-in benchmark data:

- `benchmark/allocator-baseline/windows/malloc_baseline-amd64-seed42-20260916T081617Z.jsonl`
  - Windows/x86_64 current-schema bounded baseline, 1K/10K/100K tiers.
- `benchmark/allocator-contention/windows/malloc_contention_baseline-amd64-seed42-20260916T081657Z.jsonl`
  - Windows/x86_64 current-schema 1/8/16/32-thread private/shared contention sweep.
- `benchmark/allocator-baseline/macos/malloc_baseline-arm64-seed42-20260916T100728Z.jsonl`
  - macOS/arm64 current-schema bounded baseline, 1K/10K/100K tiers.
- `benchmark/allocator-contention/macos/malloc_contention_baseline-arm64-seed42-20260916T100757Z.jsonl`
  - macOS/arm64 current-schema 1/8/16/32-thread private/shared contention sweep.
- `benchmark/allocator-baseline/linux/malloc_baseline-aarch64-seed42-20260916T110130Z.jsonl`
  - Linux/aarch64 current-schema 1K/10K/100K/1M baseline.
- `benchmark/allocator-contention/linux/malloc_contention_baseline-aarch64-seed42-20260916T110201Z.jsonl`
  - Linux/aarch64 current-schema 1/8/16/32-thread private/shared contention sweep.

Earlier macOS files from `20260916T080822Z` and `20260916T084529Z` remain useful
as first macOS timing/correctness evidence, but they predate the current
`usable_bytes` and host-RSS schema.

Focused macOS/arm64 and Linux/aarch64 CTest validation on 2026-09-16 includes:

- `malloc_baseline_test_runs`
- `malloc_contention_baseline_test_runs`
- `malloc_fork_regions_test_runs`
- `malloc_fault_test_runs`
- `host_abi_firewall_test_runs`
- `host_abi_firewall_fault_test_runs`

Both hosts passed their focused selections. Linux/aarch64 validation found
that Clang lowers `__builtin_trap()` to AArch64 `brk`/`SIGTRAP`, not the
Linux/x86_64 `SIGILL` assumed by the original test. POSIX expected-fault tests
now accept either native trap signal while still rejecting clean exits and
unrelated faults. Windows retains the controlled `128 + SIGILL` exit
convention.

Linux/aarch64 also root-caused the formerly unexplained cross-host wall-time
gap. Percentile reporting called CRT's insertion-sort `qsort()` after
`process_ns` was sampled, adding `O(N^2)` reporting time outside both internal
measurements. CRT `qsort()` now uses allocation-free heapsort with worst-case
`O(N log N)` comparisons and a 4096-element reverse/duplicate-heavy regression
test. After the fix, Linux 100K measured 2.348s internally and 2.379s wall;
1M measured 21.081s internally and 21.441s wall.

## Current Envelope

The current allocator is acceptable for the next upper-runtime tranche when all
of the following remain true:

- Correctness tests pass with zero content, alignment, fork-region, diagnostic,
  and owner-domain failures.
- Bounded 1K/10K/100K allocator baseline runs complete without allocation
  failures.
- `os_regions` stays bounded relative to operation count, demonstrating reuse
  rather than unbounded fresh OS mapping.
- `peak_usable_bytes` tracks requested peak bytes within ordinary allocator
  rounding, not uncontrolled internal fragmentation.
- Contention tests complete across 1/8/16/32 threads in both private and shared
  patterns, with no remote-free or remote-realloc corruption.
- Host peak RSS is used as a coarse process-level signal only. POSIX reports
  use `RUSAGE_CHILDREN` cumulative max deltas, so a later run may report `null`
  when it does not exceed an earlier child peak.
- `elapsed_ns` remains the primary allocator-workload timing signal.
  `wall_seconds` should be close after the qsort reporting fix; older checked-
  in Windows/macOS files retain the pre-fix reporting overhead and must not use
  wall time for allocator comparison.

## Provisional Decision

Keep the current allocator as the bootstrap/reference allocator for the next
upper-runtime work. Do not promote Scudo yet.

Rationale:

- Windows/x86_64, macOS/arm64, and Linux/aarch64 current-schema data show
  correct allocator behavior through the deterministic baseline and
  contention sweeps.
- The macOS refreshed run no longer has the earlier schema gap for
  `usable_bytes` or `host_peak_rss_bytes`.
- Linux completed the 1M tier with 19 total OS regions, 26,391,049 requested
  peak bytes versus 26,391,296 usable peak bytes, and about 135MB host peak
  RSS. Region reuse and internal-fragmentation signals remain within the
  envelope.
- Linux private contention scales from about 93K ops/s at one thread to about
  249K ops/s at 32 threads. Shared contention settles around 63K ops/s at
  8-32 threads, exposing the expected global-lock ceiling without deadlock,
  corruption, or throughput collapse.
- The qsort reporting fix explains the cross-host timing anomaly; no allocator
  replacement trigger was found on Linux/aarch64.

## Remaining Before Closing The TODO Tranche

- Run the focused allocator CTest selection on Windows/aarch64 when that host
  is available.
- Compare the first real upper-runtime stress workload against this envelope.
- Promote Scudo only if a repeatable blocker appears in the current allocator
  and the reproducer is preserved.
