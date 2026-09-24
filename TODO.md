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

### Zero-copy decoded textures

Promoted 2026-09-22 from `Planned` (`HISTORY.md`'s own "Hardware video
decode" closure entry -- full per-step evidence for macOS/Windows/Linux
hardware decode lives there now, not in this file). Connect decoded
hardware surfaces (D3D11 texture, `CVPixelBuffer`/Metal texture, VA-API
surface) to the native graphics path without a mandatory CPU readback,
where the host supports it, with a measured copy fallback where it does
not. Recommended host order and full frozen contract:
`docs/crtmedia_zero_copy_decode_acceptance.md` (Tranche 0, closed
2026-09-23 -- ownership/lifetime model, `native_handle` per-host identity,
the new `crtmedia_codec_dequeue_gpu_frame()` API shape, the
`crtgfx_skia_media` bridge boundary, and the acceptance gate every host
tranche below closes against).

* [x] **0. Freeze the zero-copy acceptance contract.**
  `docs/crtmedia_zero_copy_decode_acceptance.md`. No public ABI changed;
  `crtmedia_gpu_frame`'s layout stays frozen, `native_handle`'s real
  per-host identity and lifetime are now documented, and `libcrtmedia` gets
  no build dependency on `libcrtgfx` (a third bridge component depends on
  both, mirroring `crtgfx_skia*`'s existing shared-target split).
* [x] **1. macOS/arm64 first green (VideoToolbox -> Metal).**
  Closed 2026-09-23, recorded in `HISTORY.md`. `crtmedia_codec_dequeue_
  gpu_frame()` (`libcrtmedia/src/codec.c`): additive next to `dequeue_
  output()`, real zero-copy branch when `hw_pix_fmt ==
  AV_PIX_FMT_VIDEOTOOLBOX` (`native_handle` = the retained `AVFrame`'s own
  `CVPixelBufferRef`, `plane_count = 0`), CPU-transfer fallback on every
  other host so the API is already usable everywhere; `crtmedia_codec_is_
  hardware_accelerated()` broadened (not redefined), existing `dequeue_
  output()` callers see byte-identical behavior. `crtgfx_skia_import_
  media_frame()` (`crtgfx/skia_media.h`, `skia_bridge.cc`'s Metal branch):
  real `CVPixelBuffer` -> `CVMetalTextureCache` Y/UV textures ->
  `GrYUVABackendTextures` -> `SkImage`, no RGBA intermediate, verified via
  two standalone real-clang probes on this host before landing. New demo
  `crtgfx_skia_media_window_demo` presents 20-25 real decoded frames
  end to end with a real pixel check and a scripted resize, both passing,
  `zero_copy=yes`. Full regression green (138/138 `ctest` with
  `CRTGFX_ENABLE_SKIA=ON`/`CRT_USE_IMPORTED_LIBCXX=ON`). **Still open:**
  the isolated `04-gfx-media` distribution stage has not yet built this
  bridge (deferred to Step 6 below, mirroring "Hardware video decode"'s
  own Step 7 precedent), and device affinity (Tranche 2's own concern) is
  moot on this host's effectively-single-GPU topology, not yet proven
  handled in general.
* [x] **2. Windows/x64 (D3D11VA -> D3D12).** Closed 2026-09-23, recorded
  in `HISTORY.md`. `crtmedia_codec_dequeue_gpu_frame()`'s new
  `AV_PIX_FMT_D3D11` branch (`libcrtmedia/src/codec.c`,
  `gpu_frame_d3d11.h`): `native_handle` is a small crtmedia-owned
  `ID3D11Texture2D*`/array-index indirection (FFmpeg's own decode output
  is one slice of a shared decode-pool array texture, a single pointer
  cannot carry both), no CPU readback in `codec.c` itself. Real device
  affinity resolved by comparing DXGI adapter LUIDs (bails out to the
  existing CPU-transfer fallback on a genuine mismatch); this host's own
  measured, honestly-reported outcome is a **GPU-copy fallback**, not
  literal zero-copy (Skia's D3D12 backend has no multi-plane concept at
  all, confirmed by reading `GrD3DTypes.h` directly, so `crtgfx_skia_
  import_media_frame()`'s D3D12 branch extracts Y/UV via a plane-sliced
  `ID3D11ShaderResourceView1` and a small compute shader into two fresh
  NT-handle-shareable textures, opened into D3D12 -- still a real, no-CPU-
  readback GPU-to-GPU copy, matching this tranche's own acceptance gate).
  `crtgfx_skia_media_window_demo` presents all 25 real decoded frames end
  to end with a real pixel check and a scripted resize, both passing,
  `zero_copy=yes`, verified reproducible across repeated runs. Two real,
  non-obvious bugs found and fixed only by actually running this on real
  multi-adapter hardware, not by inspection: (1) FFmpeg's own D3D11VA
  decode-pool texture needs the `"SHADER"` `av_hwdevice_ctx_create()` opts
  key to get `D3D11_BIND_SHADER_RESOURCE` at all (default is `D3D11_BIND_
  DECODER` only, confirmed by reading `hwcontext_d3d11va.c`/`dxva2.c`
  directly) -- without it every `CreateShaderResourceView1()` call fails
  with `E_INVALIDARG`; (2) a serious, silent ABI-corruption bug in this
  project's own Windows C++ build: `-fwchar-type=int` (a deliberate,
  project-wide 4-byte `wchar_t` override) makes the real SDK
  `DXGI_ADAPTER_DESC`/`DXGI_ADAPTER_DESC1` structs' `WCHAR Description[128]`
  field the wrong size relative to what the real system `dxgi.dll` writes,
  silently misaligning every field after it (`VendorId`, `AdapterLuid`,
  ...) -- `gpu_win32.c` already had its own workaround for this
  (`crtgfx_dxgi_wchar`); `skia_bridge.cc`'s new device-affinity check
  needed the same pattern applied to two more struct shapes. A real,
  CPU-blocking `ID3D11Query`/`D3D11_QUERY_EVENT` sync was also added after
  the compute-shader copy (a plain `Flush()` only submits, does not wait
  for completion, and the D3D12 side could otherwise race the D3D11
  write), and the destination Y/UV shareable textures are now a small,
  persistent per-plane cache (created once, refreshed via the compute
  shader every frame) rather than recreated from scratch every frame.
  **Still open:** the isolated `04-gfx-media` distribution stage has not
  yet built this bridge (same deferral as macOS Tranche 1, above).
* [ ] **3. Linux (VA-API -> Vulkan).** Decide whether to use FFmpeg's own
  Vulkan frame mapping or a direct DRM PRIME/dma-buf import, verified
  against this project's pinned FFmpeg version and its Vulkan 1.1
  requirement (`libcrtgfx`'s own Vulkan instance request) before writing
  any surface-import code.
* [ ] **4. Normalize the acceptance matrix across all three hosts,** mirroring
  `docs/crtmedia_hardware_decode_acceptance.md`'s own per-host results table.
* [ ] **5. Ownership and regression validation:** repeated create/decode/
  destroy cycles leak no platform-native surface, existing CPU-resident and
  software-only paths stay green on every host after this tranche's changes.
* [ ] **6. Close distribution and package acceptance** for the new bridge
  component the same way "Hardware video decode" Step 7 did (fresh-clone
  isolated `04-gfx-media` stage build, binary-dependency audit) once every
  host above is green.


### Next Release

* [ ] Re-verify the hardware-decode-by-default packaged
  `crtmedia_player_demo`/`examples/media-player` (`HISTORY.md`, 2026-09-22)
  on macOS -- already done and verified on Windows.
* [ ] Re-verify it on Linux too.
* [ ] Produce the next release from one frozen tag/commit on every host:
  `v0.4.0-preview.1`'s three asset sets each record a different commit than
  the tag, for real, documented reasons (`docs/release_preview.md`);
  avoiding a repeat is a process fix, not new engineering.
* [ ] Finish the three-platform visual demo: only a Windows clip exists so
  far (`README.md`'s "Demo" section); record the same application on Linux
  and macOS too, with the same evidence (native window, Skia, text, input,
  resize, GPU presentation, FFmpeg playback where practical).
* [ ] Watch for and act on real issue reports from the public launch
  (`HISTORY.md`, 2026-09-23), including any clean-machine problems with the
  `v0.4.0-preview.1` release artifacts -- this is now how that gets
  verified, not a dedicated device acquired for it.



## Planned

### Upper runtime roadmap

The completed cross-host baseline and its exact validation evidence stay in
[`STATUS.md`](STATUS.md) and [`HISTORY.md`](HISTORY.md); the product boundary
and dependency order stay in [`docs/runtime_roadmap.md`](docs/runtime_roadmap.md).
The allocator baseline decision gate is closed: keep the current allocator and
leave Scudo conditional. Hardware video decode (the roadmap's first tranche)
is closed (`HISTORY.md`, 2026-09-22); **Zero-copy decoded textures**, the
next tranche, is promoted into `In Progress` above. Promote the remaining
tranches below one at a time when their own prerequisite evidence and
acceptance host are available.

1. **Encode and capture.** Build capture, conversion, hardware/software encode,
   timestamp, and muxing paths on top of the accepted media frame contract.
2. **Networking and streaming.** Add transport, buffering, back-pressure,
   reconnect, and protocol integration only after local media timing is stable.
3. **WebRTC, then JavaScript.** Treat WebRTC as a consumer-driven integration
   milestone. Build the real QuickJS core and CRT bindings before extending
   isolated distribution acceptance from `04-gfx-media` to `05-js`; a stage
   skeleton alone is not completion.

The intended execution order is encode/capture, networking/streaming, WebRTC,
and finally the complete JavaScript application-runtime layer, continuing on
from zero-copy interop above.


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
