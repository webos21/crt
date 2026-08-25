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


## Done

See [`HISTORY.md`](HISTORY.md) for the full, dated, reverse-chronological
record of completed work. This section stays empty in `TODO.md` itself --
when an item below is finished, move its writeup into `HISTORY.md` (dated,
newest entry first) rather than leaving it here.

## In Progress

Active threads, not a flat list of one-off items.

### Upper runtime roadmap

The upper-runtime work is now starting. The long-term target remains an
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
  Wayland protocol libraries as architecture references first; do not vendor a
  full compositor until the `crtgfx` surface/frame boundary has tests. From
  WSLg specifically, take the top-level-surface-to-native-window, explicit
  buffer handoff, and host-compositor presentation shape; exclude WSL/Linux
  binary execution, distro/VM packaging, RDP rail integration, and vGPU/VA-API
  dependency. See [`docs/libcrtgfx_wayland_plan.md`](docs/libcrtgfx_wayland_plan.md).
- Start `libcrtmedia` after the JS/gfx skeleton exists. FFmpeg is the first
  reference stack, with software decode first; GPU texture/audio-device handoff
  comes only after `libcrtgfx` has a real surface/frame abstraction.

Current baseline, completed on Windows first and recorded in
[`HISTORY.md`](HISTORY.md):

- `libcrtgfx`, `libcrtjs`, and `libcrtmedia` exist as default workflow,
  sysroot, and rootfs runtime artifacts. They build static/shared libraries
  and the shared runtime files are copied into `/system/lib` and `/usr/lib`.
- Common upper-runtime code links through this project's CRT libraries
  (`libc`, `libm`, `libdl`, `libc++`). Only narrow host backend objects are
  allowed to speak native OS window/GPU APIs directly.
- `libcrtgfx` now has real host adapters on all three targets:
  `include/crtgfx/window.h` flows through
  `src/wayland_weston.c`'s Weston-style toplevel/surface state, with
  per-host code underneath -- Win32 (`src/arch/windows/window_win32.c`),
  a hand-rolled core-protocol-Wayland+`xdg-shell` client
  (`src/arch/linux/window_wayland.c`), and real Cocoa driven from C via
  the Objective-C runtime, no `.m` file
  (`src/arch/macos/window_cocoa.c`, 2026-08-18) -- see
  `docs/libcrtgfx_wayland_plan.md`'s "Linux Host Adapter"/"macOS Host
  Adapter" sections for what each covers, its documented scope cuts, and
  how it was verified on real hardware/a real compositor session.
- `crtgfx_window_begin_frame()`/`crtgfx_window_end_frame()` provide the first
  BGRA8888 software buffer commit/present path. `crtgfx_window_smoke` covers
  automation and `crtgfx_window_demo` covers manual bring-up.
- **Real keyboard/mouse input now flows through `crtgfx/window.h`'s public
  `crtgfx_window_poll_event()` API on all three targets (2026-08-25,
  the "notepad-capability" plan's phases 2/3 -- see `HISTORY.md`'s
  matching dated entries).** Linux: a real `wl_seat`/`wl_keyboard`/
  `wl_pointer` client (`src/arch/linux/window_wayland.c`) plus a new
  `libcrtgfx/third_party/xkbcommon` port turning `(keycode, modifier
  state)` into real composed UTF-8 text -- verified live against a real
  compositor (actual typing/mouse activity captured as real
  `KEY_DOWN`/`KEY_UP`/`TEXT`/`POINTER_MOTION`/`POINTER_BUTTON_*` events).
  Windows: real `WM_KEYDOWN`/`WM_CHAR`/mouse-message handling in
  `src/arch/windows/window_win32.c` -- verified live on this project's
  own native Windows host (real typed "asdf" and mouse clicks captured
  correctly). macOS: real `NSEventTypeKeyDown`/`FlagsChanged`/mouse-
  event handling in `src/arch/macos/window_cocoa.c`, implemented as
  "reasoned but flagged unverified" (no macOS host access this session)
  and now confirmed working live on real macOS hardware by the user
  (2026-08-25) -- typed text and mouse activity captured correctly,
  matching Linux/Windows. All three targets are real-hardware verified.
