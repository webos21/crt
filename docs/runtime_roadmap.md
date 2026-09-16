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
   Vulkan/D3D12/Metal presentation, FFmpeg, audio, playback, and later hardware
   decode/zero-copy.
5. **JavaScript (`05-js`)** — QuickJS and bindings over the stable lower
   graphics/media contracts.

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
  validation both disabled and enabled. The remaining pixel-exact and resize
  combinations are separate evidence gaps, not a missing common API.
- The software media baseline includes FFmpeg-backed demux/decode, the common
  frame/audio/player contracts, and native audio sinks. Hardware decode and
  decoded-texture interop remain separate, explicitly reported capabilities.
- The cumulative binary-package chain reaches the current `05-js` skeleton.
  Predecessor-only isolated-stage acceptance through the option-ON
  `03-gfx-simple -> 04-gfx-media` transition is complete on Windows, macOS,
  and native Linux/aarch64, including final distribution verification and
  atomic publication.
- `libcrtjs` still contains skeleton libraries only. QuickJS, its event loop,
  modules, and graphics/media bindings have not been implemented.

Exact test counts, host evidence, and current limitations belong in
[`../STATUS.md`](../STATUS.md) and [`../HISTORY.md`](../HISTORY.md). Open work
belongs in [`../TODO.md`](../TODO.md); this document records dependency order
and completion boundaries rather than duplicating those ledgers.

## Execution Order

1. Finish the remaining live GPU presentation evidence for the existing
   Ganesh backends. Graphite is not part of this acceptance gate.
2. Enable hardware video decode per host while retaining software decode as
   the correctness fallback and reporting actual hardware use separately.
3. Define and verify zero-copy decoded-texture ownership, device affinity, and
   synchronization. Keep a measured CPU-download fallback where direct interop
   is unavailable.
4. Add capture, conversion, hardware/software encode, timestamp, and muxing on
   top of the accepted frame and playback contracts.
5. Add transport, buffering, back-pressure, reconnect, and streaming protocol
   integration only after local media timing is stable.
6. Use WebRTC as a consumer-driven integration milestone, then add the real
   QuickJS core, event loop, modules, native bindings, and JavaScript-visible
   graphics/media services. Only then extend isolated-stage acceptance from
   `04-gfx-media` to `05-js`.

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
- Upstream source is not patched merely to hide a CRT/PAL deficiency. Porting
  follows the Bionic-first discipline in [`../AGENTS.md`](../AGENTS.md).

## Deferred Scope

Graphite, V8, Chromium/Ozone, a full compositor/desktop environment, Android
framework or APK compatibility, and unmodified glibc binary compatibility are
not gates for the current Ganesh/FFmpeg/QuickJS sequence. They remain later
consumers or benchmarks after the lower contracts have stable evidence.
