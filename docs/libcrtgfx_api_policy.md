# libcrtgfx API Policy

This document records the initial API boundary decision for `libcrtgfx`.

## Decision

Expose Skia as the public 2D graphics API.

`libcrtgfx` should not invent a parallel 2D drawing API for paths, paints,
fonts, images, canvases, shaders, or text layout unless a concrete porting need
proves that Skia cannot be exposed directly. Source code that already knows
Skia should be able to include the normal Skia headers and use ordinary Skia
types.

The project-owned `crtgfx` API should stay around the parts Skia deliberately
does not standardize for this project:

- runtime initialization and shutdown;
- host window/surface creation;
- frame lifecycle and presentation;
- event-loop integration;
- software/GPU backend selection;
- Wayland-compatible compositor boundary and host adapter policy;
- handoff points for `libcrtjs` and `libcrtmedia`.

In short: Skia owns drawing. `libcrtgfx` owns where the drawing goes, how it is
presented, and how it crosses the host/platform boundary.

## Rationale

This project is rebuild-based source portability, not a frozen binary SDK.
Exposing Skia headers fits that model better than wrapping Skia behind a new
2D API:

- Skia is already the common vocabulary used by Chromium, Android-adjacent
  graphics code, Flutter-adjacent code, and many native UI stacks.
- A project-owned 2D wrapper would have to chase a large surface area:
  canvases, paths, paints, matrices, images, codecs, color spaces, fonts,
  text shaping, GPU contexts, shaders, and resource lifetime rules.
- Wrapping too early would hide useful build failures. If Skia cannot build or
  run against this CRT, that is a direct signal about the missing C++ runtime,
  atomics, threading, memory mapping, dynamic loading, font, image, or GPU
  boundary work.
- Skia's C++ ABI instability is acceptable here because consumers are rebuilt
  against the same sysroot and Skia build. The project should not promise a
  stable cross-version binary ABI for Skia objects.

## Non-Goals

- Do not expose host SDK headers as the public graphics API. Win32, Direct3D,
  Cocoa, Metal, Wayland platform headers, EGL, Vulkan, and OpenGL details stay
  behind `libcrtgfx` host adapters unless a specific third-party source is
  being ported as a documented exception.
- Do not make Skia part of `libc`, `libm`, `libdl`, or the low-level PAL.
- Do not create a full Electron-compatible application API in this tranche.
- Do not define a broad custom drawing abstraction before a real Skia bring-up
  demonstrates what is actually needed.

## Planned Shape

The public include model should look roughly like this:

```text
libcrtgfx/include/
  crtgfx/
    runtime.h       # init/shutdown and backend selection
    surface.h       # native-independent surface/window handles
    event_loop.h    # pump/wake/timer integration
    skia.h          # convenience include/bridge for Skia integration

libcrtgfx/third_party/skia/
  include/...       # upstream Skia public headers
```

`crtgfx/skia.h` should be a small bridge header, not a replacement drawing API.
It may include selected Skia public headers and declare helpers that connect a
`crtgfx_surface` to an `SkSurface`/`SkCanvas` once Skia is imported.

Current bridge status (2026-08-18):

- `libcrtgfx/include/crtgfx/skia.h` is a C++ bridge header. When real Skia
  headers are present, it includes Skia's normal `include/core/SkSurface.h`
  path and exposes `crtgfx_skia_make_raster_surface()`.
- `libcrtgfx/src/skia_bridge.cc` wraps the current BGRA8888 premultiplied
  `crtgfx_framebuffer` as a Skia CPU raster surface via
  `SkSurfaces::WrapPixels()`.
- `crtgfx_skia_raster_smoke` is registered only when
  `CRTGFX_ENABLE_SKIA=ON` and a real Skia checkout is available through
  `CRTGFX_SKIA_ROOT`; this project deliberately does not provide fake Skia
  headers just to make the target compile.
- When enabled, Skia's public `include/` tree is installed into the sysroot so
  consumers can include normal Skia headers through the same root-style include
  path Skia source uses.
- Skia source/build automation is intentionally separate from ordinary
  third-party porting recipes because Skia is a core `libcrtgfx` dependency:
  `crtgfx-skia-fetch` fetches a milestone/ref, `crtgfx-skia-configure`
  generates GN args with `tools/crt-cc`/`tools/crt-c++`, and
  `crtgfx-skia-build` builds and installs the CPU-raster Skia library into
  `CRTGFX_SKIA_INSTALL_PREFIX`.
- Default source selection is the Chrome/Skia stable milestone branch
  `m148`. Users can override it with `CRTGFX_SKIA_VERSION`; use
  `CRTGFX_SKIA_REF` plus `CRTGFX_SKIA_EXPECTED_COMMIT` when a fully pinned
  reproducible checkout is required.

The first implementation milestone should therefore be:

1. create a tiny `crtgfx` runtime/surface API;
2. keep the first smoke software-only and independent of Skia where useful;
3. import/build Skia;
4. expose Skia headers through the `libcrtgfx` include/install path;
5. add a smoke that obtains an `SkCanvas` from a `crtgfx` surface and draws a
   deterministic 2D frame.

## Software Frame Contract

Before Skia is imported, the project-owned software frame path is the stable
boundary every host must obey:

- `crtgfx_window_begin_frame()` returns one writable BGRA8888 premultiplied
  framebuffer for the current frame.
- Nested `begin_frame()` calls are invalid; callers must either submit with
  `crtgfx_window_end_frame()` or destroy the window.
- `crtgfx_window_end_frame()` transfers the rendered frame to the host
  presentation backend. After it returns, callers should treat the submitted
  contents as no longer theirs to mutate.
- A backend may copy pixels immediately into host-owned storage (current Win32
  and Cocoa policy) or retain submitted storage until the real compositor
  releases it (current Linux Wayland `wl_buffer::release` policy).
- This is the buffer/lifetime contract Skia CPU raster will attach to first.

## Open Questions

- Whether `crtgfx` should offer a C-only facade for non-C++ consumers later.
  This should be additive and narrow, not the primary drawing API.
- Which Skia subset should be exposed first: CPU raster only, Ganesh GPU,
  Graphite GPU, or a staged CPU-first/GPU-later plan.
- How Skia text/font dependencies should be staged, especially HarfBuzz,
  FreeType, ICU, and platform font discovery.
- How `libcrtmedia` should hand decoded frames to Skia: CPU pixel buffer first,
  then GPU texture interop after the host backend is stable.
