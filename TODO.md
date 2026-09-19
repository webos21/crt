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

### Hardware video decode

Add real hardware-accelerated H.264 decode while preserving the existing `crtmedia` software-decode contract and Host ABI ownership rules. This tranche ends at a validated CPU-resident decoded frame obtained from a hardware decoder. Native decoded-surface sharing with Vulkan, Metal, or D3D remains explicitly deferred to **Zero-copy decoded textures**.

Recommended host order:

`macOS/arm64 → Windows/x64 → Linux`

Use macOS/arm64 as the first-green reference host, then apply the same Phase-A contract to Windows/D3D11VA and Linux/VA-API.

---

* [x] **0. Freeze the Phase-A hardware-decode acceptance contract.**
  Completed 2026-09-18, recorded in `HISTORY.md`; contract in
  `docs/crtmedia_hardware_decode_acceptance.md`.
* [x] **1. First real hardware-decode green on macOS/arm64 (VideoToolbox).**
  Completed 2026-09-18, recorded in `HISTORY.md`: `RESULT backend=videotoolbox
  ... frame_count=25 fallback=no eos=pass clean_exit=pass`.
* [x] **2. Harden common Phase-A reporting and lifetime behavior.**
  Completed 2026-09-18, recorded in `HISTORY.md`:
  `crtmedia_codec_is_hardware_accelerated()` is true only after a real
  hardware frame is downloaded; flush/reuse and 15-cycle lifecycle tests added.

---

* [ ] **3. Enable and validate Windows/x64 D3D11VA hardware decode.**

  Apply the frozen Phase-A contract to Windows/x64 via FFmpeg D3D11VA; the
  macOS/arm64 Tranche 1-2 implementation is the reference behavior. Do not
  redesign the common `crtmedia` state machine. Ends at a real D3D11VA-backed
  H.264 frame downloaded into the existing CPU `crtmedia_frame` contract;
  D3D11 texture exposure, D3D11<->D3D12 interop, and zero-copy stay deferred.
  Core insight: FFmpeg 8.1.2's `d3d11va` needs `dxva.h` + `ID3D11VideoDecoder` +
  `ID3D11VideoContext`, and its configure probes those via `windows.h`+`d3d11.h`,
  while the CRT recipe `-U_WIN32`/`-UWIN32`/`-U__MINGW32__`s the whole Windows
  FFmpeg build -- resolve that conflict narrowly (probes + D3D/DXVA objects only).
  `codec.c` already selects `AV_HWDEVICE_TYPE_D3D11VA` and `libcrtmedia/
  CMakeLists.txt` already wires `d3d11.lib`/`dxgi.lib`.

  * [ ] **3.1 Baseline before changing FFmpeg.** Fresh Windows/x64 configure/
    build; software 25-frame/EOS contract green; `crtmedia_hw_decode_test`
    (hardware preferred) against the current no-D3D11VA FFmpeg build -- save its
    `RESULT` line (expect `hw_requested=yes`, no hardware frame,
    `hardware_accelerated=false`, 25 software frames, `fallback=yes`, EOS/clean
    exit pass) and the FFmpeg configure summary showing H.264 D3D11VA absent.
  * [ ] **3.2 Reproduce the configure blocker in isolation.** Only add
    `--enable-d3d11va`, `--enable-hwaccel=h264_d3d11va` (+ known MinGW-w64 SDK
    include path if required); capture `ffbuild/config.log` probes for `dxva.h`,
    `ID3D11VideoDecoder`, `ID3D11VideoContext`; confirm `d3d11va requested, but
    not all dependencies are satisfied`. Do not blindly enable more subsystems.
  * [ ] **3.3 Prove the macro conflict with minimal probes.** Compile a tiny
    probe through `tools/crt-cc` (`windows.h`, `dxva.h`, `d3d11.h`, use both
    video interfaces); reproduce failure under the current `-U_WIN32
    -U_WIN32_WCE -U__WIN32__ -UWIN32 -U__MINGW32__` policy; restore candidates
    minimally to find the exact declaration gate; confirm it is declaration
    visibility, not missing headers/libs/search paths. Do not globally restore
    Windows macros yet.
  * [ ] **3.4 Choose the narrowest fix.** Keep the portable global policy for
    ordinary translation units; scope native-Windows declarations to configure
    probes and the FFmpeg objects that really consume D3D11/DXVA
    (`libavutil/hwcontext_d3d11va.c`, `CONFIG_D3D11VA` objects, H.264 D3D11VA
    hwaccel) -- analogous to the macOS VideoToolbox `-fcrt-real-apple-sdk`
    scoping. Inspect the exact objects before adding overrides.
  * [ ] **3.5 Make configure truthfully report D3D11VA.** Apply recipe change;
    genuinely clean FFmpeg rebuild (clear stale FFmpeg install output -- the
    known `--rebuild` install-prefix issue); require H.264 D3D11VA in the
    enabled-hwaccel summary, `CONFIG_D3D11VA`, and `hwcontext_d3d11va` actually
    compiled. "Configure completed" alone is not evidence.
  * [ ] **3.6 Build `libcrtmedia` without reopening the Windows ABI boundary.**
    Reuse the existing `AV_HWDEVICE_TYPE_D3D11VA` path and `d3d11.lib`/
    `dxgi.lib`; add import libs only for a proven unresolved symbol; no
    `ID3D11*`/DXGI types in public `crtmedia`.
  * [ ] **3.7 First real Windows/x64 D3D11VA green.** On a physical machine with
    a real GPU/driver (not Remote Desktop): `RESULT backend=d3d11va
    hw_requested=yes hw_device_created=yes hw_pixfmt_offered=yes
    hw_frame_observed=yes cpu_transfer=pass frame_count=25 fallback=no eos=pass
    clean_exit=pass`; `crtmedia_codec_is_hardware_accelerated()` false after
    creation, true only after the first real download, true after EOS;
    NV12/image-content checks non-degenerate.
  * [ ] **3.8 Tranche 2 lifecycle contract on Windows.**
    `crtmedia_hw_decode_flush_test` (25 hardware frames, seek-to-start, same
    decoder flushed/reused, second 25 fresh hardware frames, sticky true) and
    `crtmedia_hw_decode_lifecycle_test` (15 bounded create/decode/EOS/release
    cycles, all hardware, hang-free).
  * [ ] **3.9 Software and fallback regression.** `crtmedia_extractor_codec_test`
    (software reports false before/after, 25 frames); keep the 3.1 baseline as
    the real "hardware requested but unavailable" fallback evidence; no
    synthetic D3D failure hook; hardware preference stays optional.
  * [ ] **3.10 Windows regression and binary/import audit.** Full relevant
    Windows CTest; recheck FFmpeg/`libcrtmedia` imports (only intended Windows
    dependencies); static/shared `crtmedia` valid; no host-owned D3D object
    freed through a CRT allocator domain; no D3D11 sharing/D3D11<->D3D12 interop
    code entered.
  * [ ] **3.11 Record and close.** `HISTORY.md`: original configure failure,
    proven macro/header root cause, narrow fix, enabled-hwaccel summary,
    Windows/GPU info, final `RESULT`, flush/reuse, lifecycle, software/fallback
    results. Tick the acceptance-gate Windows line; then move to item 4.

