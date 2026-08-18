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
   - Current status: macOS workflow passed locally on 2026-08-18 after the
     lifecycle/test changes; Linux Wayland backend passed C99/`-Werror`
     syntax checking from macOS. Real Linux and Windows workflow/demo
     verification is still required before this item moves to `HISTORY.md`.
2. **Connect the software frame path to Skia CPU raster drawing.**
   Keep normal Skia headers as the public 2D drawing API and keep
   project-owned headers focused on runtime/surface/present/event integration.
   Start with a deterministic CPU-raster `SkSurface`/`SkCanvas` smoke before
   any GPU backend.
   - Current status: `crtgfx/skia.h`, `src/skia_bridge.cc`, and
     `crtgfx_skia_raster_smoke` build wiring exist. Skia `m148` now builds and
     installs as a CRT-toolchain CPU archive on macOS; `tools/crt-ar` expands
     GN response files so this does not depend on Apple `ar` supporting them.
     No fake Skia headers are provided.
   - Current build automation: `crtgfx-skia-fetch`,
     `crtgfx-skia-configure`, and `crtgfx-skia-build` now exist. The default
     source track is Skia `m148` (`refs/heads/chrome/m148`), with
     `CRTGFX_SKIA_VERSION`, `CRTGFX_SKIA_REF`, and
     `CRTGFX_SKIA_EXPECTED_COMMIT` available for user pinning.
   - The first exposed C++ gap, CRT-owned `operator new/delete`, is complete
     and covered by `cxx_allocation_test`. The remaining gate is a real
     project-owned libc++ standard-library import: the default Skia archive
     uses `std::string`, shared ownership, streams, and locale even with GPU
     disabled. `CRTGFX_ENABLE_SKIA` therefore remains OFF by default; do not
     link host libc++ as a substitute. After libc++ is imported, verify the
     deterministic `crtgfx_skia_raster_smoke` on static and shared paths on all
     three hosts.
   - Cross-host build-driver status: the Linux route selects the POSIX
     `tools/crt-ar` wrapper; the Windows route selects `tools/crt-ar.cmd`,
     passes the invoking Python through `CRT_HOST_PYTHON`, and recognizes the
     MSVC STL include root. Those routes were statically checked on macOS, but
     their real host GN/Ninja workflows still require execution before Skia is
     reported as cross-host verified.
   - **C++ runtime prerequisite (started):** `crt-libcxx-fetch` now obtains
     Android's paired libc++, libc++abi, and libunwind sources under the active
     preset. Build them as one CRT static/shared runtime set and replace the
     small Bionic-shaped bootstrap only after standard-library and Skia
     link/run tests pass on all three hosts.
   - First real macOS libc++ `cxx` compile was run against Android main
     (`4f4a65c` libc++, `6571517` libc++abi, `ec57b05` libunwind). It exposed
     the next CRT work honestly: finish the Bionic/FreeBSD C99 libm family
     (`erf*`, `erfc*`, `exp2*`, `fdim*`, `hypot*`, `ilogb*`, `lgamma*`, and
     remaining siblings), configure libc++ for the CRT pthread personality,
     and disable libc++'s pre-C++14 `gets` import rather than reintroducing
     that removed unsafe API. macOS-only `Availability.h` is likewise an
     external-build configuration issue, not a CRT public header.
3. **Run the Wayland/Weston protocol/library investigation as a separate
   `libcrtgfx` sub-track.**
   Decide what is protocol parsing, what is compositor policy, and what is
   host-native window/GPU adapter code. Study Weston/wlroots/Wayland protocol
   sources before importing code; import protocol XML/generated helpers only
   when a tested boundary requires them.
4. **Add input/event delivery to the `libcrtgfx` surface contract.**
   Cover close, resize, focus, pointer, and keyboard shape across Linux
   Wayland, Win32, and Cocoa. Keep OS-native event details behind
   `src/arch/{linux,macos,windows}`.
5. **Extend Skia integration beyond primitive CPU drawing.**
   Add image/font/text staging after the CPU-raster surface smoke is stable.
   Treat HarfBuzz/FreeType/ICU/platform-font discovery as explicit follow-up
   dependencies, not hidden Skia side effects.
6. **Add GPU and media handoff only after the frame/input contract is stable.**
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
