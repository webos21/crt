# Project Status

This file is the current, evidence-based project snapshot. It intentionally
does not repeat the implementation diary in [`HISTORY.md`](HISTORY.md), the
open work queue in [`TODO.md`](TODO.md), or the per-port matrix in
[`docs/porting_status.md`](docs/porting_status.md).

Last synchronized with the source tree and git history: **2026-08-25**.
Updated only on explicit request from here on, not as part of routine
documentation passes -- see `TODO.md`'s Notice section. It may lag behind
`HISTORY.md`/`TODO.md` between syncs; those two are the source of truth.

## Current Baseline

### CRT/PAL

- The project provides a Bionic-compatible rebuild-oriented runtime for
  Linux, macOS, and Windows. It is not a glibc binary-compatibility layer, a
  WSL/container replacement, or an Android APK runtime.
- The default build produces static and shared forms of `libc`, `libm`,
  `libdl`, the C++ runtime, and the upper-runtime skeleton libraries. It also
  stages a compiler sysroot and an Android-like rootfs.
- Public headers and ABI policy follow Bionic/Linux shapes. Host SDK details
  stay behind per-host PAL adapters, including the Windows LLP64 boundary.
- The rootfs contains the project-built mksh and audited toybox applets. The
  same shell/toolchain environment is used by porting tests rather than
  silently falling back to the host libc or shell.
- The imported LLVM runtime path builds libc++abi and libc++ on all three
  hosts, plus project-owned libunwind on Linux and Windows. macOS deliberately
  uses libSystem's unwinder. Static and shared C++ smoke tests, including RTTI
  and exceptions, have passed on each host. Windows uses the documented
  DWARF-CFI exception policy rather than relying on a separate SEH-only C++
  runtime model.
- The source-porting queue through curl has static and shared coverage on
  Linux, macOS, and Windows. The authoritative package-by-package state is
  [`docs/porting_status.md`](docs/porting_status.md).

### libcrtgfx Milestone

The first CPU-raster graphics milestone is complete on all three hosts:

- `crtgfx/window.h` exposes the common window, software-frame, event, and
  presentation boundary.
- Windows uses a Win32 host adapter, macOS uses Cocoa through the Objective-C
  runtime C ABI, and Linux uses a real Wayland client speaking core protocol
  plus stable `xdg-shell`.
- `crtgfx_window_begin_frame()`/`crtgfx_window_end_frame()` present the same
  BGRA8888-premultiplied software buffer shape on every host.
- The frame contract rejects nested `begin_frame()` calls and is exercised
  across repeated submissions. Linux retains each submitted `wl_shm` buffer
  until its real `wl_buffer::release`; Windows and macOS copy into host-owned
  presentation storage.
- `crtgfx_window_poll_event()` delivers keyboard and pointer input on all
  three hosts. Linux uses `wl_seat`/`wl_keyboard`/`wl_pointer` with the
  project-built xkbcommon port; Windows translates window messages; macOS
  translates `NSEvent` input. The common keycode contract uses evdev-style
  keycodes and emits UTF-8 text events.
- Skia `m148` builds with the CRT sysroot and imported libc++, and renders
  into the software frame through a real CPU-raster `SkSurface`/`SkCanvas`.
- FreeType-backed text rendering uses Skia's real custom-directory font
  manager and the bundled DejaVu Sans Mono asset. The raster smoke verifies
  that `drawString()` changes pixels, and the interactive keyboard demo draws
  typed text on screen.
- The native window, repeated software presentation, keyboard/mouse input,
  Skia CPU raster, and FreeType text path have been exercised on real macOS,
  Linux, and Windows systems.

This milestone does **not** mean that a full Wayland compositor, GPU renderer,
font-shaping stack, or Chromium Ozone backend exists. The current Wayland
client and Weston-style state layer define the boundary on which those later
pieces can be built.

### Upper Runtime Direction

- `libcrtgfx`: Skia rendering, Wayland-compatible surface/compositor
  vocabulary, then GPU backends and a future Chromium Ozone path.
- `libcrtjs`: QuickJS first as a compact CRT/PAL pressure test; V8 remains the
  browser-class target.
- `libcrtmedia`: FFmpeg and software decode first, followed by audio-device
  and GPU-frame handoff through `libcrtgfx`.

The sequencing and ownership boundaries are recorded in
[`docs/runtime_roadmap.md`](docs/runtime_roadmap.md).

## Verification Model

### Default Workflow

The normal host check is:

```text
cmake --workflow --preset <host-preset>
```

