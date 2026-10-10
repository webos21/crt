# Project Status

This file is the current, evidence-based project snapshot. It intentionally
does not repeat the implementation diary in [`HISTORY.md`](HISTORY.md), the
open work queue in [`TODO.md`](TODO.md), or the per-port matrix in
[`docs/porting/porting_status.md`](docs/porting/porting_status.md).

Last synchronized with the source tree and git history: **2026-10-10**.
Updated only on explicit request from here on, not as part of routine
documentation passes -- see `TODO.md`'s Notice section. It may lag behind
`HISTORY.md`/`TODO.md` between syncs; those two are the source of truth.

## Current Baseline

### CRT/PAL

- The project provides a Bionic-compatible rebuild-oriented runtime for
  Linux, macOS, and Windows. It is not a glibc binary-compatibility layer, a
  WSL/container replacement, or an Android APK runtime.
- The default workflow builds and tests only the C stage. Explicit cumulative
  distributions then add C++, Simple Graphics, advanced Graphics/Media, and
  `05-ui` (`crtui`, accepted on all three hosts) under `out/<preset>/dist/`.
  `06-web` is in bring-up with Tranches 0-2 closed, but has no product SDK yet
  (the superseded `05-js`/`libcrtjs` skeleton was removed on 2026-09-29).
- No distribution bundles LLVM/Clang/LLD. Desktop and embedded consumers
  provide the host or vendor toolchain; CRT supplies the staged sysroot,
  startup/runtime objects, wrappers, configuration, and manifest.
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
  [`docs/porting/porting_status.md`](docs/porting/porting_status.md).
- The current project-owned allocator remains the bootstrap/reference
  implementation. Windows/x86_64, macOS/arm64, and Linux/aarch64 have current-
  schema baseline/contention data; Linux also passed focused correctness,
  fragmented-fork, diagnostic-fault, and Host ABI firewall validation. No
  tested host triggered the replacement envelope, so Scudo remains a
  conditional production candidate rather than a selected replacement.

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
  and invalid numeric/surface inputs.
- The common opaque GPU device/surface/frame/fence contract is implemented.
  Ganesh renders through Vulkan on Linux, D3D12 on Windows, and Metal on
  macOS. The low-level GPU presentation path and resize/swapchain recreation
  have been verified on all three hosts. Packaged Skia/Ganesh live
  presentation now passes on Linux/aarch64 after removing a mixed static/
  shared CRT C++ runtime link from the standalone example; Windows and macOS
  also pass the packaged live path. The earlier CRT-libc/glibc symbol
  collision remains fixed with CRT's private `CRT_1.0` ELF namespace on both
  aarch64 and x86_64. The final fresh Linux/aarch64 run selected lavapipe and
  passed the live Skia presentation three times with Vulkan validation off and
  three times with validation on; the acquired-image semaphore is explicitly
  consumed by Ganesh before rendering. The prebuilt shared-runtime packaged
  binary (`examples/bin/crtgfx_skia_gpu_window_demo`, distinct from the
  standalone example rebuilt from source above) also presents live on
  Linux/aarch64 now, after giving `libdl.so` the same `CRT_1.0` ELF
  symbol-version namespace as `libc.so`; a direct packaged-binary smoke
  covers it in the isolated 04-stage acceptance run. Live Ganesh/Vulkan
  presentation is now also verified on a **physical** Linux GPU (Intel UHD
  630, native x86_64 Ubuntu desktop, 2026-09-22), passing the same
  pixel-exact/mid-stream-resize contract (`resize_frame=2 pixel_check=pass
  post_resize_present=pass`) as the isolated `04-gfx-media` stage's own
  acceptance run; the Linux/aarch64 VM (lavapipe) remains the recorded
  cross-host stage-build baseline.
