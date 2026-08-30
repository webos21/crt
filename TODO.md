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

The upper-runtime work is now underway. The long-term target remains an
Electron-class rebuilt native application runtime, but the first practical
goal is narrower: a lightweight, Bionic-compatible UI/runtime stack that can
prove JavaScript, graphics, media, event-loop, filesystem, dynamic-loading,
threading, and host-window boundaries without trying to clone Electron's full
desktop API ecosystem. See [`docs/runtime_roadmap.md`](docs/runtime_roadmap.md)
and the 2026-08-17 study notes under [`docs/study/`](docs/study/).

Initial source tree shape:

```text
libcrtjs/
  include/
  src/
    *.c                        # common runtime code, directly under src/
    arch/{linux,macos,windows}/
  third_party/quickjs/

libcrtgfx/
  include/
  src/
    *.c                        # common runtime code, directly under src/
    arch/{linux,macos,windows}/
  third_party/
    skia/
    wayland/

libcrtmedia/
  include/
  src/
    *.c                        # common runtime code, directly under src/
    arch/{linux,macos,windows}/
  third_party/ffmpeg/
```

No separate `src/common/` layer in any of the three upper-runtime
libraries (2026-08-18, `libcrtgfx` first, then `libcrtjs`/`libcrtmedia`
matched the same way) -- common/host-independent runtime code lives
directly under `src/`, and only genuinely per-host code lives under
`src/arch/{linux,macos,windows}/`. See `HISTORY.md`.

Boundary decisions from `docs/study`:

- Keep `libc`/PAL focused on Bionic-compatible low-level runtime behavior.
  Graphics, JavaScript, media, host windows, GPU APIs, and application-level
  event loops stay in sibling upper-runtime libraries, not in libc.
- Start with **QuickJS** before V8. QuickJS is the smallest useful pressure
  test for event-loop, timers, module loading, native bindings, filesystem,
  dynamic loading, and process behavior. V8 waits until the C++ runtime,
  JIT/code-memory policy, atomics, threading, dynamic loading, and signal/
  exception story are stronger.
- Start `libcrtgfx` with **Skia as renderer** and a **Wayland-compatible
  boundary** as protocol/compositor vocabulary, but do not build a full
  desktop environment first. The public 2D drawing surface should expose
  normal Skia headers rather than a broad project-owned wrapper API; `crtgfx`
  owns runtime/window/surface/event/backend integration around Skia. Keep Skia
  independent from Wayland protocol parsing; host backends should map top-level
  surfaces to native windows (`HWND`, `NSWindow`, Linux
  Wayland/DRM/EGL/Vulkan/OpenGL as needed) and let Skia render directly to the
  host-appropriate GPU/software target. See
  [`docs/libcrtgfx_api_policy.md`](docs/libcrtgfx_api_policy.md).
- Treat Wayland as a compatibility boundary and future Chromium/Ozone leverage
  point, not as a reason to force Linux display-server internals into Windows
  or macOS. SDL2/GLFW/WebGPU/Vulkan-style alternatives remain fallback
  references if a host-native prototype proves the Wayland path too heavy.
  Use WSLg, Wawona/Wayoa/Cocoa-Way-style projects, Weston, wlroots, and
  Wayland protocol libraries as architecture references first; now that the
  `crtgfx` surface/frame boundary has tests, import a full compositor only for
  a concrete consumer rather than treating test availability as sufficient.
  From WSLg specifically, take the top-level-surface-to-native-window, explicit
  buffer handoff, and host-compositor presentation shape; exclude WSL/Linux
  binary execution, distro/VM packaging, RDP rail integration, and vGPU/VA-API
  dependency. See [`docs/libcrtgfx_wayland_plan.md`](docs/libcrtgfx_wayland_plan.md).
- Start `libcrtmedia` after the JS/gfx skeleton exists. FFmpeg is the first
  reference stack, with software decode first; GPU texture/audio-device handoff
  comes only after `libcrtgfx` has a real surface/frame abstraction.

The first `libcrtgfx` CPU-raster milestone is complete and no longer belongs
in this work queue. Native windows, repeated software-frame presentation,
Linux `wl_buffer::release`, keyboard/mouse input, Skia CPU raster, imported
libc++, and FreeType-backed typed text have all been verified on macOS, Linux,
and Windows. The concise current state is in [`STATUS.md`](STATUS.md); the
implementation trail is in [`HISTORY.md`](HISTORY.md).

