# Allocator Baseline Decision Record

This document records the current allocator baseline before the Upper Runtime
work adds heavier allocation pressure from hardware decode, zero-copy graphics
paths, WebRTC, QuickJS, and larger C++ workloads.

The September decision/data below describe the pre-Upper-Runtime implementation.
The 2026-10-15 Web camera follow-up at the end records its indexed-free-list
extension; earlier host results are not measurements of that newer implementation.

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

## Final Decision

Keep the current allocator as the bootstrap/reference allocator for the next
upper-runtime work. The pre-Upper-Runtime validation tranche is accepted and
closed; do not promote Scudo.

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

## Non-Blocking Follow-Up

- Run the focused allocator CTest selection on Windows/aarch64 when that host
  is available.
- Promote Scudo only if a repeatable blocker appears in the current allocator
  and the reproducer is preserved.

## Web Camera Follow-Up (2026-10-15)

macOS Web Tranche 8C exposed a nonlinear latency blocker, not a need to change
the public allocation ABI: Apple's capture model initialization coalesces C++
new/delete to the CRT allocator. Repeated sessions accumulated model work while
every free swept the entire live heap under one lock and malloc searched live
blocks from the beginning. Direct CRT/PAL capture reproduced the slowdown
without WebKit; native libSystem controls did not. The retained samples and
camera measurements are owned by [Web acceptance](crtweb_acceptance.md).

Keep the bootstrap/reference allocator, but index free blocks by size class,
track physical neighbours for local coalescing and append through a tail
pointer. Free-list links live only in released payload. This is still a
globally locked allocator, not Scudo or a claim of constant-time allocation
(a size bucket can contain multiple candidates). The private header changes,
but Bionic-facing malloc/free/realloc/alignment/error contracts do not. Header
and minimum payload alignment are at least 16 bytes, including Windows targets
whose native long double is only 8-byte aligned. Rebuild the runtime as a unit;
private heap snapshots are not interchangeable across builds.

On macOS, ordinary/diagnostic coalescing, canaries and alignment, fork-region
and fault regressions pass in the broad CTest run. Seed-42 baseline correctness
passes all four tiers through 1M operations (12 OS regions at 1M); contention
passes all eight private/shared 1/8/16/32-thread cases. Generated one-off reports
are under `out/macos-host-ninja-debug/tranche8-replay-20261015/allocator-{baseline,contention}/`.
These runs overlapped SDK build activity and are correctness/envelope evidence,
not a controlled throughput comparison against the September data. At this
macOS checkpoint, native Linux replay remained separate; its completed result
follows below, alongside Windows/x64. Cross-target compile checks are not substitutes.

Windows/x64 now passes native core CTest 155/155 after a fresh configuration
and runtime rebuild, including ordinary/diagnostic allocator, fault, fork,
runtime-reset and fragmented many-region fork tests. The seed-42 baseline
passes four tiers through 1M operations (12 OS regions), and the private/shared
1/8/16/32-thread sweep passes all eight cases. Generated reports are under
`out/web-windows-current-allocator/{baseline,contention}/`; logs are
`out/web-windows-current-core-ctest.log` and
`out/web-windows-current-allocator-{baseline,contention}.log`.
SDK work overlapped these runs: they establish correctness, not controlled
throughput comparisons. The runner's raw host-RSS counters have not been
independently reconciled with Windows startup relaunch and are not used as
memory-envelope evidence here. No allocator policy or resource bound changed.

## Windows CI Coalescing Fixture (2026-10-15)

