# Project Status

This file is the current, evidence-based project snapshot. It intentionally
does not repeat the implementation diary in [`HISTORY.md`](HISTORY.md), the
open work queue in [`TODO.md`](TODO.md), or the per-port matrix in
[`docs/porting_status.md`](docs/porting_status.md).

Last synchronized with the source tree and git history: **2026-09-18**.
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
  JavaScript under `out/<preset>/dist/`.
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
  [`docs/porting_status.md`](docs/porting_status.md).
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
  covers it in the isolated 04-stage acceptance run.
- The `crtgfx_gpu_device`/`crtgfx_gpu_surface` backend-object boundary is
  hardened and closed (2026-09-17..18, `HISTORY.md`): both are now a fixed
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
  (`docs/libcrtgfx_live_presentation_acceptance.md`, 2026-09-18): a real
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

Decoder-texture zero-copy, full font shaping/fallback/ICU, a full Wayland
compositor, and a Chromium Ozone backend are not completion claims.

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
- The public format/extractor/codec split is implemented. The extractor owns
  demux-only track/sample access, while asynchronous codec queues own software
  decode and explicit output-buffer release.
- `crtmedia_player` supplies the software playback state machine, clocks,
  seek/reset behavior, bounded render planning, and CPU-frame delivery.
- Host audio sinks are implemented and exercised through WASAPI on Windows,
  raw ALSA or PulseAudio-compatible output on Linux, and CoreAudio on macOS.

This evidence does not yet prove production-complete seeking/track selection,
network streaming, encoding/capture, hardware decode enabled in each host
FFmpeg build, decoder-texture zero-copy, or GPU-frame handoff.

### libcrtjs

`libcrtjs` currently builds and installs static/shared skeleton libraries. No
QuickJS engine or JavaScript-visible graphics/media service is integrated yet.
The intended first engine remains QuickJS; V8 remains the browser-class later
target.

### Upper Runtime Direction

- The runtime is packaged through the cumulative C, C++, Simple Graphics,
  Graphics/Media, and JavaScript stages.
- Each cumulative stage can also be bootstrapped and verified in isolation,
  purely from its own predecessor's already-packaged SDK rather than the
  in-repo build tree. The complete predecessor-only chain through
  `03-gfx-simple -> 04-gfx-media`, with Skia and FFmpeg genuinely enabled
  (not the in-repo cumulative pass's default-OFF state), is complete on
  Windows, macOS, and native Linux/aarch64. Linux's fresh cumulative run also
  passes both installed-source Vulkan examples, `verify_dist.py`, and atomic
  publication.
- Distribution hardening is complete for the current 01-through-04 contract:
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
  (2026-09-17..18) on every required host. Hardware video decode is the
  active tranche now building on that closed foundation (`TODO.md`'s In
  Progress); it is promoted but not yet scoped in implementation detail.
- FFmpeg hardware decode/zero-copy and the QuickJS core then proceed on top of
  the completed GPU rendering/presentation contract.
- JavaScript media/gfx binding follows the stable native contracts, using a
  WebCodecs-like asynchronous shape; WebRTC-style realtime services, V8, and a
  Chromium/Ozone probe remain later layers.

The sequencing and ownership boundaries are recorded in
[`docs/runtime_roadmap.md`](docs/runtime_roadmap.md); the isolated-acceptance
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

### Media Checks

| Evidence | Automated | Remaining live evidence |
| --- | --- | --- |
| CPU plane geometry, ownership, and color conversion | `crtmedia_frame_test` | none for the covered formats |
| CPU frame handoff into Skia | `crtmedia_frame_skia_smoke` | GPU texture handoff is not implemented |
| Extractor/codec separation and software decode queues | `crtmedia_extractor_codec_test` plus `crtmedia_demux_test` | broader fixture/seek/track-selection coverage |
| Player clock/state and CPU-frame planning | `crtmedia_player_test` | longer mixed audio/video sessions and underrun/recovery coverage |
| Host audio output contract | `crtmedia_audio_sink_test` | real-device behavior remains host/environment dependent |

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
- GPU decode-texture import and dmabuf-style zero-copy remain future work.
  Linux packaged Skia live presentation passes on the physical aarch64
  lavapipe host. The former `mangledName()`/Mesa/LLVM attribution was a
  downstream symptom of mixing static and shared CRT allocator instances in
  the standalone example's link; no Skia or Mesa source patch is carried.
- WSLg can negotiate the Wayland protocol while still differing from a normal
  Linux compositor in visible presentation behavior. It is useful evidence,
  but is not a substitute for a real Linux desktop run.

### libcrtmedia And libcrtjs

- The software extractor/codec/player and all three host audio sinks exist,
  but seeking/track-selection breadth, long-running queue/backpressure
  behavior, and richer compressed-media fixtures still need expansion.
- Hardware-decode negotiation and CPU fallback are defined at the API level,
  but host FFmpeg hwaccels are not yet enabled successfully. Decoder surfaces,
  device affinity, fences, CPU-download fallback, and zero-copy Skia import
  are not complete.
- No network protocol layer, mux/encode, capture, adaptive streaming, or
  realtime/WebRTC service exists.
- QuickJS has not been imported. Event-loop/timer/module/native-binding work
  and JavaScript media/gfx APIs remain open.

## Next Priorities

1. Enable and verify real FFmpeg hardware decode per host while retaining the
   software/CPU fallback as the correctness baseline. Active in `TODO.md`'s
   In Progress, promoted 2026-09-18 once live GPU presentation evidence
   closed on every required host; not yet scoped in implementation detail.
2. Connect hardware decoder textures to Skia without CPU copies, including
   device/fence ownership and CPU-download recovery.
3. Add capture/encode, then transport, buffering, reconnect, and streaming
   services after the native playback and zero-copy contracts are stable.
4. Use WebRTC as a consumer milestone, then bring up QuickJS core/event-loop/
   timers/modules and expose stable media/gfx services with WebCodecs-like
   queue semantics.
5. Continue closing the focused CRT/PAL limitations above when an upstream
   consumer exposes a concrete requirement, following the Bionic-first
   porting discipline in `AGENTS.md`. This includes non-blocking
   Windows/aarch64 allocator validation and comparison of real upper-runtime
   workloads against `docs/allocator_baseline.md`.

Detailed actionable work belongs in [`TODO.md`](TODO.md); completed changes
belong in [`HISTORY.md`](HISTORY.md).