- The `crtgfx_gpu_device`/`crtgfx_gpu_surface` backend-object boundary is
  hardened and closed (2026-09-17..18, [`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md)): both are now a fixed
  common representation (refcount/backend tag/ops table/opaque backend-state
  pointer) with no `CRTGFX_HAVE_*`-conditional layout, dispatched through a
  per-backend operations table; Vulkan/D3D12/Metal concrete device/surface
  state lives only inside its own owner. Skia and every generic test reach
  native state only through narrow borrowed views/transitions or a private,
  backend-neutral test-control surface -- never a native handle or private
  struct layout directly. Verified with a source-boundary regression guard
  plus a bounded `crtgfx-boundary-acceptance` gate (fresh cumulative `02-cxx`
  dist + binary-import/public-ABI/Host-ABI-firewall/backend-boundary/GPU/
  synthetic-event checks in an enabled tree, then static+shared common
  wrappers with no concrete backend object in a clean
  `CRTGFX_ENABLE_GPU_BACKEND=OFF` tree) green on macOS/arm64, Linux/aarch64,
  and Windows/x64.
- Machine-checkable live-presentation pixel evidence is closed on every
  required host against one frozen, backend-neutral acceptance contract
  (`docs/acceptance/libcrtgfx_live_presentation_acceptance.md`, 2026-09-18): a real
  `SkSurface::readPixels()` check against the shared reference scene on the
  live swapchain/layer-backed Ganesh surface, for both a plain 5-frame run
  and a 5-frame run with a scripted mid-stream `900x520` resize (the second
  presented frame is always the first to reflect the new extent). Passing on
  Linux/aarch64 (Vulkan, reference host), macOS/arm64 (Metal, confirming the
  `backing_size`-authoritative rule against real Retina `900x520 ->
  1800x1040` scaling), and Windows/x64 (D3D12, `backing_size` matching
  `requested_size` 1:1 on this session's non-scaled display). macOS/x86_64
  native live execution was retired from the acceptance matrix (Apple
  Silicon/macOS 27+ only) rather than left as an unverified gap.

Full font shaping/fallback/ICU, a full Wayland compositor, and a Chromium
Ozone backend are not completion claims.

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
  file input, MOV/MP4/M4A plus WAV/MP3 demux support, H.264 and MPEG-4 video
  decode, AAC/MP3/PCM audio decode, the built-in MPEG-4 software video encoder,
  and MP4 muxing. FFmpeg types do not appear in public `crtmedia` headers.
- `crtmedia_demux_test` performs a real WAV/PCM demux/decode round trip and has
  passed on all three hosts. FFmpeg was re-verified on macOS after the
  `pthread_create()` fix with pthread support enabled.
- The public format/extractor/codec split is implemented. The extractor owns
  demux-only track/sample access, while asynchronous codec queues own software
  decode/encode and explicit output-buffer release. `crtmedia_muxer` owns the
  independent MP4 track/start/write/finish lifecycle.
- `crtmedia_player` supplies the software playback state machine, clocks,
  seek/reset behavior, bounded render planning, and CPU-frame delivery.
- Host audio sinks are implemented and exercised through WASAPI on Windows,
  raw ALSA or PulseAudio-compatible output on Linux, and CoreAudio on macOS.
- Opt-in hardware H.264 decode (`CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE`)
  delivers each decoded frame into the same CPU-resident `crtmedia_frame`
  through FFmpeg hwaccels, with software decode remaining the default and the
  fallback. It now has real-hardware evidence on **all three hosts**:
  **macOS/arm64 (VideoToolbox)**, **Windows/x64 (D3D11VA, Intel UHD 630)**, and
  **Linux/x86_64 (VA-API, physical Intel UHD 630, native Ubuntu desktop,
  2026-09-22)** all report the frozen `crtmedia_hw_decode_test` `RESULT` line
  (`hw_frame_observed=yes cpu_transfer=pass frame_count=25 fallback=no
  eos=pass clean_exit=pass`), decoder flush/reuse, and a 15-cycle
  create/decode/EOS/release lifecycle; full `ctest` was 132/132 on the Linux
  run. `crtmedia_codec_is_hardware_accelerated()` is true only after a real
  hardware frame has been downloaded, never merely because a hardware device
  was created. Closing Linux needed four real, non-VA-API-specific
  build-environment/toolchain fixes (a GNU Binutils `ar`/`ranlib`/`nm`
  IFUNC-relink segfault, a `PKG_CONFIG_PATH` gap hiding the host's own
  `libva.pc`, a new `-fcrt-real-linux-sdk` `crt-cc` sentinel for `<va/va.h>`,
  and linking the real host `libva.so`/`libva-drm.so`); see
  `docs/acceptance/crtmedia_hardware_decode_acceptance.md` and [`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md)'s 2026-09-22
  entry. No platform-native decoded surface is part of the public ABI.
- Zero-copy decoded-texture interop is complete on all three hosts. macOS/
  VideoToolbox -> Metal and Linux/VA-API dma-buf -> Vulkan pass direct
  zero-copy. Windows/D3D11VA -> D3D12 passes the documented no-CPU-readback
  GPU-copy fallback. Common lifecycle stress, installed/package acceptance,
  pixel/resize checks, and exact per-host interop reporting are closed; see
  `docs/acceptance/crtmedia_zero_copy_decode_acceptance.md` and [`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md) 2026-09-27.
- Encode and capture is complete on Linux/x86_64, macOS/arm64, and
  Windows/x64. The common capture ABI feeds V4L2, AVFoundation, and Media
  Foundation backends; software MPEG-4 plus VA-API, VideoToolbox, and Media
  Foundation H.264 encoders pass mux/decode-back checks with the same
  microsecond timestamp and ownership contract. Deterministic discontinuity
  tests and 15-cycle capture/encode lifecycle gates are closed, including a
  real Linux UVC camera and all three hardware encoders. Each host also passed
  a fresh isolated `04-gfx-media` rebuild with installed consumers,
  dependency/RPATH audit, `verify_dist.py`, and atomic publication; see
  `docs/acceptance/crtmedia_encode_capture_acceptance.md` and [`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md) 2026-09-27..28.

- Networking and streaming is complete on Linux/x86_64, macOS/arm64, and
  Windows/x64. `libcrtmedia` owns a bounded, cancellable transport queue with
  no socket/TLS dependency, an HTTP input path (Range-capable and chunked
  non-seekable) under the extractor, and an HTTP upload sink that carries
  fragmented MP4 through a hard-bounded queue (`crtmedia_muxer_create_for_url`).
  Reconnect resumes only with `Range` plus `If-Range` and accepts a response
  only when it is a `206`, the `Content-Range` start matches, and the entity
  validator is unchanged; retries are bounded, a changed resource is a protocol
  error (never spliced), and output never auto-resumes. HTTPS is verify-on by
  default with a caller-supplied CA (`crtmedia/tls.h`) and a loopback
  correct-CA/wrong-CA/wrong-SAN matrix that also gates the upload path.
  Connect/stream/cancel/reconnect/destroy stress audits sockets, threads, and
  native handles, and each host passed a fresh isolated `04-gfx-media` rebuild
  with the curl/mbedTLS/zlib layer, an installed `examples/media-stream`
  consumer, `verify_dist.py`, and atomic publication. FFmpeg's own network
  stack stays disabled; libcurl sits under the CRT-owned contract. Loopback and
  IP-literal fixtures keep the minimal IPv4/UDP resolver out of the streaming
  path. See `docs/acceptance/crtmedia_networking_acceptance.md` and [`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md)
  2026-09-28..29.

This evidence does not yet prove production-complete seeking/track selection,
adaptive streaming (HLS/DASH), RTSP, or realtime/WebRTC behavior.

### libcrtui

The application UI layer (`05-ui`) is accepted on Linux/x86_64, macOS/arm64,
and Windows/x64 (Tranches 0-7, `docs/acceptance/crtui_acceptance.md`):

- `libcrtui` is a CRT-owned API: no LVGL type appears in a public header, and the
  shared library exports exactly the declared `CRTUI_API` functions (68 at the
  last Windows count, `lvgl_exports=0`), checked by `crtui_privacy_test`. LVGL
  9.6.0 is fetched at build time (pinned, SHA-256 verified), compiled privately,
  and used only as a software renderer into a caller-owned BGRA8888 buffer.
- The CRT model owns the widget tree, layout, events, focus and hit-testing.
  Layout is free, Row, Column, Stack, and scrolling (ScrollView/List) with
  margin, padding, gap, alignment and grow; styling is a CRT-neutral mask
  (colors, border, radius, opacity, font, text alignment). The v1 widgets are
  Window, Container, Text, Button, Slider, Progress, Switch, Checkbox, Image,
  List/ScrollView and TextInput. Contract, layout, surface, input and render
  tests pass headless and with LVGL on every host.
- Input arrives through the `crtui/crtgfx.h` adapter: keys, committed text
  (UTF-8), pointer, wheel, resize, DPI and focus loss (which cancels an in-flight
  press). Focus navigation is spatial. The evidence is scripted events through
  the same queue real events use; interactive device input is not automated.
- A producer-neutral external-surface scene (stable ids, clip, opacity, z-order,
  damage) is composed by `crtgfx`/Skia in the final present (`crtui_skia`
  companion, kept out of core `libcrtui`). `MediaView` binds a hardware-decoded
  `crtmedia` GPU frame to that scene (`crtui_skia_media`) with balanced frame
  ownership: zero-copy on Linux and macOS, measured GPU-copy on Windows, real
  H.264 clip, real windows.
- `05-ui` is packaged in-tree (`crt-ui-dist`) and built in isolation from the
  isolated `04-gfx-media` SDK (8/8 stage tests, installed `examples/ui-basic`
  rebuilt externally, packaged demo, `verify_dist.py`, atomic publish; Windows
  132 s, macOS 49 s, Linux 24 s, also from paths with spaces). LVGL is declared as
  a private static dependency with its MIT notice. The same work added relocation
  of text files (`.pc`, `.la`, `*-config`) that bake the isolated 04 staging
  path, now rejected by `verify_dist.py` and verified on all three hosts (on
  Windows the path is baked in the `/c/...` spelling).

### Removed: libcrtjs / 05-js

The `libcrtjs` skeleton (static/shared skeleton libraries only, no engine) and
the `05-js` stage that packaged it were deleted on 2026-09-29, together with
the `crt-js-*` targets, the `crtjs_` test-filter entries, and the `05-js`
stage in `tools/verify_dist.py`/`tools/crt_dist_prerequisites.py`. The QuickJS
plan is retired in favor of the `05-ui` and `06-web` stages (WebKit brings
JavaScriptCore). There is no standalone CRT JavaScript API or binding;
JavaScriptCore now exists only as an accepted `06-web` bring-up component.

### libcrtweb / 06-web

The WebKit CRT Port is in progress. Tranches 0-2 are closed; detailed evidence
and non-gating hardening follow-ups are in `docs/acceptance/crtweb_acceptance.md`:

- WPE WebKit 2.54.0 is pinned with archive hash, signed-tag/commit provenance,
  license inventory, patch manifest, and a source-security policy.
- JavaScriptCore is accepted on Linux/x86_64, Linux/aarch64, macOS/arm64, and
  Windows/x64 through the interpreter, Baseline/DFG/FTL tiers, WebAssembly,
  multi-thread/context lifecycle, watchdog/trap handling, Host ABI audits, and
  the sampling profiler. W^X tightening, broader SIMD, and Wasm threads remain
  documented non-gating follow-ups.
- The verified, pristine WPE 2.54.0 source builds with the native Linux toolchain
  on Ubuntu 26.04.1/x86_64 and renders the repository's local HTML/CSS/DOM/
  JavaScript/canvas fixture through WPEPlatform's built-in headless backend.
  The accepted run returned the exact DOM proof and a visible 640x480 BGRA8888
  snapshot. This is deliberately a native reference build, not CRT integration.
- No `PlatformCRT`, public `crtweb` API, `crtui` WebView, root CMake target, or
  `06-web` distribution exists yet. Tranche 3, the Linux graphics/input
  prototype followed by the product port source gate, is next.

### Upper Runtime Direction

- The runtime is packaged through the cumulative C, C++, Simple Graphics, and
  Graphics/Media stages and `05-ui`; `06-web` is the active, not-yet-packaged
  next stage.
- Each cumulative stage can also be bootstrapped and verified in isolation,
  purely from its own predecessor's already-packaged SDK rather than the
  in-repo build tree. The complete predecessor-only chain through
  `03-gfx-simple -> 04-gfx-media`, with Skia and FFmpeg genuinely enabled
  (not the in-repo cumulative pass's default-OFF state), is complete on
  Windows, macOS, and native Linux/aarch64. Linux's fresh cumulative run also
  passes both installed-source Vulkan examples, `verify_dist.py`, and atomic
  publication. The `04-gfx-media -> 05-ui` transition is accepted the same way
  on Windows, macOS, and Linux/x86_64 (the 05-ui stage built with the 04 SDK's own
  tool and embedded recipe).
- Distribution hardening is complete for the current 01-through-05 contract:
  predecessor relinking, external-prerequisite manifests, ELF/PE/Mach-O
  dependency inventory, ELF RUNPATH and Mach-O install-name/RPATH portability,
  installed external CMake consumers, configure/make port consumers, and
  space-containing paths are covered. The former Linux/aarch64 Skia blocker is
  resolved without an exception to those gates.
- Allocator baseline validation and a documented Host ABI firewall are the
  accepted foundation for FFmpeg hardware decode. The validation tranche is
  closed with the current allocator retained. Real upper-runtime workloads
  continue to compare against that baseline; Scudo remains conditional on a
  preserved, repeatable blocker.
- The `libcrtgfx` GPU backend-object boundary hardening and the live GPU
  presentation pixel-exact/resize evidence tranches are both closed
  (2026-09-17..18) on every required host. Hardware video decode built on
  that foundation and is closed on all three hosts ([`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md),
  2026-09-22): macOS/arm64 (VideoToolbox, 2026-09-18), Windows/x64 (D3D11VA,
  2026-09-19), and Linux/x86_64 (VA-API, physical Intel GPU, 2026-09-22) all
  report a real hardware frame observed and downloaded, the normalized
  cross-host acceptance matrix is recorded in
  `docs/acceptance/crtmedia_hardware_decode_acceptance.md`, and the package-acceptance
  re-audit (fresh isolated `04-gfx-media` rebuild, binary/import dependency
  audit) passed on all three hosts. A real, separate gap this closure found:
  the packaged `crtmedia_player_demo` never actually requested hardware
  decode, so it always decoded in software regardless of host; fixed the
  same day (`HISTORY.md`) -- it now requests hardware by default, with a
  documented software-only escape hatch, verified on Windows so far.
  The first public developer preview of the completed `04-gfx-media` stage
  is published: `v0.4.0-preview.1` on GitHub
  (`https://github.com/webos21/crt/releases/tag/v0.4.0-preview.1`) with
  Windows/x64, macOS/arm64, and Linux/x86_64 asset sets, checksums, and
  manifests. Its packaged `crtmedia_player_demo` predates the hardware-decode
  default above and still only decodes in software on every host; the
  release notes document that. A dedicated clean-machine run was explicitly
  removed from the project queue in favor of acting on real downloader
  reports ([`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md), 2026-09-23). Replaying the later hardware-decode-
  default demo on macOS/Linux belongs to a future release build, not the
  completed preview or the active runtime tranche.
- Zero-copy decoded-texture interop, Encode and capture, and Networking and
  streaming are closed on all three hosts.
- The roadmap is `04-gfx-media -> 05-ui -> 06-web`. `05-ui` (`crtui`, LVGL as a
  private implementation, with external surfaces so video -- and later a WebView
  -- is composed by `crtgfx` rather than copied through an LVGL framebuffer) is
  accepted; `06-web` is in progress as a WebKit CRT Port (`PlatformCRT`) with
  `libcrtweb` and a WebView, using WPE WebKit as the reference. Tranches 0-2
  are closed and Tranche 3 is next. WebRTC,
  QuickJS, WebGPU, EME/DRM, V8, and Chromium/Ozone are deferred, not gates.

The sequencing and ownership boundaries are recorded in
[`docs/design/runtime_roadmap.md`](docs/design/runtime_roadmap.md); the isolated-acceptance
trail is in `HISTORY.md`, open work in `TODO.md`.

## Verification Model

### Default Workflow

The normal host check is:

```text
cmake --workflow --preset <host-preset>
```

It configures, builds, and runs the C-stage CTest suite. Upper layers use their
explicit `*-build`, `*-test`, and `*-dist` targets. The CI matrix also
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
| Broader deterministic Skia CPU drawing | `crtgfx_skia_cpu_coverage` | compare with the per-host GPU smoke |
| Keyboard and pointer event translation | synthetic common-queue coverage plus host adapter tests | `crtgfx_keyboard_interactive` |
| Skia-backed interactive typed text | one-command `crtgfx-keyboard-interactive-skia` build | run the resulting interactive binary |
| Wayland source/toolchain integration | `crtgfx-wayland-smoke` | Linux host adapter needs a reachable compositor for the live path |

### UI Checks

| Evidence | Automated |
| --- | --- |
| Contract: errors, lifetime, threads, events, focus, geometry | `crtui_contract_test` |
| Layout, style and v1 widget behavior (headless) | `crtui_layout_test` |
| External-surface scene: clip, opacity, z-order, damage, hit-test | `crtui_surface_test` |
| Input and focus through the `crtgfx` adapter | `crtui_input_test` |
| LVGL pixels, state changes, resize, lifecycle | `crtui_render_test` |
| LVGL stays private: headers, exports, sample | `crtui_privacy_test` |
| Real GPU final composition and MediaView with a real H.264 clip | `crtui_surface_compositor_test`, `crtui_media_view_test` |
| Isolated stage and installed sample | `crt-stage-build.py` (04 -> 05) with the packaged demo, `verify_dist.py --stage 05-ui` |

`crt-ui-test` runs the `crtui_*` set; the compositor and MediaView tests exist
only in a Skia/FFmpeg-enabled tree.

### Media Checks

| Evidence | Automated | Remaining live evidence |
| --- | --- | --- |
| CPU plane geometry, ownership, and color conversion | `crtmedia_frame_test` | none for the covered formats |
| CPU/GPU frame handoff into Skia | `crtmedia_frame_skia_smoke`, `crtmedia_zero_copy_test`, `crtgfx_skia_media_lifecycle_test` | accepted cross-host; Windows is the documented GPU-copy fallback |
| Extractor/codec separation and software decode queues | `crtmedia_extractor_codec_test` plus `crtmedia_demux_test` | broader fixture/seek/track-selection coverage |
| Player clock/state and CPU-frame planning | `crtmedia_player_test` | longer mixed audio/video sessions and underrun/recovery coverage |
| Host audio output contract | `crtmedia_audio_sink_test` | real-device behavior remains host/environment dependent |
| Encode/mux timing and ownership | `crtmedia_encode_mux_test`, `crtmedia_timing_discontinuity_test`, `crtmedia_capture_encode_lifecycle_test` | accepted with real capture/hardware encode on all three hosts |
| Network streaming | `crtmedia_transport_queue_test`, `crtmedia_http_input_range_test`, `crtmedia_http_input_chunked_test`, `crtmedia_http_output_test`, `crtmedia_http_reconnect_test`, `crtmedia_https_test`, `crtmedia_http_lifecycle_test` | accepted on all three hosts against loopback/IP-literal fixtures; no real-name resolution or public-server evidence |

Headless Linux is allowed to report `CRTGFX_ERROR_UNSUPPORTED` for native
window creation. That verifies graceful fallback, not live presentation; a
real compositor run is required before claiming the visual/input path passed.

### Porting Checks

A port is complete only when its recipe records static and shared attempts on
each host, plus a link/run or round-trip test where meaningful. Recipes,
statuses, and exceptions are maintained in:

- [`porting/recipes/`](porting/recipes)
- [`docs/porting/porting_status.md`](docs/porting/porting_status.md)
- [`docs/porting/sysroot_ports.md`](docs/porting/sysroot_ports.md)

## Known Limitations

### CRT/PAL

- The DNS resolver is intentionally small: synchronous UDP A-record lookup,
  without complete IPv6, TCP fallback, search-domain, or caching behavior.
- Toybox `timeout` remains disabled until cross-process signal delivery and
  meaningful `SIGCHLD` `siginfo_t` data are complete.
- Interactive POSIX job control remains deferred. The project mksh build does
  not claim full foreground/background stop/resume semantics; see
  [`docs/design/job_control.md`](docs/design/job_control.md).
- Some console environments cannot provide a real screen buffer for
  `TIOCGWINSZ`; tty behavior and remaining applet restrictions are tracked in
  [`docs/porting/toybox_applet_status.md`](docs/porting/toybox_applet_status.md).
- Windows static archives containing constructor sections can still require a
  recipe-specific retention policy because PE/COFF archive extraction does
  not behave like a GNU ELF linker script.
- `getrusage()` currently preserves the public API shape but returns
  zero-filled usage counters. Allocator baseline tooling therefore measures
  RSS with a host-side runner rather than treating `ru_maxrss` as real data.
- Linux `libdl` remains a documented boundary rather than a CRT-owned general
  ELF loader: its `dlopen`/`dlsym`/`dlclose` honestly report "not supported"
  rather than delegating to a real loader, since this project links every
  Linux binary against the real system dynamic linker directly and defers a
  CRT-owned ELF loader to a later phase. Those unimplemented exports now
  carry the same `CRT_1.0` ELF symbol-version namespace as `libc.so`'s, so a
  real host consumer's own versioned `dlopen` reference (e.g. the Vulkan
  loader's internal ICD loading) can no longer resolve to this library's
  stub by mistake; the prebuilt shared-runtime `crtgfx_skia_gpu_window_demo`
  now loads a real host Vulkan ICD directly. A full Android-style linker
  remains a separate long-term tranche.

### libcrtgfx

- The Linux adapter uses project-owned wire handling and does not yet recycle
  Wayland object ids. It has been exercised on the available compositor, not
  across every compositor implementation.
- Image codecs, shaping, fallback fonts, ICU, and platform font discovery are
  not yet completion claims.
- GPU decode-texture import is accepted cross-host, including Linux dma-buf
  import and the documented Windows GPU-copy fallback.
  Linux packaged Skia live presentation passes on the Linux/aarch64
  acceptance host, which is a VM whose Vulkan device is virtio-gpu/lavapipe
  (a paravirtualized or software device, not a physical GPU); a physical-GPU
  Linux/Vulkan run is now also recorded separately (Intel UHD 630, native
  x86_64 desktop, 2026-09-22), but has not replaced the VM as the recorded
  cross-host stage-build baseline. The former `mangledName()`/Mesa/LLVM
  attribution was a downstream symptom of mixing static and shared CRT
  allocator instances in the standalone example's link; no Skia or Mesa source
  patch is carried.
- WSLg can negotiate the Wayland protocol while still differing from a normal
  Linux compositor in visible presentation behavior. It is useful evidence,
  but is not a substitute for a real Linux desktop run.

### libcrtmedia

- The software extractor/codec/player and all three host audio sinks exist,
  but seeking/track-selection breadth, long-running queue/backpressure
  behavior, and richer compressed-media fixtures still need expansion.
- Hardware H.264 decode reaches a CPU-resident frame on all three hosts now:
  macOS (VideoToolbox), Windows (D3D11VA), and Linux (VA-API, physical Intel
  GPU, 2026-09-22). Two earlier Linux hosts hit environment limits, not a CRT
  defect, kept here for the record: the Linux/aarch64 acceptance VM's Mesa
  `virtio_gpu` driver advertises no H.264 decode entrypoint, and on WSL2 (Mesa
  D3D12 VA-API on Intel UHD 630, Windows driver 31.0.101.2140) a real decode
  hangs in a self-deadlock inside Intel's WSL video driver, not in CRT, FFmpeg
  or Mesa. Decoder-surface ownership, affinity, synchronization, and Skia
  import are now covered by the cross-host zero-copy acceptance matrix.
- Software video encode, MP4 mux/decode-back, real host capture, and hardware
  H.264 encode are accepted on Linux/x86_64, macOS/arm64, and Windows/x64.
  The HTTP/HTTPS transport, streaming input/output, and reconnect are accepted
  on all three hosts (loopback fixtures). Adaptive streaming (HLS/DASH), RTSP,
  and realtime/WebRTC remain open. FFmpeg is still intentionally file-only
  (`--disable-network`); network I/O goes through the CRT-owned transport.

### libcrtweb

- `libcrtjs` is gone and QuickJS is no longer planned as a stage. The accepted
  JavaScriptCore and native WPE builds are `06-web` bring-up/reference evidence,
  not a standalone JavaScript product API or packaged Web runtime. W^X
  tightening, broader SIMD, Wasm threads, and native WPE dependency breadth are
  recorded limitations rather than claims that `PlatformCRT` already exists.

### libcrtui

- Text entry takes committed UTF-8 `TEXT` events only: there is no IME
  composition, text selection, clipboard, or pointer caret placement. Scrolling
  is by wheel and focus; pointer-drag scrolling is not implemented. There is no
  touch input (no `crtgfx` source for it) and no hover state.
- Image and video are drawn with Skia/LVGL scaling that is not exact at source
  edges; the tests use sources whose pixels survive it.
- Input evidence is scripted through the real event queue; interactive keyboard,
  mouse and trackpad behavior on real devices is not automated on any host.
- The `05-ui` SDK is not yet a release asset: `tools/prepare_release_assets.py`
  has no `05-ui` entry. The in-tree package ships `crtui/skia*.h` even when
  Skia/FFmpeg are off but builds the companion libraries only when they are on.
- `crt-ui-dist` needs `-DCRTUI_ENABLE_LVGL=ON` (after `crtui-lvgl-fetch`); with
  the default configuration it fails at `verify_dist.py` because the required
  `ui-basic` sample is not built.

## Next Priorities

1. Continue `TODO.md`'s active Web Runtime (`06-web`) work, a WebKit CRT Port.
   Application UI (`05-ui`, Tranches 0-7) is closed on all three hosts, so its
   External Surface contract is the accepted prerequisite. The order is
   `docs/acceptance/crtweb_acceptance.md`: Tranches 0-2 (provenance, cross-host
   JavaScriptCore, and the native Linux WPE reference baseline) are closed;
   next is the Tranche 3 `PlatformCRT` graphics/input prototype. See
   `docs/porting/crtweb_porting.md`.
2. Small follow-ups left by the UI work, not gates: teach
   `tools/prepare_release_assets.py` the `05-ui` SDK, and make a default-
   configuration `crt-ui-dist` fail with a clear message instead of a missing
   `ui-basic/main.c`.

Also ongoing, opportunistically rather than sequenced: closing the focused
CRT/PAL limitations above when an upstream consumer exposes a concrete
requirement, following the Bionic-first porting discipline in `AGENTS.md`
(`TODO.md`'s "Focused CRT/PAL follow-ups"), including non-blocking
Windows/aarch64 allocator validation and comparison of real upper-runtime
workloads against `docs/acceptance/allocator_baseline.md`.

Detailed actionable work belongs in [`TODO.md`](TODO.md); completed changes
belong in [`HISTORY.md`](HISTORY.md).