[Windows/x64 CI job 116296326734](https://github.com/webos21/crt/actions/runs/38697153498/job/116296326734)
on `8c989ac` reports only `malloc_test_runs` failing. The detailed log endpoint
requires authentication, so its individual assertion was not independently
retrieved. Locally, that revision's binary fails with `adjacent coalescing reuse`
when `CRT_MALLOC_CI_PADDING` has 22,560 `x` bytes; without padding it passes.
This size is a local reproducer, not a cross-host threshold. Windows startup
copies environment entries into the process heap before main. The previous
fixture's twelve `malloc(2048)` calls therefore need not occupy consecutive
physical blocks, making exact-address reuse an invalid assumption there.

The corrected fixture uses the same production allocator source through the
existing test-only renamed instance, in both ordinary and diagnostic builds.
It verifies one region and all twelve consecutive addresses, then requires
four released blocks to merge and satisfy 7,000 bytes at the first released
address without a new region. Ascending and descending release independently
cover backward and forward merging; live neighbours and shrinking realloc
contents remain checked. Public API tests continue to use the process allocator.
This changes neither the shipped allocator nor the public allocation contract.

Fresh Windows/x64 validation:

```powershell
cmake --fresh --preset windows-host-ninja-debug -B out/windows-malloc-ci
cmake --build out/windows-malloc-ci --target crt-c-build -j 8
ctest --test-dir out/windows-malloc-ci -E '^(crtgfx_|crtmedia_|crtui_|crtweb_|.*cxx)' --output-on-failure
```

- Core: **155/155**, including faults, canaries, contention and native fork.
- Environment sweep: both executables pass 65 padding lengths, 0..30,720 in
  480-byte increments (**130/130**). Each child inherits the scoped padding
  variable; it is removed after the sweep.
- Final normal/diagnostic repeat: 20 executions each, **40/40**.
- Negative controls: temporarily disabling backward merge makes both targets
  fail `backward coalescing reuse`; disabling only forward release merge makes
  both fail `forward coalescing reuse`. Both changes were reverted, confirmed
  by a clean diff of `libc/src/malloc.c`, before rebuilding/running the final core.

Re-run the environment sweep from the repository root (preserve any existing
value in the invoking process):

```powershell
$previousPadding = $env:CRT_MALLOC_CI_PADDING
try {
  foreach ($paddingIndex in 0..64) {
    $env:CRT_MALLOC_CI_PADDING = 'x' * ($paddingIndex * 480)
    foreach ($testName in @('malloc_test', 'malloc_debug_test')) {
      & "./out/windows-malloc-ci/libc/tests/$testName.exe"
      if ($LASTEXITCODE -ne 0) { throw "$testName failed at padding index $paddingIndex" }
    }
  }
} finally {
  $env:CRT_MALLOC_CI_PADDING = $previousPadding
}
```

Evidence: `out/malloc-ci-configure.log`, `out/malloc-ci-build-resume.log`,
`out/malloc-ci-final-build.log`, `out/malloc-ci-core-ctest.log`,
`out/malloc-ci-layout-sweep.log`, `out/malloc-ci-final-repeat.log` and
`out/malloc-ci-mutation-{backward,forward}.log`. The first build's make fetch
was interrupted after sandbox DNS failures and successfully resumed with
network access; no compile/test failure was hidden. The workflow and its
existing two-layout coverage are unchanged. Remote five-host CI confirmation
requires a subsequent push/run; native Linux replay was still a separate gate
at that checkpoint and is completed below.

### macOS Merge Replay

Merged `c0cf7a9` with the macOS Web 8B replay documentation without changing the
CI fix or the runtime. A separate fresh macOS/arm64 configuration and targeted
build pass:

```sh
cmake --fresh --preset macos-host-ninja-debug -B out/macos-malloc-ci-replay
cmake --build out/macos-malloc-ci-replay --target malloc_test malloc_debug_test -j 8
ctest --test-dir out/macos-malloc-ci-replay -R '^malloc(_debug)?_test_runs$' --output-on-failure
```

Both tests pass **2/2**. Running each executable in a subprocess with a
child-scoped `CRT_MALLOC_CI_PADDING` value of 0..30,720 `x` bytes, in 480-byte
steps, also passes **130/130**. The parent process environment is unchanged.
Logs are `out/macos-malloc-ci-replay/{configure,build,ctest,environment-sweep}.log`.
This focused fixture regression is not a full core rerun, a performance result,
or native Linux allocator acceptance; those scopes are not expanded by the merge.

## Linux Native Indexed-Allocator Replay (2026-10-15)

Replayed the current `ef081d2` runtime on Linux/x86_64 after a fresh preset
configuration, preserving the FFmpeg/Skia/GPU/LVGL/imported-libc++ options used
by the Web predecessor SDK. `crt-c-build` rebuilds the current allocator and
test executables; the cumulative `01-c` through `05-ui` SDKs are refreshed and
verified separately. This is native execution, not a cross-target compile or
a run against the old allocator binary.

- Core CTest passes **136/136**, including normal/diagnostic allocation,
  alignment, canaries, fault injection, contention, fork/runtime reset and
  fragmented many-region fork.
- The current isolated coalescing fixture passes both normal and diagnostic
  binaries at all 65 child-scoped environment padding lengths 0..30,720 in
  480-byte steps (**130/130**). The parent environment is unchanged.
- Seed-42 baseline correctness passes **4/4** operation tiers (1K/10K/100K/1M),
  every report `status=ok`, no skipped tier. The 1M run uses 19 OS regions,
  peaks at 26,391,049 requested live bytes and ends with no live workload
  allocations. Its wall time is 34.197s.
- Contention passes **8/8** private/shared cases at 1/8/16/32 threads, 5,000
  operations per thread, seed 42. Every report has `status=ok`.

These one-off runs overlap SDK work and establish correctness, not a controlled
throughput comparison with macOS/Windows or the September baseline. Missing
host-RSS samples remain unknown, not zero. No allocator implementation, public
ABI, test bound or Scudo policy changed in this Linux replay. Together with the
native macOS and Windows evidence above, this closes the indexed allocator's
three-host prerequisite for Web Tranche 8. Windows/aarch64 focused execution
and subsequent remote CI confirmation remain their separately scoped follow-ups.

Reproduce after configuring the current Linux preset:

```sh
cmake --build out/linux-host-ninja-debug --target crt-c-build -j 8
ctest --test-dir out/linux-host-ninja-debug \
  -E '^(crtgfx_|crtmedia_|crtui_|crtweb_|.*cxx)' --output-on-failure
python3 tools/run_allocator_baseline.py --build-dir out/linux-host-ninja-debug \
  --seed 42 --out-dir out/web-crt-webcore/tranche8-close-allocator/baseline
python3 tools/run_allocator_contention_baseline.py --build-dir out/linux-host-ninja-debug \
  --seed 42 --out-dir out/web-crt-webcore/tranche8-close-allocator/contention
```

Generated logs under `out/web-crt-webcore/` are `tranche8-close-configure.log`,
`tranche8-close-sdk-build.log`, `tranche8-close-core-ctest.log`,
`tranche8-close-malloc-layout-sweep.log`, `tranche8-close-allocator-baseline.log`
and `tranche8-close-allocator-contention.log`. Four-tier and eight-case JSONL
reports live in `tranche8-close-allocator/{baseline,contention}/`; they are
ad-hoc correctness evidence, not promoted controlled benchmark data.
