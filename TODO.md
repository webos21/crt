# TODO: CRT Runtime Work Queue

This file tracks the active CRT runtime work queue and its ordered follow-up.
Completed work moves to `HISTORY.md` in reverse chronological order. Detailed
policy, acceptance evidence and provenance stay in `docs/` and import manifests.

## Notice

- Keep recipe statuses current in
  [`porting/recipes/*.json`](porting/recipes) and
  [`docs/porting/porting_status.md`](docs/porting/porting_status.md) whenever a host is
  rerun. Porting policy and the normal configure/make loop live in
  [`docs/porting/sysroot_ports.md`](docs/porting/sysroot_ports.md); completed porting
  investigations belong in [`HISTORY.md`](HISTORY.md).
- A port is not done until both static and shared builds are attempted
  in the same pass on each host, with any host-specific deferral recorded
  in the recipe notes and status matrix. See
  [`docs/porting/porting_status.md`](docs/porting/porting_status.md) for status meanings.
- For CMake wiring changes, do not trust a long-lived local `out/`
  directory. Verify with a fresh clone or at least
  `cmake --fresh --preset <preset>` before calling the change done; stale
  `CMakeCache.txt`/rootfs artifacts have hidden real CI-only ordering
  bugs before. The resolved cases are recorded in [`HISTORY.md`](HISTORY.md).
- Keep toybox applet enablement tied to audited CRT/PAL support, especially
  LLP64 assumptions on Windows. The live applet list and deferrals are in
  [`docs/porting/toybox_applet_status.md`](docs/porting/toybox_applet_status.md).
- Keep terminal/tty behavior coherent for shell and configure use. Current
  syscall/ioctl coverage is tracked in
  [`docs/porting/sysroot_ports.md`](docs/porting/sysroot_ports.md), with interactive job
  control policy deferred in [`docs/design/job_control.md`](docs/design/job_control.md).
- Treat `CRT_SPAWN_NATIVE_WINDOWS=1` as a narrow launcher hint for native
  host tools only. The wrapper details live in [`tools/crt-cc`](tools/crt-cc),
  [`tools/crt-c++`](tools/crt-c++), [`tools/crt-native-tool`](tools/crt-native-tool),
  and [`docs/porting/sysroot_ports.md`](docs/porting/sysroot_ports.md).
- If a new public libc or `__crt_sys_*` symbol is added, regenerate or replace
  [`porting/recipes/mbedtls-windows-exclude-symbols.rsp`](porting/recipes/mbedtls-windows-exclude-symbols.rsp)
  in the same pass. The reason is documented in
  [`porting/recipes/mbedtls.json`](porting/recipes/mbedtls.json) and
  [`docs/porting/porting_status.md`](docs/porting/porting_status.md).
- Keep work status in exactly one place per purpose, not restated across all
  three: [`HISTORY.md`](HISTORY.md) holds the detailed, dated record of what
  was actually done and why; `TODO.md` keeps only the terse ordered checkbox
  outline needed to see the current tranche and its prerequisites. A checked
  sub-gate may remain in that outline for sequence visibility, but commands,
  dates, measurements and findings move to `HISTORY.md` or the owning
  acceptance document rather than being repeated here. [`STATUS.md`](STATUS.md)
  is updated only when explicitly asked for, not as part of routine
  documentation passes -- do not touch it on a normal work/doc-cleanup turn.


## Done

See [`HISTORY.md`](HISTORY.md) for the full, dated, reverse-chronological
record of completed work. This section stays empty in `TODO.md` itself. Finished
top-level work moves to `HISTORY.md`; terse checked prerequisite gates may remain
under an active tranche only to make its execution order legible.

## In Progress

### Web Runtime (`06-web`: WebKit CRT Port)

Promoted 2026-10-03 after Application UI (`05-ui`, Tranches 0-7) closed on
Windows/x64, macOS/arm64 and Linux/x86_64; its External Surface contract is the
accepted prerequisite (`HISTORY.md`, `docs/acceptance/crtui_acceptance.md`). The contract,
gates and host order live in
[`docs/acceptance/crtweb_acceptance.md`](docs/acceptance/crtweb_acceptance.md); the upstream mapping in
[`docs/porting/crtweb_porting.md`](docs/porting/crtweb_porting.md). Keep completed evidence in
`HISTORY.md`; this list tracks only tranche state and execution order. Linux is
the first implementation host because WPE is the reference; each tranche then
uses the host replay order stated below and in the acceptance document.