It configures, builds, and runs the registered CTest suite. The CI matrix also
covers Linux aarch64/x86_64, Windows aarch64/x86_64, and macOS. Exact test
counts are intentionally not frozen in this document because adding a test
would otherwise make the status text stale; the workflow result is the source
of truth.

### Graphics Checks

| Evidence | Automated | Requires a real desktop/user |
| --- | --- | --- |
| Window creation and software frame lifecycle | `crtgfx_window_smoke` | visible animation via `crtgfx_window_demo` |
| Skia CPU raster and FreeType ink pixels | `crtgfx-skia-smoke` / `crtgfx_skia_raster_smoke` | visual text quality is manually inspectable |
| Keyboard and pointer event translation | host adapters compile; no synthetic-input regression yet | `crtgfx_keyboard_interactive` |
| Skia-backed interactive typed text | one-command `crtgfx-keyboard-interactive-skia` build | run the resulting interactive binary |
| Wayland source/toolchain integration | `crtgfx-wayland-smoke` | Linux host adapter needs a reachable compositor for the live path |

Headless Linux is allowed to report `CRTGFX_ERROR_UNSUPPORTED` for native
window creation. That verifies graceful fallback, not live presentation; a
real compositor run is required before claiming the visual/input path passed.

### Porting Checks

A port is complete only when its recipe records static and shared attempts on
each host, plus a link/run or round-trip test where meaningful. Recipes,
statuses, and exceptions are maintained in:

- [`porting/recipes/`](porting/recipes/)
- [`docs/porting_status.md`](docs/porting_status.md)
- [`docs/sysroot_ports.md`](docs/sysroot_ports.md)

## Known Limitations

### CRT/PAL

- The DNS resolver is intentionally small: synchronous UDP A-record lookup,
  without complete IPv6, TCP fallback, search-domain, or caching behavior.
- Toybox `timeout` remains disabled until cross-process signal delivery and
  meaningful `SIGCHLD` `siginfo_t` data are complete.
- Interactive POSIX job control remains deferred. The project mksh build does
  not claim full foreground/background stop/resume semantics; see
  [`docs/job_control.md`](docs/job_control.md).
- Some console environments cannot provide a real screen buffer for
  `TIOCGWINSZ`; tty behavior and remaining applet restrictions are tracked in
  [`docs/toybox_applet_status.md`](docs/toybox_applet_status.md).
- Windows static archives containing constructor sections can still require a
  recipe-specific retention policy because PE/COFF archive extraction does
  not behave like a GNU ELF linker script.
- Linux `libdl` remains a documented boundary rather than a CRT-owned general
  ELF loader. A full Android-style linker is a separate long-term tranche.

### libcrtgfx

- The common dispatch shape currently assumes one active top-level window per
  process. Multi-window display ownership and routing are not complete.
- The Linux adapter uses project-owned wire handling and does not yet recycle
  Wayland object ids. It has been exercised on the available compositor, not
  across every compositor implementation.
- Skia coverage currently proves CPU raster, basic painting, and FreeType text.
  Broader paths, images, clipping/layers, shaders, shaping, fallback fonts,
  ICU, and platform font discovery are not yet completion claims.
- Keyboard/pointer translation has live three-host evidence but no automated
  synthetic-input injector yet; the interactive test remains the behavioral
  verification path.
- GPU presentation is not implemented: Direct3D on Windows, Metal on macOS,
  and EGL/Vulkan/dmabuf paths on Linux remain future work.
- WSLg can negotiate the Wayland protocol while still differing from a normal
  Linux compositor in visible presentation behavior. It is useful evidence,
  but is not a substitute for a real Linux desktop run.

## Next Priorities

1. Add deterministic common-event/input coverage, then expand Skia CPU-raster
   coverage beyond the initial text and fill smoke while preserving normal
   Skia headers as the drawing API.
2. Define the GPU buffer/surface handoff contract, then add one backend at a
   time while retaining the software path as the correctness baseline.
3. Start QuickJS integration in `libcrtjs` to exercise event-loop, module,
   filesystem, timer, and native-binding boundaries.
4. Start FFmpeg software decode in `libcrtmedia`, followed by audio output and
   decoded-frame handoff to `libcrtgfx`.
5. Continue closing the focused CRT/PAL limitations above when an upstream
   consumer exposes a concrete requirement, following the Bionic-first
   porting discipline in `AGENTS.md`.

Detailed actionable work belongs in [`TODO.md`](TODO.md); completed changes
belong in [`HISTORY.md`](HISTORY.md).
