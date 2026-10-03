# Runtime Roadmap

## Product Direction

CRT is a Bionic-compatible runtime/PAL for Linux-kernel embedded products and
native Linux, Windows, and macOS development. The product is the rebuild-based
portability layer and its staged SDK, not an Electron clone, an Android
framework, or a compiler distribution.

Typical consumers include set-top boxes, industrial HMIs, IVI systems, and
small game consoles. Desktop builds let application developers run and debug
the same source with each host's native executable format and UX.

## Stage Gates

Development and release follow the cumulative stages defined in
[`distribution.md`](distribution.md):

1. **C Runtime (`01-c`)** — libc/libm/libdl, startup, shell, make/mksh/toybox.
2. **C++ Runtime (`02-cxx`)** — imported libc++/libc++abi/libunwind.
3. **Simple Graphics (`03-gfx-simple`)** — one window, keyboard/mouse, and a
   CPU-writable framebuffer. Skia raster/text is explicitly outside this gate.
4. **Graphics/Media (`04-gfx-media`)** — Skia CPU and GPU, native
   Vulkan/D3D12/Metal presentation, FFmpeg, audio, playback, and opt-in
   hardware H.264 decode plus decoded-texture interop (verified on all three
   hosts; Windows uses the documented GPU-copy fallback).
5. **Application UI (`05-ui`)** — `crtui`, LVGL, application widgets, and
   external-surface composition (video today, WebView later) over the stable
   lower graphics/media contracts. Accepted on all three hosts; see
   [`crtui_acceptance.md`](crtui_acceptance.md).
6. **Web Runtime (`06-web`)** — JavaScriptCore, WebCore, the WebKit
   multi-process runtime, a `PlatformCRT` port, and `crtweb`/WebView
   integration. In progress (Tranche 0 closed); see [`crtweb_acceptance.md`](crtweb_acceptance.md)
   and [`crtweb_porting.md`](crtweb_porting.md).

The earlier plan named a QuickJS stage (`05-js`). It is superseded: WebKit
brings JavaScriptCore in anyway, so a separate QuickJS stage would be
duplicate investment. The `05-js`/`libcrtjs` skeleton and its build targets
were deleted on 2026-09-29; `05-ui` was created fresh by UI Tranche 0 and is
now accepted (see [`../HISTORY.md`](../HISTORY.md)).

The installed output of one stage is the input boundary for the next. A stage
is not complete merely because an in-tree target links.

## Current Baseline

- CRT/PAL, shell/rootfs, and the configure/make porting loop are operational on
  Linux, Windows, and macOS.
- The imported Android libc++/libc++abi/libunwind lane has static/shared
  smoke coverage on all three hosts.
- Simple window, framebuffer, keyboard, mouse, and resize behavior exists.
- The common GPU device/surface/frame/fence contract and live native
  presentation are implemented with Vulkan/Wayland, D3D12/Win32, and
  Metal/Cocoa.
- Skia CPU raster/text and Ganesh GPU paths build and run on Linux, Windows,
  and macOS. Low-level live GPU presentation exists on all three hosts. The
  packaged Skia/Ganesh path is accepted on Windows, macOS, and native
  Linux/aarch64. Linux passes repeated cold-cache presentation with Vulkan
  validation both disabled and enabled, and directly runs the packaged
  shared-runtime Vulkan/Skia binary as an isolated-stage smoke. Machine-
  checkable pixel-exact and resize-plus-Ganesh evidence is now closed on
  every required host (Linux/aarch64 Vulkan, macOS/arm64 Metal, Windows/x64
  D3D12) against one shared, backend-neutral acceptance contract
  (`../docs/libcrtgfx_live_presentation_acceptance.md`); this was the last
  gap before hardware decode, not a missing common API.
- The software media baseline includes FFmpeg-backed demux/decode, the common
  frame/audio/player contracts, native audio sinks, software MPEG-4 video
  encode, and MP4 mux/decode-back. Hardware H.264 decode
  into a CPU-resident frame is now verified on all three hosts: macOS/arm64
  (VideoToolbox), Windows/x64 (D3D11VA), and Linux/x86_64 (VA-API, physical
  Intel GPU, 2026-09-22). Decoded-texture interop is also accepted on all
  three hosts: direct zero-copy on macOS/Linux and a measured D3D11-to-D3D12
  GPU-copy fallback on Windows. Encode & Capture Tranches 0-6 are also closed
  on Linux/x86_64, macOS/arm64, and Windows/x64: real host capture, software
  and hardware encode, mux/decode-back, timestamp-discontinuity and lifecycle
  gates, plus isolated `04-gfx-media` package acceptance are recorded in
  `crtmedia_encode_capture_acceptance.md` and `HISTORY.md` (2026-09-27..28).