* [x] **0. Scope, version and license freeze.** Closed 2026-10-03.
  * [x] Pin and verify WPE WebKit 2.54.0, signed tag/commit and archive hash.
  * [x] Inventory licenses and freeze the security, source-correspondence and SBOM policy.
* [x] **1. JavaScriptCore / JSCOnly bring-up.** Closed 2026-10-09 on all four
  acceptance host/architecture pairs.
  * [x] **1A:** build-host tool inventory and CRT-built ICU/gperf dependency ports.
  * [x] **1B:** C-loop interpreter, lifecycle/thread/RSS checks and Host ABI audit.
  * [x] **1C:** assembly interpreter plus Baseline JIT compile proof and watchdog.
  * [x] **1D-A:** concurrent DFG compiler threads.
  * [x] **1D-B:** FTL/B3.
  * [x] **1D-C:** WebAssembly interpreter, BBQ/OMG and fast-memory fault path.
  * [x] **1D-D:** separate sampling-profiler build and main/worker VM sampling.
* [x] **2. Linux WPE reference baseline.** Closed 2026-10-10 on Linux/x86_64.
  * [x] Build the pristine upstream WPE reference with the native toolchain.
  * [x] Render the local HTML/CSS/DOM/JavaScript/canvas fixture and retain the
    accepted 640x480 frame/proof string for later comparison.
* [x] **3. `PlatformCRT` graphics and input prototype.** Closed 2026-10-13: 3A, 3B-B0..B7 on
  Linux/x86_64, B4-B7 replayed on macOS/arm64 and Windows/x64. Detailed
  contracts, commands and evidence are in
  [`docs/acceptance/crtweb_acceptance.md`](docs/acceptance/crtweb_acceptance.md).
  * [x] **3A:** freeze the v1 frame/input wire contract; prove shared-memory
    rendering and input through the Linux WPEPlatform `crt` reference module.
  * [x] **3A consumer replay:** validate the wire/layout/consumer contracts on
    macOS/arm64, Windows/x64 and Linux/aarch64; the WPE module itself remains
    Linux/x86_64 reference-only by design.
  * [x] **3B-B0:** pin the full product-source commit; close source correspondence,
    per-file licensing and patch-manifest gates.
  * [x] **3B-B1:** compose `PORT=CRT` and pass JSC acceptance on Linux/x86_64,
    macOS/arm64, Windows/x64 and Linux/aarch64.
  * [x] **3B-B2:** build and test the Web dependency ports and make WebKit's
    `find_package()` resolve them on all four acceptance pairs (Windows OpenSSL
    and libcurl remain static-only as documented).
  * [x] **3B-B3:** build/link WTF, PAL and WebCore with bundled Skia and pass the
    graph/paint/text test on all four acceptance pairs.
  * [x] **3B-B4:** on Linux/x86_64 launch UI/Web/Network processes, round-trip IPC
    and detect forced child termination. Replayed on macOS/arm64 and
    Windows/x64 (2026-10-12).
  * [x] **3B-B5:** on Linux/x86_64 `reference.html` renders through `DrawingAreaCRT`,
    the proof matches the WPE reference and the 640x480 frame reaches the CRT
    consumer through the 3A contract; 8.3 % of pixels differ (text: font set).
    Windows/x64 and macOS/arm64 replayed (2026-10-13); Linux/aarch64 only on assembly changes.
  * [x] **3B-B6:** on Linux/x86_64 the 3A input contract (pointer, key, wheel, text,
    resize, stalled consumer) passes through `PlatformCRT` against `surface_probe`.
    Windows/x64 and macOS/arm64 replayed (2026-10-13); Linux/aarch64 only on assembly changes.
  * [x] **3B-B7:** Linux/x86_64 prototype closed: 100x create/destroy flat, Host ABI audit
    (static and `/proc/maps`) clean, license scan and patch manifest rechecked (patch 0018).
    Windows/x64 and macOS/arm64 replayed (2026-10-13); Linux/aarch64 only on assembly changes (patch 0019 on macOS).