- **Skia's real FreeType-backed font manager (`SkFontMgr_New_Custom_
  Directory`, pointed at the bundled `libcrtgfx/assets/fonts/
  DejaVuSansMono.ttf`) is now verified end to end on all three targets**
  (`crtgfx_skia_raster_smoke: ok`, a real `canvas->drawString()`
  producing real ink pixels): Windows and macOS were already verified;
  **Linux was the last one, unblocked 2026-08-25 by fixing this
  project's own imported static `libc++.a`** (see the next bullet) --
  previously blocked entirely (`CRTGFX_ENABLE_SKIA` had to stay `OFF` on
  Linux). `crtgfx_keyboard_interactive` (a manual, non-ctest demo
  binary) now also draws real typed text on screen this same way when
  built with `CRTGFX_ENABLE_SKIA=ON`, falling back to a plain gradient
  fill otherwise.
- **This project's own imported static `libc++.a` is now correct on
  Linux (2026-08-25).** Two real, compounding bugs, both closed: (1)
  the previously-reported "only 3 archive members, no locale/iostream
  objects at all" was a stale build artifact, not a structural bug --
  a genuinely fresh rebuild produces a full, correct archive with no
  source changes needed; (2) fixing that exposed a real `__dso_handle`
  multiple-definition conflict between `libc/src/arch/linux/common/
  cxa_atexit.c`'s own hidden-visibility copy and a shim `libcxx`/
  `libcxxabi` inject for macOS (whose `crt1.o` has no equivalent at
  all) -- fixed at the CMake level, excluding the shim only from each
  library's *static* object list on Linux (the *shared* `.so` targets
  still need their own copy, since a hidden-visibility symbol can never
  resolve across a shared-object boundary). Verified via a genuinely
  clean, from-scratch rebuild and a real static-linked program using
  `<iostream>`/`<locale>`/`<sstream>`, which ran and printed correct
  output. Both `CRT_USE_IMPORTED_LIBCXX=ON` and the default `OFF`
  config pass the full `ctest` suite with no regressions.

Next work order:

1. **Lock the `libcrtgfx` software frame lifecycle.**
   - Define the meaning of `begin_frame()`/`end_frame()` around buffer
     ownership, resize, repeated frame submission, and when a submitted buffer
     may be reused or released.
   - Make Linux Wayland honor real `wl_buffer::release` before freeing a
     submitted `wl_shm` buffer. Windows/macOS already copy the submitted frame
     into host-owned presentation storage, so they satisfy the same contract
     through a different backend policy.
   - Expand `crtgfx_window_smoke` from a single-frame smoke into a small
     repeated-frame lifecycle check, including rejecting nested
     `begin_frame()` calls.
   - Verification rule: run the full workflow on macOS/Linux/Windows and run
     the visible demo on each host when a real desktop/compositor is available.
   - Current status: the basic single-frame workflow itself has since been
     exercised live on all three hosts many times over, via the keyboard/
     mouse input work and the Skia raster smoke gate (both 2026-08-25) --
     but this item's own actual remaining scope, honoring real
     `wl_buffer::release` on Linux and expanding `crtgfx_window_smoke` into
     a repeated-frame lifecycle check, has not been touched yet.
2. **Connect the software frame path to Skia CPU raster drawing.**
   Keep normal Skia headers as the public 2D drawing API and keep
   project-owned headers focused on runtime/surface/present/event integration.
   - Status: the deterministic CPU-raster smoke gate is done. Skia `m148`
     builds as a CRT-toolchain CPU archive (via this project's own imported
     libc++) and links on all three hosts; `crtgfx_skia_raster_smoke`'s real
     FreeType-backed font manager (`SkFontMgr_New_Custom_Directory`)
     produces real ink pixels on Windows, macOS, and Linux -- Linux was
     last, unblocked 2026-08-25 by the imported static `libc++.a` fix (see
     the "Current baseline" bullets above). `crtgfx_keyboard_interactive`
     now also renders real typed text the same way. One-command build
     targets exist for both (`crtgfx-skia-smoke`,
     `crtgfx-keyboard-interactive-skia`), and `tools/test_crtgfx_skia_
     smoke.py` builds any real crtgfx executable target the same way via
     `--target`/`--no-run`. The full build/bug-fix trail (Skia fetch/pin,
     GN/Ninja toolchain wiring across all three hosts, the imported-libc++
     port, the `<bit>`/`<inttypes.h>` libc gaps, the Windows DWARF-unwind
     safety net) lives in `HISTORY.md` and is not repeated here.
   - Still open: real 2D drawing coverage beyond the one smoke binary
     (paths, images, shaders, clipping/layers -- anything past
     `drawString()`/a raster fill), a GPU backend (tracked separately as
     item 3 below), and a real, still-open Wayland `present_software`
     connectivity difference between different shell contexts on the same
     WSL host (unrelated to the smoke test itself, which passes cleanly
     through the real `ctest`-driven run).
   - A dedicated Windows `<filesystem>` behavior test (especially UTF-32
     `wchar_t` to native UTF-16 path conversion) remains worthwhile before
     claiming that imported-libc++ API family's runtime semantics are
     fully covered.
3. **Add GPU and media handoff only after the frame/input contract is stable.**
   Windows D3D, macOS Metal, Linux EGL/Vulkan/dmabuf, and `libcrtmedia`
   decoded-frame/audio handoff are later optimization/integration tranches,
   not prerequisites for the first Skia raster milestone.

## Planned

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
