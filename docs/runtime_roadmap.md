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
- Skia CPU raster/text and Ganesh GPU paths have/??
