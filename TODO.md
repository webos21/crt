# TODO: CRT Shell, Rootfs, And Porting Loop

This file tracks the shell/rootfs/porting work queue. The list is ordered by
state: completed work first, current work second, and planned follow-up last.
Detailed policy and provenance stay in `docs/` and import manifests.

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

Active threads, not a flat list of one-off items.

### Upper runtime roadmap

The completed CPU baseline is summarized in [`STATUS.md`](STATUS.md), and its
dated implementation trail belongs in [`HISTORY.md`](HISTORY.md). The live
queue below starts at the next compatibility boundary: a stable media API,
software playback, GPU resource ownership, hardware decode, and the point at
which QuickJS can safely bind those services without freezing a temporary API.
The long-term target remains the Electron-class runtime described in
[`docs/runtime_roadmap.md`](docs/runtime_roadmap.md).

1. **Decide the public media API policy.** Compare an Android NDK Media-shaped
   C core (`AMediaFormat`/extractor/codec concepts), a WebRTC-shaped optional
   realtime track layer, and a WebCodecs-shaped JavaScript binding. Keep
   FFmpeg types private, retain the existing `crtmedia_demuxer_*` API as a
   convenience layer, and document whether exact NDK source compatibility is
   a later adapter rather than the core ABI.
2. **Complete software-decode evidence.** Add real H.264+AAC MP4 and MP3
   fixtures; verify threaded H.264 decode, PTS/DTS/duration ordering, EOF
   drain/flush, malformed input, and static/shared behavior on all three
   hosts. The existing WAV/PCM test remains the smallest baseline.
3. **Separate extractor and codec.** Introduce opaque format, packet,
   extractor, and codec objects; track selection, seek, flush, synchronous
   polling, asynchronous callbacks, bounded queues, and backpressure. Rebuild
   the current demux/decode convenience API on this layer.
4. **Build the software player.** Add play/pause/seek/stop, a monotonic master
   clock, A/V synchronization, buffering/frame-drop policy, and host audio
   sinks (WASAPI, CoreAudio, and PipeWire/ALSA) while keeping decoded video on
   the verified CPU-frame/Skia path.
5. **Fix the common GPU resource contract.** Define opaque `crtgfx` GPU
   device/surface and `crtmedia` GPU-frame objects, capability queries,
   CPU/GPU memory kinds, retain/release, device affinity, and acquire/release
   fences without exposing Direct3D, Metal, Vulkan, or FFmpeg types in public
   headers. Software fallback must remain a first-class path.
6. **Enable Skia GPU rendering.** Start with a Ganesh vertical slice and keep
   Graphite as a later measured alternative: Direct3D on Windows, Metal on
   macOS, and Vulkan/Wayland on Linux. Run the existing deterministic Skia
   drawing coverage against GPU surfaces and add resize, device-loss, and
   context-recreation tests.
7. **Add hardware decode, phase A.** Enable FFmpeg D3D11VA/D3D12VA,
   VideoToolbox, and VA-API backends, initially downloading decoded frames to
   CPU memory so codec/device selection, fallback, and recovery can be proved
   independently of zero-copy interop.
8. **Add hardware decode, phase B.** Connect decoder-owned textures directly
   to Skia: D3D resources on Windows, `CVPixelBuffer`/Metal textures on macOS,
   and VA-API/DRM PRIME/dmabuf/Vulkan images on Linux. Tie decoder frame-pool
   release to real GPU/presentation completion and retain CPU-copy fallback.
9. **Add hardware encode and capture.** Stage camera, microphone, and screen
   sources; hardware H.264/HEVC encode; mux/record; latency, bitrate, and
   key-frame controls.
10. **Expand network and streaming.** Add custom I/O and HTTP range first,
    then buffering, reconnect/discontinuity handling, and HLS/DASH or the
    justified FFmpeg protocol subset.
11. **Add an optional WebRTC-shaped realtime layer.** Define source/track/sink
    and execution-context contracts before deciding whether to port full
    WebRTC for RTP/RTCP, jitter buffering, congestion control, and AEC/NS/AGC.
12. **Expose the service through `libcrtjs`.** Start the QuickJS engine,
    event loop, timers, modules, and native binding work after step 5 while
    steps 6-8 continue in parallel. Bind media only after the extractor/codec/
    player contracts are stable, using WebCodecs-like chunks/frames/queue
    semantics and a higher-level asynchronous player API. V8 and a minimal
    Chromium/Ozone probe remain later consumers of the same contracts.

Execution order is therefore: steps 1-5 form the sequential contract gate;
Skia GPU, hardware decode, and the QuickJS core then proceed in parallel;
zero-copy completion gates the GPU-aware JavaScript media binding, but not the
initial QuickJS bring-up. A full compositor, complete font shaping, and every
codec are not prerequisites for beginning QuickJS.

## Planned

### Focused CRT/PAL follow-ups

These are real remaining limitations, but none blocks the completed
`libcrtgfx` CPU-raster milestone. Promote one into active work when a consumer
or host investigation supplies the required evidence.

- Extend the resolver from its current synchronous UDP IPv4/A-record baseline
  when IPv6, TCP fallback, search domains, or caching becomes a consumer
  requirement.
- Complete cross-process signal delivery and meaningful `SIGCHLD` `siginfo_t`
  data before enabling toybox `timeout`.
- Revisit a CRT-owned ELF loader/Android-linker boundary only after a real
  upper-runtime consumer requires behavior the host loader adapter cannot
  provide.
- Harden FreeType's fetch beyond the single SourceForge URL fix (`5b87197`)
  -- add retry-on-transient-failure, a documented fallback mirror, and
  SHA-256 verification of the cached archive before reuse, matching the
  reliability bar other `porting/recipes/*.json` ports already meet.
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
`flags.h` check); `timeout` (hang fixed, two deeper gaps remain: real
`SIGCHLD` `siginfo_t` data, cross-process `kill()`); and a confirmed-not-
guessed deferred list (`ps`/`top`/`iotop`/`pgrep`/`pkill`, `mount`/
`umount`, `ifconfig`, `login`, procfs-heavy commands).