* [x] **4. `libcrtweb` and the WebView.** C0-C8 done on Linux/x86_64 2026-10-13 and replayed on
  macOS/arm64 and Windows/x64 (`HISTORY.md`); closed.
  * [x] **C0-C2:** the CRT-owned runtime/view API and callbacks (`crtweb.h`, `libcrtweb.so`) and
    the `crtui` WebView (`crtweb::WebView` over the out-of-process `crtweb_host`).
  * [x] **C4:** multiple runtimes per process, each with its own website data store.
  * [x] **C5:** cookies and website data: persistent or ephemeral data directory, cookie policy, clearing.
  * [x] **C6:** downloads (request, destination decision, progress, result).
  * [x] **C7:** popups (`window.open`, `window.close`) and dialogs (alert, confirm, prompt).
  * [x] **C8:** IME: composition, commit of arbitrary UTF-8 (Hangul), cancel.
  * [x] **C3:** replay `crtweb_api_test` and `crtweb_webview_test` (and C4-C8) on macOS/arm64
    and Windows/x64: macOS/arm64 and Windows/x64 replayed (2026-10-13).
* [x] **5. Multi-process lifecycle.** Closed on Linux/x86_64 2026-10-13:
  public API recovery, bounded teardown and 100x runtime/view cycles; evidence
  in `HISTORY.md` and `docs/acceptance/crtweb_acceptance.md`. GPUProcess joins in Tranche 9.
* [x] **6. Windows/x64 replay.** Closed 2026-10-14: the combined eight-entry
  public API/WebView lifecycle, B5-B7 and static/runtime PE Host ABI gate is green;
  evidence and fixes are in `HISTORY.md` and `docs/acceptance/crtweb_acceptance.md`.
* [x] **7. macOS/arm64 replay.** Done 2026-10-13: B3-B7, Tranche 4 and the Tranche 5
  public API/WebView lifecycle gates pass with the same `PlatformCRT` port (`HISTORY.md`).
* [ ] **8. CRT subsystem substitution.** Sequential gates in `docs/acceptance/crtweb_acceptance.md`.
  Windows/x64 automatic replay is complete; remaining gates are real audio/device evidence,
  Linux/macOS replay of the common permission-token lifetime fix, and camera resource stress.
  Investigation/reproduction: [`Windows Tranche 8 checkpoint`](docs/bringup/crtweb_windows_tranche8.md).
  * [x] **8A network:** CRT-owned Curl/OpenSSL/PAL path and explicit TLS policy accepted on
    Linux/x86_64, macOS/arm64 and Windows/x64 (acceptance).
  * [ ] **8B playback:** CPU playback replay accepted on all three hosts; macOS real output accepted.
    Linux and Windows real-output evidence remains; Windows needs an available default render endpoint.
  * [ ] **8C capture:** synthetic replay accepted on all three hosts; Linux/macOS physical-camera
    evidence recorded. Replay the strengthened token-lifetime regression on Linux/macOS, then perform
    explicitly opted-in Windows physical-camera and same-process physical-camera resource-delta tests.
* [ ] **9. GPU integration.**
  * [ ] Present WebKit's private Skia output through the CRT external-surface path.
  * [ ] Add shared/zero-copy GPU buffer import and synchronization; sharing one
    Skia context is not an initial gate.
