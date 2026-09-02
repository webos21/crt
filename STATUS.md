# Project Status

This file is the current, evidence-based project snapshot. It intentionally
does not repeat the implementation diary in [`HISTORY.md`](HISTORY.md), the
open work queue in [`TODO.md`](TODO.md), or the per-port matrix in
[`docs/porting_status.md`](docs/porting_status.md).

Last synchronized with the source tree and git history: **2026-09-02**.
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
- The core source-porting queue through curl, plus the upper-runtime FreeType
  and FFmpeg ports, has static and shared coverage on Linux, macOS, and
  Windows. The authoritative package-by-package state is
  [`docs/porting_status.md`](docs/porting_status.md).

### libcrtgfx

The software/CPU graphics baseline is complete on all three hosts:

- `crtgfx/window.h` exposes a multi-window software-frame, event, damage, and
  presentation boundary. Resize, close, focus, expose, scroll, DPI-scale, and
  asynchronous frame-complete events are part of the common contract.
- Windows uses a Win32 host adapter, macOS uses Cocoa through the Objective-C
  runtime C ABI, and Linux uses a real Wayland client speaking core protocol
  plus stable `xdg-shell`.
- `crtgfx_window_begin_frame()`/`crtgfx_window_end_frame()` present the same
  BGRA8888-premultiplied software buffer shape on every host.
- The frame contract rejects nested `begin_frame()` calls, tracks framebuffer
  generation across resize, accepts damage rectangles, and is exercised across
  repeated submissions. Linux retains each submitted `wl_shm` buffer until its
  real `wl_buffer::release`; macOS copies into host-owned presentation storage;
  Windows uploads damage rectangles into a real D3D11/DXGI flip-model swap
  chain.
- `crtgfx_window_poll_event()` delivers keyboard and pointer input on all
  three hosts. Linux uses `wl_seat`/`wl_keyboard`/`wl_pointer` with the
  project-built xkbcommon port; Windows translates window messages; macOS
  translates `NSEvent` input. The common keycode contract uses evdev-style
  keycodes and emits UTF-8 text events.
- A deterministic synthetic-event test covers ordering, queue overflow,
  per-window isolation, repeated create/destroy, and event/frame independence
  without requiring desktop input automation.
- Skia `m148` builds with the CRT sysroot and imported libc++, and renders
  into the software frame through a real CPU-raster `SkSurface`/`SkCanvas`.
- FreeType-backed text rendering uses Skia's real custom-directory font
  manager and the bundled DejaVu Sans Mono asset. The raster smoke verifies
  that `drawString()` changes pixels, and the interactive keyboard demo draws
  typed text on screen.
- The native window, repeated software presentation, keyboard/mouse input,
  Skia CPU raster, and FreeType text path have been exercised on real macOS,
  Linux, and Windows systems.
- A separate headless Skia CPU suite covers paths, transforms, clipping,
  save/restore/layers, representative shader/blend behavior, raw raster images,
  and invalid numeric/surface inputs. It does not yet claim image-codec or GPU
  coverage.

Windows now uses the GPU for final presentation, but Skia still renders into a
CPU surface and uploads it. macOS similarly hands a CPU image to a
hardware-composited layer, and Linux remains on `wl_shm`. Therefore a Skia GPU
renderer, cross-host GPU resource/fence contract, decoder-texture zero-copy,
full Wayland compositor, font-shaping stack, and Chromium Ozone backend are not
completion claims.

### libcrtmedia

The first FFmpeg-backed software-media baseline is complete on all three
hosts:

- `crtmedia_frame` describes packed RGBA/BGRA and planar YUV420P CPU video,
  including plane geometry, stride, color range/space, timestamp, and explicit
  release ownership. Deterministic conversion covers BT.601, BT.709, and
  BT.2020 limited/full-range YUV-to-RGB.
- `crtmedia_audio_buffer` describes owned interleaved S16/float PCM output.
- The synthetic CPU-frame-to-Skia bridge has passed on Linux, macOS, and
  Windows.
- The opt-in FFmpeg 8.1.2 build is LGPL-only and intentionally narrow: local
  file input, MOV/MP4/M4A plus WAV/MP3 demux support needed by the enabled
  paths, H.264 video, and AAC/MP3/PCM audio software decoders. FFmpeg types do
  not appear in public `crtmedia` headers.
- `crtmedia_demux_test` performs a real WAV/PCM demux/decode round trip and has
  passed on all three hosts. FFmpeg was re-verified on macOS after the
  `pthread_create()` fix with pthread support enabled.