---

* [ ] **4. Enable and validate Linux VA-API hardware decode.**

  * First establish a clean Linux environment where the existing software-only FFmpeg recipe rebuilds successfully.
  * Treat any host-toolchain failure such as `ar`/`nm` crashes as a build-environment blocker, not a VA-API defect.
  * Use a native Linux machine with:

    * a real VA-API-capable GPU
    * working `/dev/dri/renderD*`
    * an appropriate VA driver
  * Enable only the FFmpeg VA-API/H.264 pieces required by the existing narrow recipe.
  * Verify from configure output that H.264 VA-API hardware acceleration is actually enabled.
  * Perform a fresh FFmpeg build and rebuild `libcrtmedia`.
  * Run the same hardware-decode fixture.
  * Require:

    * VA-API requested
    * real VA-API hardware frame observed
    * successful CPU transfer from the hardware frame
    * expected frame count
    * valid timestamps
    * valid decoded image contents
    * clean EOS
    * clean destruction
  * Run software-only mode and verify the baseline remains green.
  * Verify hardware-unavailable mode falls back cleanly.
  * Do not count software fallback on a non-VA-API machine as Linux hardware-decode acceptance.
  * Keep VA surface → Vulkan zero-copy interop outside this tranche.

---

* [ ] **5. Normalize the same acceptance matrix across all supported hosts.**

  * Use the same H.264 fixture and the same `crtmedia_hw_decode_test` semantics on:

    * macOS/arm64 — VideoToolbox
    * Windows/x64 — D3D11VA
    * Linux — VA-API
  * For each host, record:

    * requested hardware backend
    * actual hardware backend selected
    * whether a real hardware frame was observed
    * decoded frame count
    * first/last timestamp or monotonicity result
    * CPU-transfer result
    * pixel/image-content result
    * fallback status
    * EOS result
    * clean-exit result
  * Treat the following as distinct outcomes:

    * `decode=pass, hardware=active` → hardware-decode PASS
    * `decode=pass, hardware=inactive, fallback=yes` → fallback PASS, not hardware-decode PASS
    * `decode=fail` → FAIL
  * Keep host-specific implementation details behind FFmpeg/platform ownership boundaries.
  * Do not add platform-native texture handles to the public `crtmedia` API during this tranche.

---

