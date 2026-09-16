# TODO: CRT Shell, Rootfs, And Porting Loop

This file tracks the shell/rootfs/porting work queue. Only current work and
planned follow-up stay here; completed work moves to `HISTORY.md` in reverse
chronological order. Detailed policy and provenance stay in `docs/` and import
manifests.

## Notice

- Keep recipe statuses current in
  [`porting/recipes/*.json`](porting/recipes/) and
  [`docs/porting_status.md`](docs/porting_status.md) whenever a host is
  rerun. Porting policy and the normal configure/make loop live in
  [`docs/sysroot_ports.md`](docs/sysroot_ports.md); completed porting
  investigations belong in [`HISTORY.md`](HISTORY.md).
- A port is not done until both static and shared builds are attempted
  in the same pass on each host, with any host-specific deferral recorded
  in the recipe notes and status matrix. See
  [`docs/porting_status.md`](docs/porting_status.md) for status meanings.
- For CMake wiring changes, do not trust a long-lived local `out/`
  directory. Verify with a fresh clone or at least
  `cmake --fresh --preset <preset>` before calling the change done; stale
  `CMakeCache.txt`/rootfs artifacts have hidden real CI-only ordering
  bugs before. The resolved cases are recorded in [`HISTORY.md`](HISTORY.md).
- Keep toybox applet enablement tied to audited CRT/PAL support, especially
  LLP64 assumptions on Windows. The live applet list and deferrals are in
  [`docs/toybox_applet_status.md`](docs/toybox_applet_status.md).
- Keep terminal/tty behavior coherent for shell and configure use. Current
  syscall/ioctl coverage is tracked in
  [`docs/sysroot_ports.md`](docs/sysroot_ports.md), with interactive job
  control policy deferred in [`docs/job_control.md`](docs/job_control.md).
- Treat `CRT_SPAWN_NATIVE_WINDOWS=1` as a narrow launcher hint for native
  host tools only. The wrapper details live in [`tools/crt-cc`](tools/crt-cc),
  [`tools/crt-c++`](tools/crt-c++), [`tools/crt-native-tool`](tools/crt-native-tool),
  and [`docs/sysroot_ports.md`](docs/sysroot_ports.md).
- If a new public libc or `__crt_sys_*` symbol is added, regenerate or replace
  [`porting/recipes/mbedtls-windows-exclude-symbols.rsp`](porting/recipes/mbedtls-windows-exclude-symbols.rsp)
  in the same pass. The reason is documented in
  [`porting/recipes/mbedtls.json`](porting/recipes/mbedtls.json) and
  [`docs/porting_status.md`](docs/porting_status.md).
- Keep work status in exactly one place per purpose, not restated across all
  three: [`HISTORY.md`](HISTORY.md) holds the detailed, dated record of what
  was actually done and why; an item here in `TODO.md` should track live
  progress in a line or two, not re-narrate what a finished sub-part already
  accomplished (once something is done, move the detail to `HISTORY.md` and
  cut it here rather than leaving both). [`STATUS.md`](STATUS.md) is updated
  only when explicitly asked for, not as part of routine documentation
  passes -- do not touch it on a normal work/doc-cleanup turn.


## Done

See [`HISTORY.md`](HISTORY.md) for the full, dated, reverse-chronological
record of completed work. This section stays empty in `TODO.md` itself --
when an item below is finished, move its writeup into `HISTORY.md` (dated,
newest entry first) rather than leaving it here.

## In Progress

### Allocator baseline validation before Upper Runtime

The Linux/aarch64 `03-gfx-simple -> 04-gfx-media` release sign-off and the
separate packaged Vulkan/Skia demo fix are complete. Before adding hardware
decode, zero-copy surfaces, WebRTC, or QuickJS allocation pressure, validate
the current allocator as a short foundation tranche. This is a measurement
and hardening pass, not an allocator replacement: the current allocator stays
in place unless the evidence below demonstrates a blocker. Extend, rather than
replace, the existing API/alignment/reuse coverage in `malloc_test`, the
direct-linked `malloc_debug_test`, and the 4-thread/200-iteration
`malloc_contention_test`.

