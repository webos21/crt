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

**Software-decode evidence is done on Linux, Windows, and macOS**
(2026-09-02): real H.264+AAC MP4 (`assets/test_video.mp4`) and MP3
(`assets/test_tone.mp3`) fixtures, `crtmedia_demux_video_test`/
`crtmedia_demux_mp3_test`/`crtmedia_demux_malformed_test` cover threaded
H.264 decode (`src/demux.c` now explicitly requests `thread_count = 2`),
PTS ordering, EOF drain/flush, and malformed-input handling (null args, a
nonexistent path, non-media bytes, a truncated real fixture) -- all real,
evidence-based checks, not link-only smoke. Full ctest suite clean on all
three hosts, **including the long-standing `crtgfx_window_smoke` WSL
failure, now genuinely fixed** (Linux 110/110, Windows 126/126, macOS
110/110, all 100%) -- see the entry directly below. macOS re-verified the
same day from real macOS hardware (`crtmedia_demux_video_test` in
particular doubling as the first real confirmation that FFmpeg's own
threaded H.264 decode actually works through this project's pthread PAL
on macOS). See `HISTORY.md`'s 2026-09-02 entry for the full trail.

**The `crtgfx_window_smoke` WSL failure, repeatedly dismissed throughout
this project's own history as "no reachable Wayland compositor," was
actually a real, fixable `memfd_create()` bug -- root-caused via `strace`,
not re-guessed, and fixed for real** (2026-09-02, found asking whether it
was really worth living with). `libc/src/mman.c`'s own `memfd_create()`
emulation (`open()` a real file, `unlink()` it immediately, keep using the
now-nameless fd) created that file at a bare relative path in the
caller's own current working directory -- on WSL, that CWD is very often
`/mnt/c/...` (DrvFs, a 9p bridge to the real Windows NTFS volume), which
does not preserve POSIX "delete-while-open" semantics the way a native
filesystem does: `open()`/`unlink()` both succeeded, but the next
`ftruncate()` on the resulting fd failed with `ENOENT`. A real Wayland/
WSLg connection was present the entire time. Fixed with a new
`memfd_tmpdir()` helper (`$TMPDIR`, then `$TEMP`/`$TMP`, then a `/tmp`
fallback -- a real `getenv()`-based directory choice, not another
CWD-relative guess) -- a first attempt hardcoded `/tmp` outright, which
then broke `memfd_create_test` on Windows for real (`/tmp` does not exist
there at all), caught by the same full-ctest-on-every-host discipline
this project already applies everywhere else. `crtgfx_window_smoke`, the
whole existing ctest suite, and `memfd_create_test` itself all verified
passing on both Linux (WSL) and Windows after the real fix -- 100% on
both hosts, no known ctest failures left on either. **Re-verified on
macOS the same day**: the WSL-specific failure mode cannot occur there
through the same path at all (`memfd_create()` is only ever called from
`libcrtgfx`'s Linux-only Wayland backend; macOS's own backend never
calls it, and real macOS/APFS honors delete-while-open correctly where
WSL's DrvFs does not) -- confirmed directly rather than only reasoned,
by running `memfd_create_test` and `crtgfx_window_smoke` for real on
macOS hardware (both pass, full ctest 110/110).

**`docs/libcrtmedia_api_policy.md`'s decided core is implemented and verified
on Linux, Windows, and macOS** (2026-09-02): `crtmedia_format` (`format.h`/`format.c`,
a real key-value store -- int32/int64/string/buffer, the last for real H.264
SPS/PPS/AAC AudioSpecificConfig codec-config data under a new
`CRTMEDIA_FORMAT_KEY_CSD` key), `crtmedia_extractor` (`extractor.h`/
`extractor.c`, demux-only -- no `AVCodecContext` at all, track selection,
raw owned `crtmedia_sample` output), and `crtmedia_codec` (`codec.h`/
`codec.c`, the real async buffer-queue decoder -- `queue_input`/
`dequeue_output`/`flush`, `CRTMEDIA_WOULD_BLOCK` backpressure, the same
explicit multi-threaded H.264 decode as `demux.c`'s own). `crtmedia_
extractor_codec_test` decodes the same real MP4 fixture `crtmedia_demux_
video_test` already covers through the older convenience API, end to end
through the new core, and gets the exact same real result (25 video
frames, correct PTS ordering, correct audio sample range) -- proving the
new layer is a real, correct alternative path, not just code that
compiles. `crtmedia_format_test` covers the key-value store deterministically.
Full ctest suite clean on all three hosts (Linux 112/112, Windows 128/128,
macOS 112/112, all 100%). macOS re-verified the same day from real macOS
hardware -- `crtmedia_extractor_codec_test` doubling as a second real
confirmation (alongside `crtmedia_demux_video_test`) that FFmpeg's own
threaded H.264 decode works through this project's pthread PAL there. See
`HISTORY.md`'s 2026-09-02 entry for the full trail.

**Still open from this same step**: `crtmedia_demuxer_*` (`demux.h`) is
still its own, separate, independent implementation -- not yet rebuilt as
a thin wrapper over the new core, deliberately deferred to keep this pass's
real regression risk to the new, isolated code only (see `demux.c`'s/
`codec.c`'s own comments). A real, working core exists to rebuild it over
whenever that becomes the next priority.

1. **Rebuild `crtmedia_demuxer_*` over the new core.** Reimplement `demux.h`'s
   existing `crtmedia_demuxer_open`/`_read`/`_close` as a thin convenience
   wrapper composing `crtmedia_extractor` + one `crtmedia_codec` per track,
   instead of its own independent FFmpeg integration -- the last piece of
   TODO.md's original "Separate extractor and codec" item. Every existing
   `crtmedia_demux_*_test` must keep passing unchanged (same public API,
   same behavior) once this lands.
2. **Build the software player.** Add play/pause/seek/stop, a monotonic master
   clock, A/V synchronization, buffering/frame-drop policy, and host audio
   sinks (WASAPI, CoreAudio, and PipeWire/ALSA) while keeping decoded video on
   the verified CPU-frame/Skia path.
3. **Fix the common GPU resource contract.** Define opaque `crtgfx` GPU
   device/surface and `crtmedia` GPU-frame objects, capability queries,
   CPU/GPU memory kinds, retain/release, device affinity, and acquire/release
   fences without exposing Direct3D, Metal, Vulkan, or FFmpeg types in public
   headers. Software fallback must remain a first-class path.
4. **Enable Skia GPU rendering.** Start with a Ganesh vertical slice and keep
   Graphite as a later measured alternative: Direct3D on Windows, Metal on
   macOS, and Vulkan/Wayland on Linux. Run the existing deterministic Skia
   drawing coverage against GPU surfaces and add resize, device-loss, and
   context-recreation tests.
5. **Add hardware decode, phase A.** Enable FFmpeg D3D11VA/D3D12VA,
   VideoToolbox, and VA-API backends, initially downloading decoded frames to
   CPU memory so codec/device selection, fallback, and recovery can be proved
   independently of zero-copy interop.
6. **Add hardware decode, phase B.** Connect decoder-owned textures directly
   to Skia: D3D resources on Windows, `CVPixelBuffer`/Metal textures on macOS,
   and VA-API/DRM PRIME/dmabuf/Vulkan images on Linux. Tie decoder frame-pool
   release to real GPU/presentation completion and retain CPU-copy fallback.
7. **Add hardware encode and capture.** Stage camera, microphone, and screen
   sources; hardware H.264/HEVC encode; mux/record; latency, bitrate, and
   key-frame controls.
8. **Expand network and streaming.** Add custom I/O and HTTP range first,
   then buffering, reconnect/discontinuity handling, and HLS/DASH or the
   justified FFmpeg protocol subset.
9. **Add an optional WebRTC-shaped realtime layer.** Define source/track/sink
   and execution-context contracts before deciding whether to port full
   WebRTC for RTP/RTCP, jitter buffering, congestion control, and AEC/NS/AGC.
10. **Expose the service through `libcrtjs`.** Start the QuickJS engine,
    event loop, timers, modules, and native binding work after step 3 while
    steps 4-6 continue in parallel. Bind media only after the extractor/codec/
    player contracts are stable, using WebCodecs-like chunks/frames/queue
    semantics (a close match to step 1's own `crtmedia_codec` shape) and a
    higher-level asynchronous player API. V8 and a minimal Chromium/Ozone
    probe remain later consumers of the same contracts.

Execution order is therefore: steps 1-3 form the sequential contract gate;
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