- Networking and streaming (Tranches 0-6) is closed on Linux/x86_64,
  macOS/arm64, and Windows/x64: a bounded, cancellable transport core;
  progressive HTTP input (Range and chunked) into the extractor; fragmented
  MP4 output over an HTTP upload sink with a hard memory bound; bounded
  `Range`+`If-Range` reconnect that never splices a changed resource and never
  auto-resumes output; HTTPS with an explicit trust policy and a loopback
  correct-CA/wrong-CA/wrong-SAN matrix; and lifecycle/isolated-package
  acceptance with an installed streaming consumer. See
  `crtmedia_networking_acceptance.md` and `HISTORY.md` (2026-09-28..29).
  FFmpeg's own network stack stays disabled; libcurl sits under a CRT-owned
  transport contract.
- The cumulative binary-package chain currently ends at `05-ui` (`06-web` is
  next). Predecessor-only isolated-stage acceptance through the option-ON
  `03-gfx-simple -> 04-gfx-media` transition is complete on Windows, macOS,
  and native Linux/aarch64, and `04-gfx-media -> 05-ui` is accepted on Windows,
  macOS and Linux/x86_64, including final distribution verification and
  atomic publication.
- `libcrtjs` was a skeleton only and was removed (2026-09-29); no QuickJS
  engine, event loop, or bindings exist or are planned as a stage.
- The bootstrap/reference allocator has API, debug-mode, contention,
  fragmented-fork, and expected-fault coverage. Windows/x86_64, macOS/arm64,
  and Linux/aarch64 have current-schema baseline data; Linux did not exceed
  the replacement envelope. The validation tranche is accepted and closed.
  Real upper-runtime workloads remain regression comparisons; Scudo stays
  conditional and is not a prerequisite unless measurements demonstrate a
  blocker.

Exact test counts, host evidence, and current limitations belong in
[`../STATUS.md`](../STATUS.md) and [`../HISTORY.md`](../HISTORY.md). Open work
belongs in [`../TODO.md`](../TODO.md); this document records dependency order
and completion boundaries rather than duplicating those ledgers.

## Execution Order

Preserve `allocator_baseline.md`'s accepted baseline and Host ABI firewall
ownership rules throughout the sequence below. Real workloads are regression
comparisons; promote Scudo only if repeatable evidence exceeds the baseline.

1. ~~Finish the remaining live GPU presentation evidence for the existing
   Ganesh backends.~~ **Complete 2026-09-18** (`HISTORY.md`): machine-
   checkable pixel-exact/resize evidence closed on Linux/aarch64,
   macOS/arm64, and Windows/x64. Graphite was not part of this acceptance
   gate and remains out of scope.
2. ~~Enable hardware video decode per host while retaining software decode as
   the correctness fallback and reporting actual hardware use separately.~~
   **Complete 2026-09-22** (`HISTORY.md`): macOS/arm64 (VideoToolbox),
   Windows/x64 (D3D11VA), and Linux/x86_64 (VA-API, physical Intel GPU) all
   report a real hardware frame observed and downloaded.
3. ~~Define and verify zero-copy decoded-texture ownership, device affinity,
   and synchronization, with a measured fallback where direct interop is
   unavailable.~~ **Complete 2026-09-27** (`HISTORY.md`): macOS/arm64 and
   Linux/x86_64 pass direct zero-copy; Windows/x64 passes its documented
   D3D11-to-D3D12 GPU-copy fallback; lifecycle and packaged-stage gates pass.
4. ~~Add capture, conversion, hardware/software encode, timestamp, and muxing
   on top of the accepted frame and playback contracts.~~ **Complete
   2026-09-28** (`HISTORY.md`): Linux/x86_64, macOS/arm64, and Windows/x64
   pass real capture, software/hardware encode, deterministic timing and
   lifecycle tests, and isolated-package acceptance. See
   `crtmedia_encode_capture_acceptance.md`.