* [ ] **10. Distribution and security closure.**
  * [ ] Build `06-web` in isolation from the installed `05-ui` SDK and add the
    stage-source/prerequisite manifests.
  * [ ] Prove the build-host/target split with x86_64-host -> Linux/aarch64-target:
    host gperf/Perl/Python/Ruby/ICU generators, target ICU/libc++/JSC/WebCore/WebKit.
  * [ ] Package `libcrtweb` and a browser sample; pass `verify_dist.py`, dependency,
    RPATH/install-name and Host ABI audits on Linux, Windows and macOS.
  * [ ] Restrict Windows `libcrtmedia.dll` exports to its owned API: the FFmpeg-enabled
    SDK exports 5,981 names (only 90 `crtmedia_*`); a clean PE import audit is not export hygiene.
    Keep FFmpeg/SDK GUID symbols private, as the macOS export boundary already does.
  * [ ] Publish provenance, notices, source correspondence, SBOM and CVE/advisory state.

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
and dependency order stay in [`docs/design/runtime_roadmap.md`](docs/design/runtime_roadmap.md).
The allocator baseline decision gate is closed: keep the current allocator and
leave Scudo conditional. Hardware video decode, Zero-copy decoded textures,
Encode and capture, Networking and streaming, and Application UI (`05-ui`) are
all closed on Linux/x86_64, macOS/arm64, and Windows/x64 ([`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md),
2026-09-22..10-03; tranche-by-tranche detail in
`docs/acceptance/crtmedia_encode_capture_acceptance.md`,
`docs/acceptance/crtmedia_networking_acceptance.md` and `docs/acceptance/crtui_acceptance.md`). The
Web Runtime (`06-web`) is in progress above.

Deferred (not gates; see `docs/design/runtime_roadmap.md`): WebRTC, QuickJS (unless a
non-WebKit lightweight runtime becomes a real product requirement), WebGPU,
EME/DRM, and JS-native application bindings.

The remaining execution order is `06-web`.


### Runtime architecture hardening backlog

These remain independent follow-ups. Promote one at a time when a concrete
consumer or failure justifies their cost. The backend object-boundary work
that had to precede further Upper Runtime expansion is complete ([`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md),
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
   then enable the applet, update `docs/porting/toybox_applet_status.md`, move the
   completed record to `HISTORY.md`, and remove this Planned section.

### Focused CRT/PAL follow-ups

These are real remaining limitations, but none blocks the completed
`libcrtgfx` CPU-raster milestone. Promote one into active work when a consumer
or host investigation supplies the required evidence.

- Measure CRT mksh/Make scaling during FFmpeg configure, including an optimized-build comparison,
  before changing allocator search/coalescing; evidence in [Windows Tranche 8 investigation](docs/bringup/crtweb_windows_tranche8.md).
- macOS shared libraries bind libc symbols to Apple's `libSystem`, not the CRT's
  `libc.dylib`: `crt_configure_shared_runtime()` leaves `-lSystem` ahead of
  `libc.dylib` and two-level namespace binding takes the first definer. Code
  compiled against the CRT's 40-byte `pthread_mutex_t` therefore calls Apple's
  64-byte `pthread_mutex_init()`. `libcrtui.dylib` hit this as a heap overflow
  (fixed for crtui in `libcrtui/CMakeLists.txt`, with
  `crtui_contract_shared_test_runs` as the guard; crtmedia fixed and replayed
  2026-10-14, `HISTORY.md`). `libcrtgfx.dylib` and the other CRT dylibs still need
  the common binding audit (`nm -m` shows `_pthread_mutex_init (from libSystem)`).
  Decide whether to fix `crt_configure_shared_runtime()` for all of them and
  add a dylib-linked test per library. Found 2026-09-30
  (`docs/acceptance/crtui_acceptance.md`, Tranche 3 macOS replay).
- Make the ordinary in-tree Linux `crt-gfx-media-dist` verifiable with
  `CRTMEDIA_ENABLE_CURL=ON`: `libcrtmedia.so` links the shared `libcurl.so.4`
  in-tree (static zlib is not PIC) and `verify_dist.py` reports `undeclared
  libcurl.so.4`, which blocks `crt-ui-dist` in a curl-enabled dev tree. Either
  ship/declare the shared curl chain or document that the ordinary dist is
  options-OFF only; the isolated stage (static curl) is unaffected. Found
  2026-09-30 (`docs/acceptance/crtui_acceptance.md`, Tranche 0 Linux replay).
- The options-default Linux `crt-ui-dist` tree (`out/linux-ui-check`) packaged no
  `libxdg-shell-protocol.a` (that tree never fetched/built Wayland), so
  `examples/ui-basic` could not be rebuilt against its own SDK without borrowing the
  archive from a Wayland-enabled tree. Determine whether a fresh checkout's
  ordinary Linux dist chain always builds Wayland (then this tree was just
  stale) or whether `crt-ui-dist` should depend on it. Found 2026-09-30
  (`docs/acceptance/crtui_acceptance.md`, Tranche 3 Linux replay).
- Extend the built-in resolver (Linux, Windows; macOS uses the system resolver and
  already answers AAAA) from its synchronous UDP IPv4/A-record baseline: AAAA,
  TCP fallback, search domains, caching, and Windows `AF_INET6` sockets, when a
  consumer needs them. IPv6 literals, sockets and `getaddrinfo()` lists are
  there on Linux, macOS (2026-10-11) and Windows (2026-10-12), `socket_ipv6_test`.
- Windows AF_UNIX follow-ups (Gates A/B closed 2026-10-12): `SOCK_DGRAM`, abstract names, `MSG_PEEK` on
  `SOCK_SEQPACKET`, `SCM_RIGHTS` and `SCM_CREDENTIALS` on a stream socket (needs a CRT-to-CRT framed stream), file
  descriptor kinds other than files/pipes/memfds/sockets (eventfd, epoll), `dup()` of a socket fd, and reporting
  socket addresses back with CRT paths instead of host paths. Gate C (Windows `SurfaceClient` consumer replay) closed 2026-10-12.
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

`docs/design/job_control.md`'s "Interactive Job Control" section has the decided
design for all three pieces below; nothing here is implemented yet, and this
project's own mksh build has job control compiled out entirely on every host
(`MKSH_NOPROSPECTOFWORK`), not just Windows -- see that section for why this
is forward-looking policy, not a current gap being actively worked.
Re-evaluated (2026-08-16) against `docs/design/runtime_roadmap.md`: none of the
planned upper-runtime components (`crtui`/LVGL, `libcrtweb`/WebKit, `libcrtgfx`, `libcrtmedia`)
actually depend on POSIX job-control signals (`SIGSTOP`/`SIGTSTP`/`SIGCONT`)
or real fg/bg switching -- confirmed genuinely optional infrastructure, not
something blocking the roadmap. (V8's own "signal/process behavior"
prerequisite in that doc is a separate matter -- `SIGSEGV`-trap-based WASM
bounds checks and `SIGPROF`-style profiling, the "vectored exception
handling" question `docs/design/signal_delivery.md` already tracks independently,
answerable with fully documented Windows APIs.) A full Windows stop/resume
implementation would also need reversing this project's "avoid undocumented
NT internals" pattern (`NtSuspendProcess`/`NtResumeProcess` -- see
`docs/design/job_control.md`'s own "Stopped-child status" note for the design that
was investigated and the alternatives ruled out). Stays deferred.

