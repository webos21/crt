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

Earlier macOS files from `20260916T080822Z` and `20260916T084529Z` remain useful
as first macOS timing/correctness evidence, but they predate the current
`usable_bytes` and host-RSS schema.

Focused macOS/arm64 CTest validation on 2026-09-16:

- `malloc_baseline_test_runs`
- `malloc_contention_baseline_test_runs`
- `malloc_fork_regions_test_runs`
- `malloc_fault_test_runs`
- `host_abi_firewall_fault_test_runs`

All five passed. The macOS expected-fault path accepts Darwin's `SIGTRAP`
result for Clang `__builtin_trap()`, while Linux continues to expect `SIGILL`
and Windows continues to use the controlled `128 + SIGILL` exit convention.

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
- Until the open wall-clock anomaly is root-caused, `elapsed_ns` and
  `process_ns` from inside the CRT binary are the primary allocator timing
  signals. A large external `wall_seconds` gap is tracked as an environment or
  process-boundary investigation item, not by itself an allocator rejection.

## Provisional Decision

Keep the current allocator as the bootstrap/reference allocator for the next
upper-runtime work. Do not promote Scudo yet.

Rationale:

- Windows/x86_64 and macOS/arm64 current-schema data both show correct
  allocator behavior through the deterministic baseline and contention sweeps.
- The macOS refreshed run no longer has the earlier schema gap for
  `usable_bytes` or `host_peak_rss_bytes`.
- Fragmentation and region-reuse signals are acceptable for the tested bounded
  tiers.
- The known timing anomaly is cross-host and still open, but the in-process
  allocator timing stays interpretable and proportional enough to keep using
  these runs as baseline data.

## Remaining Before Closing The TODO Tranche

- Run the current-schema baseline and contention sweep on Linux/aarch64.
- Run the focused allocator CTest selection on Linux and Windows/aarch64 when
  those hosts are available.
- Compare the first real upper-runtime stress workload against this envelope.
- Promote Scudo only if a repeatable blocker appears in the current allocator
  and the reproducer is preserved.