Execution plan:

1. **Completed 2026-09-16: build a deterministic workload and result
   format.** `libc/tests/malloc_baseline_test.c` (ctest: `malloc_baseline_
   test_runs`, a small bounded correctness variant) + `tools/run_allocator_
   baseline.py` (opt-in, tiered 10^3/10^4/10^5/10^6 runner with a wall-time
   budget gate). Zero correctness failures across all four tiers on a real
   run. Found a real, reproducible, still-unattributed anomaly in the
   process: at the 10^5/10^6 tiers, the binary's own in-process
   `elapsed_ns`/`process_ns` (QueryPerformanceCounter-backed, correctly
   bracketing the entire workload) stay proportionate to op count (2.8s /
   25.9s), but the externally-observed wall time is 10-100x larger (27.6s /
   2540s) -- confirmed real and NOT an artifact of: this session's MSYS/
   Git-Bash shell (reproduces identically timed via a native PowerShell
   `Stopwatch`), `add_crt_test()`'s always-on Windows fork-capable-relaunch
   startup dance (reproduces identically with that object left out of the
   link entirely), or the allocator's own `heap_os_region_count` (only 10
   distinct `mmap()`/`VirtualAlloc()` regions at the 10^5 tier -- too few
   for OS-level address-space teardown at exit to plausibly cost seconds).
   Full detail and the ruled-out-causes list are in `HISTORY.md`'s matching
   dated entry so a future investigator does not repeat this work. Treat
   this session's own `elapsed_ns` numbers as the trustworthy figure for
   tranches below until the gap is root-caused; do not trust bare wall-
   clock timings from this host without cross-checking `elapsed_ns`.
2. **Completed 2026-09-16: cover realistic allocation shapes.** Already
   satisfied by tranche 1's own `malloc_baseline_test.c`, built with this
   tranche's requirements in mind from the start (see that file's own top
   comment): `do_allocate()` mixes `malloc`(80%)/`calloc`(10%, its own
   zero-fill contract verified before reuse)/`posix_memalign`(10%,
   alignment verified) across `pick_size()`'s weighted small(16-128B,60%)/
   medium(129B-4KiB,30%)/large(4KiB-64KiB,9.8%)/mapping-sized(1-4MiB,0.2%,
   "occasional"+"repeated large allocate/touch/free" both satisfied by this
   one rare-but-recurring tier) classes; `do_realloc()` covers grow/shrink;
   the bounded 4096-entry live set forces random churn once full (confirmed
   for real: only 10 distinct `mmap()`/`VirtualAlloc()` regions after
   100,000 operations -- heavy reuse/fragmentation is genuinely happening,
   not merely nominally exercised); every entry's content (and, for
   `posix_memalign`'d entries, alignment) is reverified before any
   operation that reads, moves, or releases it. No new code needed; this
   entry exists so the execution plan reflects that tranche 2 is not a
   separate future task.