5. ~~Add transport, bounded buffering, explicit back-pressure, cancellation,
   reconnect, and streaming protocol integration.~~ **Complete 2026-09-29**
   (`HISTORY.md`): Linux/x86_64, macOS/arm64, and Windows/x64 pass the bounded
   transport core, progressive HTTP input and fragmented-MP4 HTTP output,
   reconnect/discontinuity, HTTPS trust matrix, and isolated-package
   acceptance. See `crtmedia_networking_acceptance.md`.
6. ~~Application UI (`05-ui`): freeze the `crtui` contract, import LVGL behind
   it, wire CRT input/focus/resize, add the external-surface view and a media
   view over the zero-copy path, then close cross-host and isolated-package
   acceptance.~~ **Complete 2026-10-03** (`HISTORY.md`): Windows/x64,
   macOS/arm64 and Linux/x86_64 pass Tranches 0-7, including the isolated
   `04-gfx-media -> 05-ui` stage. See `crtui_acceptance.md`.
7. **Web Runtime (`06-web`) — In Progress (Tranche 0 closed).** JavaScriptCore/JSCOnly bring-up, a Linux WPE
   reference baseline, `PlatformCRT` graphics/input, `libcrtweb` and the
   WebView, multi-process lifecycle, then Windows and macOS replay, CRT
   subsystem substitution, GPU integration, and distribution/security
   closure. Linux first, with an early three-host JavaScriptCore replay; the
   WebView plugs into the `05-ui` external-surface contract. See
   `crtweb_acceptance.md`.

Each step may expose a lower CRT/PAL gap. Fix that gap at the
Bionic-compatible public surface or the controlled PAL boundary, rerun the
consumer that exposed it, and preserve a portable fallback where an optional
host facility is unavailable.

## Completion Rules

- A capability is complete only when its public ownership, lifetime, error,
  and synchronization behavior is defined and exercised on the applicable
  hosts.
- Compile or link success alone is not runtime acceptance. GPU and media paths
  must distinguish real presentation or hardware use from a supported
  software/headless fallback.
- A cumulative in-tree build does not replace predecessor-only stage
  acceptance. Release stages follow [`distribution.md`](distribution.md).
- Allocator replacement requires a preserved reproducer and cross-host
  measurements; asymptotic concern alone is not an acceptance failure.
- Host-library opaque objects must be created, synchronized, and destroyed by
  their owning host library/allocator domain. CRT adapters may transport
  handles but must not reinterpret private layouts or free host-owned storage.
- Upstream source is not patched merely to hide a CRT/PAL deficiency. Porting
  follows the Bionic-first discipline in [`../AGENTS.md`](../AGENTS.md).

## Deferred Scope

Not gates for the `05-ui`/`06-web` sequence; each is a later consumer or a
benchmark once the lower contracts have stable evidence:

- **WebRTC** — a consumer-driven integration after the browser baseline, wired
  through WebCore's backend only when a real consumer requires it. It is no
  longer a prerequisite of anything.
- **QuickJS** — deferred unless a non-WebKit lightweight JavaScript runtime
  becomes an actual product requirement; not a stage.
- **WebGPU, EME/DRM, WebXR, camera/microphone in the web runtime, and JS-native
  application bindings.**
- Graphite, V8, Chromium/Ozone, Gecko, a full compositor/desktop environment,
  Android framework or APK compatibility, and unmodified glibc binary
  compatibility.

## Host Order

| Work | First host | Reason |
| --- | --- | --- |
| `05-ui` (done) | Windows/x64 | fastest loop for interactive input, focus, and resize checks |
| `05-ui` replay (done) | macOS/arm64, then Linux | event semantics first, embedded/product closure last |
| `06-web` JavaScriptCore | Linux, replayed on all three hosts early | exposes threads/TLS/executable-memory/signal gaps in the lower runtime |
| `06-web` WPE reference and `PlatformCRT` | Linux | WPE is the known-good reference to compare against |
| `06-web` replay | Windows/x64, then macOS/arm64 | proves `PlatformCRT` is OS-neutral rather than a Linux port |

The document set that follows this roadmap: this file records dependency
order and stage boundaries only; [`../TODO.md`](../TODO.md) records the
current and next tranche; [`../HISTORY.md`](../HISTORY.md) records completed
work and host evidence; [`../STATUS.md`](../STATUS.md) is the point-in-time
support matrix; `crtui_acceptance.md`, `crtweb_acceptance.md`, and
`crtweb_porting.md` hold each stage's detailed contract and evidence.
