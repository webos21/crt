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

## Public API Shape

The public include model is organized as follows:

```text
libcrtgfx/include/
  crtgfx/
    runtime.h       # init/shutdown and backend selection
    surface.h       # native-independent surface/window handles
    event_loop.h    # pump/wake/timer integration
    skia.h          # convenience include/bridge for Skia integration

libcrtgfx/third_party/skia/
  README.md         # source/provenance and build-policy metadata

out/<preset>/external/skia/src/
  include/...       # fetched upstream Skia public headers
```

`crtgfx/skia.h` is a small bridge header, not a replacement drawing API. It
includes selected Skia public headers when available and declares helpers that
connect a `crtgfx` framebuffer to an `SkSurface`/`SkCanvas`.

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
- `libcrtgfx/third_party/skia/` deliberately stores only project-owned source
  selection, provenance, and build-policy metadata. The fetched Skia checkout,
  GN output, and installed archive are always under the active preset's
  `out/<preset>/external/skia/` tree and are never committed as an accidental
  source import.
- A successful `crtgfx-skia-build` proves that the selected Skia source builds
  with CRT headers and libraries. The separate `crtgfx-skia-smoke` target then
  stages the project-owned imported libc++, links a real Skia consumer, and
  runs CPU-raster plus FreeType-backed text checks. This complete path now
  passes on Linux, macOS, and Windows; host libc++ is not a fallback.
- The build driver is host-specific only at the tool boundary: Linux uses the
  POSIX `tools/crt-ar` response-file wrapper, while Windows uses
  `tools/crt-ar.cmd` and the selected Python interpreter plus an MSVC STL
  include-root probe. This preserves one GN policy while avoiding an accidental
  dependency on Apple `ar`, the Windows `py` launcher, or host C++ headers in
  the CRT public surface. Each route has now completed a real host GN/Ninja
  build and runnable raster smoke.

The completed first implementation milestone consists of:

1. a small `crtgfx` runtime/surface/event API;
2. a software-only frame smoke independent of Skia;
3. a CRT-built Skia and imported libc++ toolchain path;
4. normal Skia headers exposed through the sysroot;
5. a smoke that obtains an `SkCanvas` from a `crtgfx` surface and draws a
   deterministic 2D frame.

## Software Frame Contract

The project-owned software frame path is the stable boundary every host and
the current Skia CPU-raster integration obey:

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
- This is the buffer/lifetime contract used by the current Skia CPU-raster
  path and retained as the correctness baseline for future GPU backends.

## Open Questions

- Whether `crtgfx` should offer a C-only facade for non-C++ consumers later.
  This should be additive and narrow, not the primary drawing API.
- Which GPU API should be the first backend after the completed CPU-raster
  baseline, and whether Ganesh or Graphite is the better first Skia path.
- How to grow from the completed FreeType custom-directory baseline into
  HarfBuzz shaping, ICU, fallback fonts, and platform font discovery.
- How `libcrtmedia` should hand decoded frames to Skia: CPU pixel buffer first,
  then GPU texture interop after the host backend is stable.