3. **Completed 2026-09-16: scale contention deliberately.**
   `libc/tests/malloc_contention_baseline_test.c` (ctest: `malloc_
   contention_baseline_test_runs`, a small fixed-4-thread correctness pass
   through both patterns) + `tools/run_allocator_contention_baseline.py`
   (opt-in 1/8/16/32-thread x private/shared sweep). "private": each
   thread only ever allocates/reallocs/frees its own pointers (malloc_
   contention_test.c's own existing shape, generalized to tranche 1's
   fuller size-class mix). "shared": all threads draw from and return to
   one shared, mutex-guarded live-set table, so a pointer routinely gets
   freed or reallocated by a *different* thread than allocated it -- a
   real remote-free/remote-realloc workload this project had never
   actually exercised before, chosen (project owner's own call, 2026-09-16)
   as the concrete meaning of "shared pool" given `malloc.c` has no
   per-thread arena of its own to make a literal separate pool out of;
   both patterns still fully serialize on the allocator's own single
   `heap_lock` regardless. Zero correctness failures across all 8
   (threads x pattern) combinations on Windows/x86_64.

   A real design bug was caught and fixed before this could be trusted:
   an early draft gave each thread its own `live_count` field even under
   the shared pattern, which is not just an inaccurate stat but a genuine
   corruption risk -- two threads could independently compute the same
   "next free slot" index into the one shared array and each write a live
   pointer into it, silently clobbering the other's entry, despite every
   individual step already being mutex-protected (the mutex serializes
   *execution* of a step; it does nothing to make an ordinary per-thread
   struct field into shared state). Fixed by routing all live-set/byte/
   count bookkeeping through a `pool_ref` of pointers, bound once per run
   to either private per-thread storage or the one real set of shared
   globals -- see the test file's own top comment.

   The 10^5/10^6-tier wall-clock-vs-`elapsed_ns` anomaly this same day's
   tranche 1 entry found (`HISTORY.md`) reproduced here too, in a
   completely different (multithreaded) binary: `threads=16`/private, for
   example, measured 192ms internally but 19.4s of external wall time (a
   ~100x gap); `threads=32`/private measured 516ms internally against
   78.2s externally (~150x). This rules out anything specific to
   `malloc_baseline_test.c`'s own single-threaded workload as the cause,
   and now also correlates with thread count, not just total op count --
   new evidence for whoever picks up the still-open root-cause
   investigation, not a new, separate anomaly.
4. **Completed 2026-09-16: measure fragmentation and resident memory
   outside the CRT ABI.** Both baseline binaries now track and report
   `live.usable_bytes`/`live.peak_usable_bytes` (`malloc_usable_size(ptr)`
   summed across the live set, alongside the existing requested-byte
   counters -- the gap between the two is this allocator's own internal
   fragmentation from `align_size()` rounding). `tools/host_rss.py` (new)
   samples each child's own peak host-observed resident memory --
   `PeakWorkingSetSize` via `GetProcessMemoryInfo()` on Windows,
   `resource.getrusage(RUSAGE_CHILDREN).ru_maxrss` deltas on POSIX -- kept
   entirely on the host-side runner as the TODO wording requires, not
   added to the Bionic-facing binaries. Both `tools/run_allocator_
   baseline.py`/`_contention_baseline.py` now report `host_peak_rss_bytes`
   per run.

   "Post-free reuse" vs. "continually growing resident memory" is already
   answered by tranche 1's own `os_regions` field across the tier sweep:
   6 regions at both the 1K and 10K tiers (zero new OS mappings for 10x
   more operations), only 10 at 100K (10x more ops again, far short of
   10x more regions) -- clear evidence freed blocks are actually being
   reused by later allocations, not forcing new mappings each time.

   A second real, unexplained host-memory anomaly was found and
   investigated (not fully root-caused, same treatment as tranche 1's
   timing anomaly): host-observed peak RSS stayed flat (~4.04MB) across
   the 1K/10K/100K tiers despite `live.peak_bytes` growing 3x (3.5MB to
   10.9MB) between the 10K and 100K tiers, and `PeakPagefileUsage`
   (committed/pagefile-backed memory, which should track `VirtualAlloc(...,
   MEM_RESERVE | MEM_COMMIT, ...)` directly -- confirmed that is genuinely
   what `__crt_sys_mmap()` calls on Windows) barely moved either
   (6.18MB to 6.19MB). The RSS-sampling method itself was validated
   against a known-good control first (a plain Python subprocess
   allocating and touching 50MB correctly reported ~60MB peak working
   set), ruling out a bug in `tools/host_rss.py` or a broken/sandboxed
   `GetProcessMemoryInfo()` in this environment -- the flat reading is
   specific to this allocator's own executable. Left open for whoever
   picks up tranche 8's cross-host decision record; see `HISTORY.md`'s
   2026-09-16 entry for the full writeup.
5. **Exercise fork interaction and allocator regions.** Add a many-region,
   fragmented-heap fork regression. The child must verify inherited contents,
   mutate and allocate independently, and exit cleanly while the parent proves
   its allocations were unchanged. Run the native fork path on Linux/macOS
   and the memory-copy fork path on both supported Windows architectures.
6. **Make diagnostics permanent and testable.** Change
   `CRT_ENABLE_DEBUG_MALLOC` from a temporary investigation switch into a
   supported diagnostic mode, retaining its normal and direct-linked CTest
   coverage. Add expected-fault tests for double free, cross-instance owner
   mismatch, and canary damage. Evaluate and implement
   `CRT_ENABLE_GUARD_MALLOC` only as a distinct opt-in guard-page diagnostic,
   with explicit large-memory/slow-test labeling; it is not the production
   allocator. Record a go/no-go decision, but do not hold the baseline gate
   open solely to build it if debug-malloc expected-fault coverage already
   provides the required diagnostics.
7. **Freeze the Host ABI firewall before hardware decode.** Document and audit
   the ownership rule for Wayland/Vulkan and the upcoming FFmpeg hardware,
   VA-API, PipeWire, and EGL boundaries: opaque host objects are created,
   retained, synchronized, and destroyed only by the host library and
   allocator domain that owns them. CRT adapters may transport opaque handles
   but must not reinterpret private layouts or free host-owned storage. Add
   boundary assertions/tests where ownership can be checked mechanically.
8. **Publish one cross-host decision record.** Run the bounded correctness
   suite on the normal Linux, Windows, and macOS matrix and record the heavier
   baseline per host/architecture in `docs/allocator_baseline.md`. Declare the
   workload/time/RSS regression envelope before using results to choose an
   allocator. Record raw results under `benchmark/allocator-baseline/<os>/`
   (checked into git -- see `benchmark/README.md` -- superseding this
   tranche's earlier "not in git, under `out/`" plan: tranche 1 landed the
   runner first and the project owner asked for results to accumulate
   per-host in the repo itself, where they can be compared and reproduced
   directly, rather than staying only in one contributor's local build tree).

Decision gate:

- If correctness, fork, and diagnostic tests pass and representative upper-
  runtime workloads stay inside the declared scaling and memory envelope,
  retain the current allocator, move this tranche to `HISTORY.md`, and proceed
  with the Upper Runtime Roadmap.
- If repeatable nonlinear cost, lock-contention collapse, unbounded RSS growth,
  or a correctness limitation exceeds that envelope, first preserve the
  reproducer, then promote the conditional Scudo tranche below into In
  Progress. Do not start Scudo merely because the current implementation is
  theoretically `O(N)`.

## Planned

### Conditional Scudo production allocator

Do not promote this tranche until the baseline-validation decision gate records
a repeatable current-allocator limitation. The completed Skia incident was an
allocator-domain linkage error, not evidence that the allocator algorithm must
be replaced.

1. **Separate the bootstrap allocator from a production allocator only when
   measurements justify it.**
   `libc/src/malloc.c` is a clear, easy-to-audit design for CRT bring-up,
   but its `malloc()`/`free()`/new-chunk paths are all linear scans
   (`append_chunk()` walks the whole chunk list to find its tail;
   `malloc_unlocked()` first-fits linearly; `coalesce_free_blocks()`
   rescans the whole free list on every `free()`), all under one global
   `heap_lock` spinlock -- `O(N)` per operation as allocation count `N`
   grows, a real risk once Skia/FFmpeg/QuickJS/libc++ are all allocating
   heavily through it. Keep this implementation as the bootstrap/reference/
   diagnostic allocator; evaluate LLVM's Scudo Hardened Allocator (size-
   class allocators, per-thread caches, mmap-backed large allocations,
   built-in quarantine, official standalone build support) as the long-term
   production allocator -- a natural fit given this project's own Bionic-
   compatibility framing, since Scudo has been Android's own default
   allocator since Android 11.
2. **Decouple allocator regions from Windows fork emulation before any swap.**
   Replace `__crt_malloc_os_region_count()`/`_base()`/`_size()` with a small,
   allocator-agnostic heap-region snapshot interface consumed by both Windows
   fork implementations. Prove the interface first with the current allocator
   and the many-region fork workload; Scudo cannot be selected until the same
   contract can enumerate or otherwise preserve every child-visible region.
3. **Integrate Scudo behind an explicit allocator selection.** Import it with
   provenance, preserve Bionic-compatible public allocation behavior, keep the
   bootstrap allocator selectable for bring-up/diagnostics, and run the exact
   same correctness, fork, performance, RSS, distribution, and upper-runtime
   workloads against both implementations. Adoption requires measured benefit
   on the failing baseline without regressing supported hosts.

### Runtime architecture hardening backlog

These remain independent follow-ups. Promote one at a time when a concrete
consumer or failure justifies its cost; they do not block allocator baseline
validation unless that validation exposes the same boundary.

1. **Remove the "build once, then reconfigure" CMake pattern.** The native
   Wayland Vulkan WSI backend's own `CRTGFX_HAVE_NATIVE_WAYLAND` gate
   checks `EXISTS` on a generated `libxdg-shell-protocol.a` at *configure*
   time, so a genuinely clean tree needs `configure -> build
   crtgfx-wayland-build -> reconfigure -> build` -- already documented as a
   deliberate, temporary workaround for a real dependency-cycle risk, not
   a permanent design. Skia's own external/nested-build integration is
   accumulating similar shape. The durable fix is a physically separate
   superbuild stage (libc++/Wayland/Skia/FFmpeg as one dependency-prefix-
   producing phase) feeding a single, cycle-free runtime configure for
   `libcrtgfx`/`libcrtmedia` -- not another one-off `EXISTS` gate per new
   dependency. Budget real effort here given `05-js`'s QuickJS/V8 work will
   add at least one more such external dependency.

2. **Stop letting a compile macro change a shared struct's own layout.**
   `libcrtgfx/CMakeLists.txt` already carries a load-bearing comment
   explaining why `CRTGFX_HAVE_VULKAN` must be a directory-wide
   `add_compile_definitions()`, not a per-target one: `src/gpu.c` and
   `gpu_vulkan.c` both see the same `struct crtgfx_gpu_device`, and a
   mismatched definition between translation units changes that struct's
   own size, corrupting memory. The current fix (matching the definition
   everywhere) is correct, but the underlying shape -- one macro flip away
   from real memory corruption -- is itself worth removing as a class of
   bug: give `crtgfx_gpu_device` a fixed `{backend tag, ops vtable, opaque
   backend pointer}` layout and hide `CRTGFX_HAVE_VULKAN`/`_D3D12`/`_METAL`
   backend-specific fields entirely inside each backend's own translation
   unit, so the macro only ever changes which code exists, never the
   common struct's own layout.
3. **Move `tools/crt_dist_prerequisites.py`-style manifest thinking to
   `libc.so`'s own ELF export surface.** The current `CRT_1.0 { global: *;
   };` whole-surface version script (`libc/CMakeLists.txt`) is the right
   *shape* of fix for the collision recorded in the 2026-09-16 history entry,
   but long-term, generating the version-script/export list from an
   explicit Bionic-compatibility symbol manifest (public headers ->
   manifest -> generated `.map` file) would prevent accidental exports and
   give a real basis for `CRT_1.0`/`CRT_1.1`/`CRT_2.0`-style ABI evolution,
   rather than hand-maintaining `global: *;` indefinitely.
4. **Add an upper-stage smoke gate to routine CI**, not just the C stage.
   Every recent Linux ELF-collision-family bug (`libc` vs host glibc,
   `libc` vs `libc++`, `libc++` vs Skia, Wayland vs Vulkan, Skia vs Vulkan)
   showed up specifically at a *stage boundary*, not within `01-c` alone.
   Full Skia/FFmpeg builds are too slow for every PR, but a per-PR
   `02-cxx` smoke plus a headless `libcrtgfx` smoke, with a nightly full-
   Skia/FFmpeg/predecessor-only-dist/binary-import-audit pass from a
   genuinely empty build directory and cache (this project's own CMake
   state has already been shown to interact with external-build artifact
   presence -- see item 1), would have caught several of the bugs in
   `HISTORY.md`'s dated entries closer to when they were introduced.
5. **Split the Linux Wayland backend into two independent implementations**
    instead of one code path that changes object universes depending on
    whether Vulkan is enabled. `STATUS.md` already records a real, current
    limitation in the hand-rolled path (the software-window adapter does
    not recycle Wayland object IDs); the GPU path already uses the real
    host `libwayland-client.so.0` entirely separately. Making that an
    explicit `crtgfx` Wayland abstraction with two backends (CRT wire
    protocol vs. host-native) rather than one file family that
    conditionally reinterprets its own objects would be easier to maintain
    as both paths keep evolving.
6. **Make binary/package/ABI lint an explicit acceptance gate.** Extend the
    distribution dependency and absolute-path checks with a separately
    runnable ABI audit: compare public exports and unresolved imports against
    the stage's declared symbol/dependency manifests, reject accidental host
    ABI leakage, and retain a baseline suitable for detecting incompatible
    changes between released CRT ABI versions.

### Upper runtime roadmap

The completed cross-host baseline and its exact validation evidence stay in
[`STATUS.md`](STATUS.md) and [`HISTORY.md`](HISTORY.md); the product boundary
and dependency order stay in [`docs/runtime_roadmap.md`](docs/runtime_roadmap.md).
This roadmap remains Planned until the allocator baseline decision gate above
closes. After that, promote one tranche at a time into In Progress when its
prerequisite evidence and acceptance host are available.

1. **Finish live GPU presentation evidence.** Close the remaining macOS/x86_64
   live path, pixel-exact macOS checks, resize-plus-Ganesh coverage on macOS,
   and pixel-exact resize coverage on Windows. Keep Graphite as a later backend
   rather than part of the current Ganesh acceptance bar.
2. **Hardware video decode.** Complete Phase A backend bring-up and tests.
   The current blockers are the Windows D3D11VA configure path, a WSL-hosted
   binutils 2.46 `ar`/`nm` crash, and an unattempted macOS pass. Preserve a
   software fallback and report hardware use separately from decode success.
3. **Zero-copy decoded textures.** Add explicit ownership and synchronization
   for D3D surfaces, `CVPixelBuffer`/Metal textures, and VAAPI/Vulkan or native
   Linux surfaces; retain a measured copy fallback where interop is absent.
4. **Encode and capture.** Build capture, conversion, hardware/software encode,
   timestamp, and muxing paths on top of the accepted media frame contract.
5. **Networking and streaming.** Add transport, buffering, back-pressure,
   reconnect, and protocol integration only after local media timing is stable.
6. **WebRTC, then JavaScript.** Treat WebRTC as a consumer-driven integration
   milestone. Build the real QuickJS core and CRT bindings before extending
   isolated distribution acceptance from `04-gfx-media` to `05-js`; a stage
   skeleton alone is not completion.

The intended execution order is live GPU evidence, hardware decode, zero-copy
interop, encode/capture, networking/streaming, WebRTC, and finally the complete
JavaScript application-runtime layer.

### Windows process signals and Toybox `timeout`

Close the two confirmed Windows PAL gaps that keep Toybox `timeout` disabled:
`__crt_sys_kill()` currently supports only the calling process's
`kill(pid, 0)` probe and returns `ENOSYS` for a real child or process group,
while `deliver_signal()` synthesizes `SA_SIGINFO` with the caller's pid and
zero `si_code`/`si_status` even when the Windows child registry already knows
which child exited and its exit code. Keep this tranche limited to
non-interactive child lifecycle and timeout enforcement; Ctrl-C/Ctrl-Break,
foreground-terminal arbitration, stopped-child reporting, and re-enabling
mksh job control remain in the separate deferred section below.

1. **Freeze the Bionic-facing contract and reproduce each failure.** Check
   Bionic's `kill()` pid-domain, errno, default-action, and `SIGCHLD`
   `siginfo_t` rules before changing the PAL. Add bounded Windows regressions
   for a live/dead foreign-pid `kill(pid, 0)` probe, positive child signaling,
   negative process-group signaling, and a child exiting with status 7 whose
   `SA_SIGINFO` handler must observe the real pid, `CLD_EXITED`, and status 7.
   Retain direct consumer evidence that `timeout 2 sleep 10` currently runs
   for roughly 10 seconds instead of enforcing its deadline.
2. **Thread real child-exit information through signal dispatch.** Extend the
   private signal backend/dispatch interface so Windows can pass a populated
   `siginfo_t` from `child_process_table`/`child_pid_table` and
   `GetExitCodeProcess()` without changing `raise()`'s self-delivery contract.
   Natural exits must report `CLD_EXITED` and the actual status; a termination
   mechanism introduced below must distinguish `CLD_KILLED` and its signal.
   Preserve the existing once-per-state-change notification and blocked-
   `SIGCHLD`/`pselect()` behavior.
3. **Implement positive-pid `kill()` with explicit, honest semantics.** Use
   documented Windows APIs for existence/access probing and `SIGKILL`; design
   a CRT-owned cooperative delivery channel for catchable signals so
   `SIGTERM`/user handlers are not silently reduced to unconditional
   `TerminateProcess()`. Define PID-reuse protection, permissions, pending-
   signal coalescing, and delivery checkpoints up front. Unsupported signals
   must fail explicitly rather than report success without delivery.
4. **Implement the non-interactive process-group subset needed by
   `timeout`.** Connect the CRT-managed pgid to real spawned-child membership
   and support Toybox's default `kill(-pgid, SIGTERM)`/`SIGKILL` path, including
   descendants. Evaluate documented Windows process groups and Job Objects
   against the post-`fork()` `setpgid(0, 0)` lifecycle before choosing the
   mechanism; do not pull in undocumented suspend/resume APIs or claim full
   POSIX job control as part of this tranche.
5. **Accept through the real packaged consumer.** Require positive-pid and
   process-group signal tests, correct `SIGCHLD` pid/code/status for natural
   and killed exits, `timeout 2 sleep 10` completing near two seconds with
   status 124, `timeout --preserve-status 10 sh -c 'exit 7'` returning 7, and
   the TERM-to-KILL `-k` path. Re-run the existing `pselect_sigchld`, process-
   stress, fd-snapshot, fork, waitpid, shell, and full Windows CTest coverage;
   rebuild the packaged shell/dist and repeat the timeout cases there. Only
   then enable the applet, update `docs/toybox_applet_status.md`, move the
   completed record to `HISTORY.md`, and remove this Planned section.

### Focused CRT/PAL follow-ups

These are real remaining limitations, but none blocks the completed
`libcrtgfx` CPU-raster milestone. Promote one into active work when a consumer
or host investigation supplies the required evidence.

- Extend the resolver from its current synchronous UDP IPv4/A-record baseline
  when IPv6, TCP fallback, search domains, or caching becomes a consumer
  requirement.
- Revisit a CRT-owned ELF loader/Android-linker boundary only after a real
  upper-runtime consumer requires behavior the host loader adapter cannot
  provide.
- Harden FreeType's fetch beyond the single SourceForge URL fix (`5b87197`)
  -- add retry-on-transient-failure, a documented fallback mirror, and
  SHA-256 verification of the cached archive before reuse, matching the
  reliability bar other `porting/recipes/*.json` ports already meet.
- Root-cause the packaged Windows `02-cxx` `<iostream>` static-initialization
  crash. Reproduce it against packaged and in-tree builds, compare `.ctors`
  and `std::ios_base::Init` startup behavior, and keep the stage runner free of
  retries that could hide the fault. `<cstdio>` working is a diagnostic fact,
  not evidence that C++ startup is complete.

### Interactive job control (deferred until it's an actual priority)

`docs/job_control.md`'s "Interactive Job Control" section has the decided
design for all three pieces below; nothing here is implemented yet, and this
project's own mksh build has job control compiled out entirely on every host
(`MKSH_NOPROSPECTOFWORK`), not just Windows -- see that section for why this
is forward-looking policy, not a current gap being actively worked.
Re-evaluated (2026-08-16) against `docs/runtime_roadmap.md`: none of the
planned upper-runtime components (`libcrtjs`/QuickJS+V8, `libcrtgfx`, `libcrtmedia`)
actually depend on POSIX job-control signals (`SIGSTOP`/`SIGTSTP`/`SIGCONT`)
or real fg/bg switching -- confirmed genuinely optional infrastructure, not
something blocking the roadmap. (V8's own "signal/process behavior"
prerequisite in that doc is a separate matter -- `SIGSEGV`-trap-based WASM
bounds checks and `SIGPROF`-style profiling, the "vectored exception
handling" question `docs/signal_delivery.md` already tracks independently,
answerable with fully documented Windows APIs.) A full Windows stop/resume
implementation would also need reversing this project's "avoid undocumented
NT internals" pattern (`NtSuspendProcess`/`NtResumeProcess` -- see
`docs/job_control.md`'s own "Stopped-child status" note for the design that
was investigated and the alternatives ruled out). Stays deferred.

- Bridge `SetConsoleCtrlHandler` (`CTRL_C_EVENT`/`CTRL_BREAK_EVENT`, both to
  `SIGINT`) into `signal_actions[]`/`raise()`, mirroring `SIGCHLD`'s existing
  pending-flag-plus-checkpoint pattern (`docs/signal_delivery.md`).
- Track the real Windows process-group id behind this project's own
  CRT-managed `pgid` integer once a job is actually spawned into a new
  process group, so `tcsetpgrp()` and a targeted `CTRL_BREAK_EVENT` have a
  real id to act on.
- Re-enable `MKSH_UNEMPLOYED` (mksh's own job control) once the above exists,
  and only then decide whether stopped-child (`WIFSTOPPED`) support is worth
  the low-level Windows work it would need -- `docs/job_control.md` currently
  keeps that explicitly out of scope.

### Toybox applet expansion (deferred until it's an actual priority)

Only when the backing Bionic-compatible CRT/PAL surface exists.
Full applet-by-applet status (what's enabled,
what's still open and why, the deferred-applet list with each one's
concrete reason, and the `globals.h`/`flags.h` registration traps found
while enabling `df`/`stty`) now lives in
[`docs/toybox_applet_status.md`](docs/toybox_applet_status.md) -- this
bullet stays a pointer. Still open there: `expand`/`logger`/`fold`/
`uudecode`/`cal`/`split`/`strings` (a `globals.h` fix, plus a per-applet
`flags.h` check); `timeout` (tracked by the concrete Windows process-signal
plan above); and a confirmed-not-guessed deferred list
(`ps`/`top`/`iotop`/`pgrep`/`pkill`, `mount`/
`umount`, `ifconfig`, `login`, procfs-heavy commands).