* [ ] **6. Run ownership, regression, and repeated-lifecycle validation.**

  * Repeat hardware decode multiple times in one process where supported.
  * Exercise create → decode → EOS → destroy cycles repeatedly.
  * Verify no stale hardware context survives decoder destruction.
  * Confirm hardware-frame download does not leak or retain platform-native surfaces indefinitely.
  * Re-run allocator/Host ABI diagnostics if any new cross-domain ownership path is introduced.
  * Verify that generic media tests do not directly interpret:

    * `CVPixelBuffer`
    * `ID3D11Texture2D`
    * VA-API private surface structures
  * Ensure platform resources remain opaque and are destroyed by the APIs that own them.
  * Re-run existing software decode/media regression tests on every host after common code changes.

---

* [ ] **7. Close distribution and package acceptance.**

  * Rebuild FFmpeg and `libcrtmedia` from a fresh state on each acceptance host.
  * Rebuild the normal `04-gfx-media` cumulative stage.
  * Run packaged media consumers, not only build-tree tests.
  * Audit binary/runtime dependencies:

    * macOS: VideoToolbox/CoreVideo/CoreMedia-related frameworks
    * Windows: D3D11/DXGI-related imports
    * Linux: expected VA-API/runtime library dependencies
  * Confirm no unexpected host ABI or allocator-domain dependency is introduced.
  * Confirm existing public `crtmedia` ABI and software-only callers remain compatible.
  * Record exact host/architecture, GPU, decoder backend, FFmpeg configuration, test command, and result in `HISTORY.md`.
  * Keep raw logs/results outside git unless they are small, stable project fixtures.

---

### Acceptance gate

This tranche is complete when all of the following are true:

* [x] macOS/arm64 decodes the project H.264 fixture through real VideoToolbox hardware frames. (Tranche 1)
* [ ] Windows/x64 decodes the same fixture through real D3D11VA hardware frames.
* [ ] Linux decodes the same fixture through real VA-API hardware frames on a capable native host.
* [ ] Every hardware path successfully transfers at least one decoded hardware frame into the existing CPU-resident `crtmedia` frame contract.
* [ ] The expected decoded frame count, timestamp behavior, image-content validation, EOS, and cleanup checks pass on every host.
* [x] `hardware_active` or its equivalent means “a real hardware frame was actually observed,” not merely “a hardware device was created.”
* [ ] Software-only decode remains green on every host.
* [ ] Hardware-unavailable or unsupported configurations fall back cleanly to software without breaking the decode contract.
* [ ] No public API exposes platform-native decoded textures or surfaces yet.
* [ ] Host/platform resources remain owned and released by FFmpeg/platform APIs, not by unrelated CRT allocator domains.
* [ ] Fresh packaged `04-gfx-media` builds and binary/import audits remain green on the supported matrix.

**Decision:** when this gate is green, move **Hardware video decode** to `HISTORY.md` and promote **Zero-copy decoded textures** into `In Progress`.

### Recommended execution order

* [x] macOS/arm64 — establish the first real VideoToolbox green.
* [x] Harden common reporting/lifetime semantics using the macOS evidence.
* [ ] Windows/x64 — solve D3D11VA FFmpeg enablement and obtain real hardware evidence.
* [ ] Linux — first remove any toolchain/build-environment blocker, then obtain real VA-API evidence.
* [ ] Re-run the normalized cross-host acceptance matrix.
* [ ] Close packaged `04-gfx-media` and distribution/import acceptance.
* [ ] Promote **Zero-copy decoded textures**.


## Planned

### Upper runtime roadmap

The completed cross-host baseline and its exact validation evidence stay in
[`STATUS.md`](STATUS.md) and [`HISTORY.md`](HISTORY.md); the product boundary
and dependency order stay in [`docs/runtime_roadmap.md`](docs/runtime_roadmap.md).
The allocator baseline decision gate is closed: keep the current allocator and
leave Scudo conditional. Hardware video decode (the roadmap's first tranche)
is now active above; promote the remaining tranches one at a time into
In Progress when their own prerequisite evidence and acceptance host are
available.

1. **Zero-copy decoded textures.** Add explicit ownership and synchronization
   for D3D surfaces, `CVPixelBuffer`/Metal textures, and VAAPI/Vulkan or native
   Linux surfaces; retain a measured copy fallback where interop is absent.
2. **Encode and capture.** Build capture, conversion, hardware/software encode,
   timestamp, and muxing paths on top of the accepted media frame contract.
3. **Networking and streaming.** Add transport, buffering, back-pressure,
   reconnect, and protocol integration only after local media timing is stable.
4. **WebRTC, then JavaScript.** Treat WebRTC as a consumer-driven integration
   milestone. Build the real QuickJS core and CRT bindings before extending
   isolated distribution acceptance from `04-gfx-media` to `05-js`; a stage
   skeleton alone is not completion.

The intended execution order is zero-copy
interop, encode/capture, networking/streaming, WebRTC, and finally the complete
JavaScript application-runtime layer, continuing on from hardware decode above.


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
   dependency. Budget real effort here given `05-js`'s QuickJS/V8 work will
   add at least one more such external dependency.

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