This evidence does not yet prove real H.264/AAC/MP3 runtime decoding,
threaded video decode, seeking/track selection, audio-device playback,
network streaming, encoding/capture, hardware decode, or GPU-frame handoff.

### libcrtjs

`libcrtjs` currently builds and installs static/shared skeleton libraries. No
QuickJS engine or JavaScript-visible graphics/media service is integrated yet.
The intended first engine remains QuickJS; V8 remains the browser-class later
target.

### Upper Runtime Direction

- `libcrtmedia` and `libcrtgfx` next establish the extractor/codec/player and
  opaque GPU device/frame/fence contracts.
- Skia GPU rendering, FFmpeg hardware decode, and the QuickJS core then proceed
  in parallel rather than waiting for graphics/media to become indefinitely
  "complete".
- JavaScript media/gfx binding follows the stable native contracts, using a
  WebCodecs-like asynchronous shape; WebRTC-style realtime services, V8, and a
  Chromium/Ozone probe remain later layers.

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
| Multi-window creation and software frame lifecycle | `crtgfx_window_smoke` | visible animation via `crtgfx_window_demo` |
| Event ordering, overflow, isolation, and repeated lifecycle | `crtgfx_synthetic_event` | native event translation remains manually inspectable |
| Skia CPU raster and FreeType ink pixels | `crtgfx-skia-smoke` / `crtgfx_skia_raster_smoke` | visual text quality is manually inspectable |
| Broader deterministic Skia CPU drawing | `crtgfx_skia_cpu_coverage` | GPU equivalence is not implemented yet |
| Keyboard and pointer event translation | synthetic common-queue coverage plus host adapter tests | `crtgfx_keyboard_interactive` |
| Skia-backed interactive typed text | one-command `crtgfx-keyboard-interactive-skia` build | run the resulting interactive binary |
| Wayland source/toolchain integration | `crtgfx-wayland-smoke` | Linux host adapter needs a reachable compositor for the live path |

### Media Checks

| Evidence | Automated | Remaining live evidence |
| --- | --- | --- |
| CPU plane geometry, ownership, and color conversion | `crtmedia_frame_test` | none for the covered formats |
| CPU frame handoff into Skia | `crtmedia_frame_skia_smoke` | GPU texture handoff is not implemented |
| FFmpeg local-file demux/software decode | `crtmedia_demux_test` with WAV/PCM | real H.264+AAC/MP3 fixtures and audio/video playback |

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

- The Linux adapter uses project-owned wire handling and does not yet recycle
  Wayland object ids. It has been exercised on the available compositor, not
  across every compositor implementation.
- Skia coverage is CPU-only. Image codecs, shaping, fallback fonts, ICU, and
  platform font discovery are not yet completion claims.
- Windows has DXGI/D3D11 presentation, but no host has a Skia GPU render path
  or a common opaque GPU resource/fence API. Metal and Linux Vulkan/dmabuf
  paths remain future work.
- WSLg can negotiate the Wayland protocol while still differing from a normal
  Linux compositor in visible presentation behavior. It is useful evidence,
  but is not a substitute for a real Linux desktop run.

### libcrtmedia And libcrtjs

- The public media API policy is not fixed yet. The current demuxer combines
  extraction and decode and lacks packet/codec separation, seek, track
  selection, bounded asynchronous queues, and a playback session/clock.
- Only CPU frames are public. Hardware decoder surfaces, device affinity,
  fences, CPU-download fallback, and zero-copy Skia import are not defined.
- No host audio sink, network protocol, mux/encode, capture, adaptive
  streaming, or realtime/WebRTC layer exists.
- QuickJS has not been imported. Event-loop/timer/module/native-binding work
  and JavaScript media/gfx APIs remain open.

## Next Priorities

1. Decide and document the media API policy, then add real H.264+AAC/MP3
   fixtures and split extractor, packet, and codec responsibilities.
2. Build a software playback session with audio output, bounded queues, and
   A/V synchronization on all three hosts.
3. Define the opaque cross-library GPU device/surface/frame/fence contract.
4. Proceed in parallel with Skia GPU rendering, FFmpeg hardware decode, and
   QuickJS core/event-loop integration; retain software/CPU fallback as the
   correctness baseline.
5. Connect hardware decoder textures to Skia without CPU copies, then expose
   stable media/gfx services to QuickJS with WebCodecs-like queue semantics.
6. Continue closing the focused CRT/PAL limitations above when an upstream
   consumer exposes a concrete requirement, following the Bionic-first
   porting discipline in `AGENTS.md`.

Detailed actionable work belongs in [`TODO.md`](TODO.md); completed changes
belong in [`HISTORY.md`](HISTORY.md).
