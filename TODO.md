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

### Web Runtime (`06-web`: WebKit CRT Port)

Promoted 2026-10-03 after Application UI (`05-ui`, Tranches 0-7) closed on
Windows/x64, macOS/arm64 and Linux/x86_64; its External Surface contract is the
accepted prerequisite (`HISTORY.md`, `docs/crtui_acceptance.md`). The contract,
gates and host order live in
[`docs/crtweb_acceptance.md`](docs/crtweb_acceptance.md); the upstream mapping in
[`docs/crtweb_porting.md`](docs/crtweb_porting.md). Keep completed evidence in
`HISTORY.md`; this list tracks only tranche state. Linux first (WPE is the
reference), with an early three-host JavaScriptCore replay, then Windows/x64,
then macOS/arm64.

* [x] **0. Scope, version and license freeze.** Closed 2026-10-03: WPE WebKit
  2.54.0 pinned and verified (`libcrtweb/third_party/webkit/`: recomputed SHA-256, signed
  tag, license-file inventory), upstream mapping written, security/SBOM policy
  confirmed; evidence in `HISTORY.md` and `docs/crtweb_acceptance.md`.
* [ ] **1. JavaScriptCore / JSCOnly bring-up.** Linux/x86_64 1A (dependency ports) and 1B
  (interpreter first-green) are done 2026-10-04 (`HISTORY.md`, `docs/crtweb_acceptance.md`),
  built by `tools/build_webkit_jsc.py` against an installed SDK.
  * [x] **1C. Baseline JIT.** Done on Linux/x86_64 and macOS/arm64 2026-10-04 (`HISTORY.md`,
    `docs/crtweb_acceptance.md`): pre-JIT gate, assembly-interpreter and Baseline-JIT runs with a
    compile proof, watchdog termination of compiled code, threads, host-ABI audit.
  * [ ] **1D and hardening.** DFG/FTL/WebAssembly tiers (compiled in, disabled at run time) with
    concurrent compiler threads and the thread-stack question; W^X policy (upstream default does not
    exercise it); the sampling profiler.
  * [ ] **Replays.** macOS/arm64: 1A/1B/1C done 2026-10-04 (interpreter and Baseline-JIT acceptance, thread
    cycles, watchdog and the Mach-O host-ABI audit pass; WebKit is presented as a Linux-shaped platform with two
    carried patches, see `docs/crtweb_acceptance.md`); macOS/x86_64 has no signal conversion or JIT permissions.
    Linux/x86_64 re-verified after the macOS replay (2026-10-04, no regression). Linux/aarch64: 1A/1B/1C
    done 2026-10-05 after implementing native thread TLS (variant I); full ctest 136/136, interpreter and Baseline-JIT
    acceptance green (`HISTORY.md`, `docs/crtweb_acceptance.md`); Windows/x64: 1A and 1B done 2026-10-05 (interpreter acceptance, thread cycles and the PE audit pass; Linux-shaped persona; `HISTORY.md`, `docs/crtweb_acceptance.md`); **next: 1C on Windows** (COFF assembler flavour for offlineasm, the Baseline JIT, the watchdog; polling traps first), and understand why libc++.dll lacks the `<sstream>` exports (Windows links libc++ statically meanwhile)
    ("Linux verification of the macOS replay" in the same document).
    Windows/x64: the ICU/gperf ports, the
    harness, the clocks/`sched_*`/signal code (software signal mask and a stub `sigsuspend`),
    `jit_memory_test` and `VirtualAlloc` shapes.
  * [ ] **Follow-ups found.** ICU and libc++abi use global-dynamic TLS (loader-provided
    `__tls_get_addr`); the default thread stack is 1 MiB; run the harness from the installed
    `tools/crt_dist_prerequisites.py` and `docs/distribution.md` still list `libatomic.so.1` as a required Linux `02-cxx` host dependency, but the libc++ recipe now sets `LIBCXX_HAS_ATOMIC_LIB=OFF` and the 1C audit found it gone: on Linux/x86_64 and aarch64 regenerate a fresh `02-cxx`, check `readelf -d` of `libc++.so`, then drop the prerequisite and the doc text; `05-ui` SDK and from the isolated stage chain (Tranche 10); the Windows/macOS replays must
    include the host-ABI audit (`llvm-readobj`/`dumpbin`, `otool -L`).
  * [ ] **Build-tool/target split.** Before `06-web` is the embedded product stage, separate
    build-host tools (gperf, ICU's data generators, Perl/Python/Ruby) from target libraries so
    a cross build (x86_64 host, Linux/aarch64 target) works; native three-host builds are
    unaffected.
* [ ] **2. Linux WPE reference baseline.** Upstream WPE unchanged rendering
  local HTML; the known-good baseline to diff `PlatformCRT` against.
* [ ] **3. `PlatformCRT` graphics and input prototype.** (Gates before 3B: pin the full
  WebKit commit behind the signed tag as the `PlatformCRT` product source; per-file license scan and
  patch manifest before the first carried patch.) 3A: Linux WPEPlatform
  prototype (shared memory -> `crtgfx` -> `crtui` external surface); 3B+: the
  `PlatformCRT` WebKit port, then GPU-buffer output. The frame-producer and input
  contracts are frozen before 3A (`docs/crtweb_acceptance.md`).
* [ ] **4. `libcrtweb` and the WebView.** The CRT-owned runtime/view API and
  `crtui`'s web view; no WebKit type in a public header.
* [ ] **5. Multi-process lifecycle.** UI/Web/Network/GPU processes: launch, IPC,
  shutdown, forced termination and recovery, 100x create/destroy, leak audit.
* [ ] **6. Windows/x64 replay.**
* [ ] **7. macOS/arm64 replay.**
* [ ] **8. CRT subsystem substitution.** Network, media and capture, one at a
  time, only after a working browser.
* [ ] **9. GPU integration.** WebKit Skia output to an external surface, later a
  shared/zero-copy buffer.
* [ ] **10. Distribution and security closure.** Isolated `06-web` build from
  the installed `05-ui` SDK; update `tools/create_stage_source.py` and
  `tools/crt_dist_prerequisites.py` in the same change; provenance/SBOM/CVE
  artifacts.

Web follow-up (not a gate): `tools/check_webkit_security.py` -- compare the pinned
version with the upstream WPE advisory feed; becomes a package gate before the
first public `06-web` release.

Small UI follow-ups (not gates): teach `tools/prepare_release_assets.py` the
`05-ui` SDK, and make a default-configuration `crt-ui-dist` fail with a clear
message instead of a missing `ui-basic/main.c` (it needs
`-DCRTUI_ENABLE_LVGL=ON`).

## Planned

### Upper runtime roadmap

The completed cross-host baseline and its exact validation evidence stay in
[`STATUS.md`](STATUS.md) and [`HISTORY.md`](HISTORY.md); the product boundary
and dependency order stay in [`docs/runtime_roadmap.md`](docs/runtime_roadmap.md).
The allocator baseline decision gate is closed: keep the current allocator and
leave Scudo conditional. Hardware video decode, Zero-copy decoded textures,
Encode and capture, Networking and streaming, and Application UI (`05-ui`) are
all closed on Linux/x86_64, macOS/arm64, and Windows/x64 (`HISTORY.md`,
2026-09-22..10-03; tranche-by-tranche detail in
`docs/crtmedia_encode_capture_acceptance.md`,
`docs/crtmedia_networking_acceptance.md` and `docs/crtui_acceptance.md`). The
Web Runtime (`06-web`) is in progress above.

Deferred (not gates; see `docs/runtime_roadmap.md`): WebRTC, QuickJS (unless a
non-WebKit lightweight runtime becomes a real product requirement), WebGPU,
EME/DRM, and JS-native application bindings.

The remaining execution order is `06-web`.


### Runtime architecture hardening backlog

These remain independent follow-ups. Promote one at a time when a concrete
consumer or failure justifies their cost. The backend object-boundary work
that had to precede further Upper Runtime expansion is complete (`HISTORY.md`,
2026-09-17..18); the items here were never part of that acceptance gate.

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
   dependency. Budget real effort here given `05-ui`'s LVGL and `06-web`'s
   WebKit/JavaScriptCore work will add more such external dependencies.

2. **Move `tools/crt_dist_prerequisites.py`-style manifest thinking to
   `libc.so`'s own ELF export surface.** The current `CRT_1.0 { global: *;
   };` whole-surface version script (`libc/CMakeLists.txt`) is the right
   *shape* of fix for the collision recorded in the 2026-09-16 history entry,
   but long-term, generating the version-script/export list from an
   explicit Bionic-compatibility symbol manifest (public headers ->
   manifest -> generated `.map` file) would prevent accidental exports and
   give a real basis for `CRT_1.0`/`CRT_1.1`/`CRT_2.0`-style ABI evolution,
   rather than hand-maintaining `global: *;` indefinitely.
3. **Make binary/package/ABI lint an explicit acceptance gate.** Extend the
    distribution dependency and absolute-path checks with a separately
    runnable ABI audit: compare public exports and unresolved imports against
    the stage's declared symbol/dependency manifests, reject accidental host
    ABI leakage, and retain a baseline suitable for detecting incompatible
    changes between released CRT ABI versions.

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
   grows, a real risk once Skia/FFmpeg/LVGL/WebKit/libc++ are all allocating
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

- macOS shared libraries bind libc symbols to Apple's `libSystem`, not the CRT's
  `libc.dylib`: `crt_configure_shared_runtime()` leaves `-lSystem` ahead of
  `libc.dylib` and two-level namespace binding takes the first definer. Code
  compiled against the CRT's 40-byte `pthread_mutex_t` therefore calls Apple's
  64-byte `pthread_mutex_init()`. `libcrtui.dylib` hit this as a heap overflow
  (fixed for crtui only, in `libcrtui/CMakeLists.txt`, with
  `crtui_contract_shared_test_runs` as the guard). `libcrtgfx.dylib`,
  `libcrtmedia.dylib` and the other CRT dylibs have the same binding
  (`nm -m` shows `_pthread_mutex_init (from libSystem)`); crtmedia embeds a
  mutex in its transport structs, so it is exposed too, just not crashing so far.
  Decide whether to fix `crt_configure_shared_runtime()` for all of them and
  add a dylib-linked test per library. Found 2026-09-30
  (`docs/crtui_acceptance.md`, Tranche 3 macOS replay).
- Make the ordinary in-tree Linux `crt-gfx-media-dist` verifiable with
  `CRTMEDIA_ENABLE_CURL=ON`: `libcrtmedia.so` links the shared `libcurl.so.4`
  in-tree (static zlib is not PIC) and `verify_dist.py` reports `undeclared
  libcurl.so.4`, which blocks `crt-ui-dist` in a curl-enabled dev tree. Either
  ship/declare the shared curl chain or document that the ordinary dist is
  options-OFF only; the isolated stage (static curl) is unaffected. Found
  2026-09-30 (`docs/crtui_acceptance.md`, Tranche 0 Linux replay).
- The options-default Linux `crt-ui-dist` tree (`out/linux-ui-check`) packaged no
  `libxdg-shell-protocol.a` (that tree never fetched/built Wayland), so
  `examples/ui-basic` could not be rebuilt against its own SDK without borrowing the
  archive from a Wayland-enabled tree. Determine whether a fresh checkout's
  ordinary Linux dist chain always builds Wayland (then this tree was just
  stale) or whether `crt-ui-dist` should depend on it. Found 2026-09-30
  (`docs/crtui_acceptance.md`, Tranche 3 Linux replay).
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
planned upper-runtime components (`crtui`/LVGL, `libcrtweb`/WebKit, `libcrtgfx`, `libcrtmedia`)
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