Build reproducibility for this milestone is closed: clean-build verified on
all three hosts, FreeType's fetch now uses the stable SourceForge URL
(`5b87197`), and libffi's three-host verification (`63e07ee`) landed the same
way.

**The `libcrtgfx` window/event contract (Phase 1) is complete on all three
hosts, including real macOS hardware confirmation** -- multi-window,
resize/close/focus/expose/scroll events, key-repeat policy, queue/threading
contract, and the header-split decision, plus `CRTGFX_EVENT_DPI_SCALE_
CHANGED` delivery on Windows/Linux and the macOS `windowDidChangeBackingProperties:`
wiring (see `HISTORY.md`'s 2026-08-29/08-30 entries for the full trail; the
macOS real-hardware pass is its own 2026-08-30 entry).

**Phase 2, deterministic automated event-queue coverage, is also done**
(2026-08-30): `crtgfx_window_inject_event()` (a testing-only hook, `crtgfx/
window.h`) plus the new `crtgfx_synthetic_event` ctest target cover
ordering, the drop-newest-on-overflow policy (confirmed live at exactly 64
on both Windows and Linux), multi-window queue isolation, repeated create/
destroy (with a real `/proc/self/fd` leak check on Linux), and event-queue/
frame-cycle independence, all without needing real OS input delivery. See
`HISTORY.md`'s 2026-08-30 entry. Neither Phase 1 nor Phase 2 belong in this
work queue anymore.

**The software frame contract extension is also done** (2026-08-30):
framebuffer `generation` tracking across a resize, damage rectangles/partial
present (real per-rect `StretchDIBits`/`wl_surface::damage` on Windows/
Linux, honestly whole-frame on macOS -- confirmed on real macOS hardware to
be a genuine, understood limitation, not an oversight: a real fix would
need double/triple-buffering, which conflicts with this backend's own
tear/use-after-free-avoiding fresh-copy-per-frame design), a real
`CRTGFX_EVENT_FRAME_COMPLETE` notification (genuinely asynchronous on both
Linux, via `wl_surface::frame`/`wl_callback::done`, and macOS, via
`-[CATransaction setCompletionBlock:]` -- live-measured on real hardware,
658/658 frames in one run delivered exactly one pump cycle later, never
synchronously; synchronous only on Windows, via `StretchDIBits`), and the
producer/consumer acquire/release ownership contract now documented
explicitly on all three hosts. See `HISTORY.md`'s 2026-08-30 entries for the
full trail; not a work-queue item anymore.

Open upper-runtime work, in recommended order:

1. **Broaden deterministic Skia CPU coverage.** Path, transform, clip,
   save/restore, and layer tests; image decode/draw/scaling; one or two
   representative shaders and blend modes; error paths for NaN/Inf and
   invalid surface sizes; and the still-open focused Windows `<filesystem>`
   behavior test for UTF-32 `wchar_t` to UTF-16 path handling. Keep normal
   Skia headers as the public 2D API -- this is regression coverage, not a
   project-owned drawing facade. Treat the resulting CPU path as the golden
   reference every later GPU backend must match.
2. **Define the `libcrtmedia` CPU frame handoff contract.** A CPU video
   frame descriptor covering packed RGB/BGRA and planar YUV, per-plane
   stride/dimensions, color range/space, timestamp, and frame ownership,
   plus a CPU-only smoke that hands a synthetic RGBA/YUV frame to a Skia
   `SkImage`/`SkSurface`. This is the gate before `libcrtmedia` itself starts.

Once 1-2 land, run these tracks in parallel rather than gating one on
another:

- `libcrtmedia`: FFmpeg demux/software decode -> the CPU frame contract from
  item 3 -> audio buffer handoff.
- `libcrtgfx` GPU surface contract: an opaque GPU handle that never exposes a
  host SDK type in a public header, a backend capability query with software
  fallback, a shared lifetime/fence model across Direct3D, Metal, and Linux
  EGL/Vulkan/dmabuf, and a decision between Skia Ganesh and Graphite for the
  first real backend.
- `libcrtjs` with QuickJS (independent of graphics/media): pressure-test
  timers, module loading, filesystem, networking, dynamic loading, native
  bindings, and the common event-loop boundary before attempting V8.

Revisit Chromium/Ozone only after those three tracks produce stable
three-host evidence -- a minimal Ozone platform probe first, not a full
Chromium port. Full GPU backends, HarfBuzz/ICU font shaping, a full
Weston/other-compositor import, and platform font discovery are deliberately
not prerequisites for starting `libcrtmedia`.

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
