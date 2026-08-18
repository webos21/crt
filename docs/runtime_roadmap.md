# Runtime Roadmap

This document records the long-term target above the current libc/PAL and
porting-test baseline.

## Target Shape

The long-term product target is an Electron-class native application runtime:
a rebuilt application stack should be able to use one common CRT/PAL base on
Linux, macOS, Windows, and Android, with graphics, media, JavaScript, dynamic
loading, process, file, socket, and threading behavior provided by this project
rather than by ad hoc per-application ports.

This does not mean importing Electron itself as the next task. Electron is the
reference level of integration: a browser/JavaScript/application-shell style
runtime with enough graphics, media, networking, process, filesystem, and
extension surface to host large applications.

## Current Baseline

The current baseline is still the Bionic-compatible CRT/PAL:

- libc/libm/libdl/libc++ bootstrap libraries;
- shared-library artifacts for all hosts;
- Android-like rootfs, mksh, and toybox applet environment;
- configure/make porting flow through CRT wrappers;
- verified source-port queue through curl on Linux, macOS, and Windows.

Before starting the upper runtime in earnest, the remaining planned libc/PAL
items in `TODO.md` should be reduced. The upper runtime should expose new gaps,
but it should not be used to avoid known low-level work such as process, fd,
rootfs, resolver, console, and symbol-export hygiene.

## Upper Libraries

### libcrtgfx

`libcrtgfx` is the graphics and compositor layer.

Initial direction:

- Skia as the primary 2D rendering engine.
- Normal Skia headers as the public 2D drawing API; project-owned `crtgfx`
  headers should cover runtime, surface/window, frame presentation, event-loop,
  backend, and compositor integration rather than wrapping Skia's drawing
  model. See `docs/libcrtgfx_api_policy.md`.
- A project-owned Wayland-compatible compositor boundary as the application and
  toolkit-facing display protocol. Use existing projects such as Wayland
  protocol libraries, Weston/wlroots, WSLg, and Wawona/Wayoa-style host-native
  compositors as architecture references before importing source. WSLg is a
  reference for surface/window/buffer handoff structure only, not for WSL,
  Linux binary execution, RDP rail, or vGPU dependencies; see
  `docs/libcrtgfx_wayland_plan.md`.
- Chromium Ozone backend work as the long-term browser integration path.
- Host backends hidden below the graphics PAL:
  - Linux: Wayland/DRM/EGL/Vulkan/OpenGL paths as they become necessary.
  - macOS: native windowing/GPU adapters behind the same compositor boundary.
  - Windows: native windowing/GPU adapters behind the same compositor boundary.
  - Android: align with Android graphics assumptions where that helps the
    Bionic-compatible shape.

The first goal is not a full desktop environment. It is a stable graphics
contract that source-rebuilt toolkits or browser components can target without
reimplementing the OS split themselves.

Current bring-up starts with real host windows before drawing:

- `include/crtgfx/window.h` defines the first host-independent window surface
  API.
- `libcrtgfx/src/wayland_weston.c` carries the first Weston-style
  toplevel/surface state, directly under `src/` (no separate `src/common/`
  layer). Each host maps that state to a native host window under
  `libcrtgfx/src/arch/{linux,macos,windows}/`.
- `crtgfx_window_begin_frame()`/`crtgfx_window_end_frame()` provide the first
  software buffer commit/present path, using BGRA8888 premultiplied pixels.
- `libcrtgfx`, `libcrtjs`, and `libcrtmedia` are default workflow artifacts:
  they build as static/shared libraries, install into the sysroot, and their
  shared runtime files are copied into the Android-like rootfs.
- Common upper-runtime code links against this project's CRT libraries
  (`libc`, `libm`, `libdl`, `libc++`). Only the narrow host backend layer is
  allowed to call host window/GPU APIs directly.
- The next graphics step is Skia drawing onto the same software surface,
  followed later by GPU-backed present paths.

### libcrtmedia

`libcrtmedia` is the audio/video/media layer.

Initial direction:

- FFmpeg as the main demux/decode/encode reference stack.
- Codec libraries kept as explicit dependencies rather than hidden inside libc.
- Audio output abstraction per host.
- Video frame and GPU texture handoff designed to work with `libcrtgfx`.
- Clear policy for hardware acceleration later; software decode is the first
  portability baseline.

The first useful milestone is file/network media decode into frames and audio
buffers using CRT files, sockets, threads, atomics, memory mapping, and dynamic
loading.

### libcrtjs

`libcrtjs` is the JavaScript/application scripting layer.

Initial direction:

- Start with QuickJS because it is small, C-oriented, and suitable for exposing
  CRT/PAL gaps quickly.
- Keep V8 as the final target for browser-class runtime compatibility.
- Treat QuickJS as a bring-up engine, not as a substitute for V8.
- Grow event loop, timers, module loading, filesystem, networking, and native
  binding policy against the CRT/PAL before attempting V8.

V8 should be attempted only after the C++ runtime, threading, atomics, memory
mapping, signal/process behavior, JIT/code-memory policy, and dynamic loading
story are strong enough to make failures actionable.

## Order Of Work

1. Finish the remaining planned libc/PAL cleanup in `TODO.md`.
2. Stabilize the porting-test discipline: every new port should verify static
   and shared builds in the same pass on each host, or document why not.
3. Bring up `libcrtgfx` first: native host window, software frame present,
   then Skia drawing.
4. Define the Wayland-compatible compositor boundary and host window/GPU
   adapters as the `libcrtgfx` surface contract becomes concrete.
5. Add QuickJS as the first `libcrtjs` bring-up target.
6. Add FFmpeg as the first `libcrtmedia` target.
7. Revisit Chromium/Ozone and V8 after the lower layers have produced enough
   passing evidence.

## Non-Goals For This Phase

- Do not port Electron itself first.
- Do not patch large upstreams as the default solution.
- Do not expose host SDK headers as public CRT ABI.
- Do not make libcrtgfx/libcrtmedia/libcrtjs part of libc.
- Do not claim Android framework or APK compatibility.