- Bridge `SetConsoleCtrlHandler` (`CTRL_C_EVENT`/`CTRL_BREAK_EVENT`, both to
  `SIGINT`) into `signal_actions[]`/`raise()`, mirroring `SIGCHLD`'s existing
  pending-flag-plus-checkpoint pattern (`docs/design/signal_delivery.md`).
- Track the real Windows process-group id behind this project's own
  CRT-managed `pgid` integer once a job is actually spawned into a new
  process group, so `tcsetpgrp()` and a targeted `CTRL_BREAK_EVENT` have a
  real id to act on.
- Re-enable `MKSH_UNEMPLOYED` (mksh's own job control) once the above exists,
  and only then decide whether stopped-child (`WIFSTOPPED`) support is worth
  the low-level Windows work it would need -- `docs/design/job_control.md` currently
  keeps that explicitly out of scope.

### Toybox applet expansion (deferred until it's an actual priority)

Only when the backing Bionic-compatible CRT/PAL surface exists.
Full applet-by-applet status (what's enabled,
what's still open and why, the deferred-applet list with each one's
concrete reason, and the `globals.h`/`flags.h` registration traps found
while enabling `df`/`stty`) now lives in
[`docs/porting/toybox_applet_status.md`](docs/porting/toybox_applet_status.md) -- this
bullet stays a pointer. Still open there: `expand`/`logger`/`fold`/
`uudecode`/`cal`/`split`/`strings` (a `globals.h` fix, plus a per-applet
`flags.h` check); `timeout` (tracked by the concrete Windows process-signal
plan above); and a confirmed-not-guessed deferred list
(`ps`/`top`/`iotop`/`pgrep`/`pkill`, `mount`/
`umount`, `ifconfig`, `login`, procfs-heavy commands).
