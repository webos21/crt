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
[`distribution.md`](../guides/distribution.md):

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
   [`crtui_acceptance.md`](../acceptance/crtui_acceptance.md).
6. **Web Runtime (`06-web`)** — JavaScriptCore, WebCore, the WebKit
   multi-process runtime, a `PlatformCRT` port, and `crtweb`/WebView
   integration. In progress (Tranches 0-2 closed: cross-host JavaScriptCore and
   the native Linux WPE reference; Tranche 3 `PlatformCRT` prototype next); see [`crtweb_acceptance.md`](../acceptance/crtweb_acceptance.md)
   and [`crtweb_porting.md`](../porting/crtweb_porting.md).

The earlier plan named a QuickJS stage (`05-js`). It is superseded: WebKit
brings JavaScriptCore in anyway, so a separate QuickJS stage would be
duplicate investment. The `05-js`/`libcrtjs` skeleton and its build targets
were deleted on 2026-09-29; `05-ui` was created fresh by UI Tranche 0 and is
now accepted (see [September history](../history/HISTORY-2026-09.md) and
[UI acceptance](../acceptance/crtui_acceptance.md)).

The installed output of one stage is the input boundary for the next. A stage
is not complete merely because an in-tree target links.

## Evidence And Execution

Detailed completion evidence is owned by the corresponding acceptance document:

| Contract | Evidence owner |
| --- | --- |
| Allocator replacement envelope | [Allocator baseline](../acceptance/allocator_baseline.md) |
| Native GPU pixels, resize, and presentation | [Live presentation](../acceptance/libcrtgfx_live_presentation_acceptance.md) |
| Observed hardware decode and CPU fallback | [Hardware decode](../acceptance/crtmedia_hardware_decode_acceptance.md) |
| GPU-frame ownership, synchronization, and copy classification | [Decoded-texture interop](../acceptance/crtmedia_zero_copy_decode_acceptance.md) |
| Capture, encode, timing, and mux/decode-back | [Encode and capture](../acceptance/crtmedia_encode_capture_acceptance.md) |
| Bounded HTTP/HTTPS, cancellation, reconnect, and lifecycle | [Networking and streaming](../acceptance/crtmedia_networking_acceptance.md) |
| Widgets, input, external surfaces, and isolated `05-ui` | [UI acceptance](../acceptance/crtui_acceptance.md) |
| JavaScriptCore, WPE reference, `PlatformCRT`, and WebView | [Web acceptance](../acceptance/crtweb_acceptance.md) and [upstream mapping](../porting/crtweb_porting.md) |

The accepted graphics/media sequence was live presentation, hardware decode,
decoded-texture interop, capture/encode, and networking, followed by application
UI. Dates, per-host corrections, and measured results stay in the
[monthly history](../history/README.md) and [current-month log](../../HISTORY.md).
Do not copy that completed checklist into the roadmap again.

Continue `06-web` in the tranche order defined by Web acceptance. Preserve the
accepted allocator envelope and [Host ABI firewall](host_abi_firewall.md);
replace the allocator only when repeatable workload evidence exceeds the
recorded envelope. Every upper-layer failure feeds a Bionic-compatible CRT/PAL
fix and a replay of the consumer that exposed it. Optional host features retain
a portable fallback where the contract specifies one.

[TODO](../../TODO.md) owns the current and next work item;
[STATUS](../../STATUS.md) owns the point-in-time support matrix. This roadmap
owns dependency order, stage boundaries, and completion rules.

## Completion Rules

- A capability is complete only when its public ownership, lifetime, error,
  and synchronization behavior is defined and exercised on the applicable
  hosts.
- Compile or link success alone is not runtime acceptance. GPU and media paths
  must distinguish real presentation or hardware use from a supported
  software/headless fallback.
- A cumulative in-tree build does not replace predecessor-only stage
  acceptance. Release stages follow [`distribution.md`](../guides/distribution.md).
- Allocator replacement requires a preserved reproducer and cross-host
  measurements; asymptotic concern alone is not an acceptance failure.
- Host-library opaque objects must be created, synchronized, and destroyed by
  their owning host library/allocator domain. CRT adapters may transport
  handles but must not reinterpret private layouts or free host-owned storage.
- Upstream source is not patched merely to hide a CRT/PAL deficiency. Porting
  follows the Bionic-first discipline in [`../AGENTS.md`](../../AGENTS.md).

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
