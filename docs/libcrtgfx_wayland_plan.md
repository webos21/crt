# libcrtgfx Wayland Plan

This document records the initial plan for the Wayland-compatible compositor
part of `libcrtgfx`.

## Direction

Use existing Wayland/compositor projects as references first, not as immediate
source imports.

The goal is to learn and reuse proven architecture:

- top-level Wayland surfaces mapped 1:1 to host-native windows;
- thin protocol/compositor boundary above host-specific window and GPU code;
- software buffer path first, then GPU texture/direct-render paths;
- host event-loop integration without leaking host SDK headers into public CRT
  or `libcrtgfx` APIs.

This follows the direction in `docs/study/crt_gfx_direction_20260817.md`: avoid
writing a full compositor from scratch too early, but also avoid importing a
large compositor stack before the project's own boundary is clear.

## Candidate References

- **Wayland protocol libraries**: use as the protocol vocabulary and generator
  reference. The first concrete question is whether this project needs only
  generated protocol headers/state machines, or a real client/server library
  import.
- **wlroots/Weston**: use mainly as Linux reference designs for compositor
  roles, surface lifecycle, input dispatch, and `wl_shm`/buffer handling.
  They are not assumed to be directly portable to Windows/macOS.
- **WSLg**: reference for Windows integration ideas: Linux-side Wayland
  protocol, Windows-side individual window integration, and compositor/host
  handoff. This is architectural reference only; RDP/vGPU internals are not a
  dependency.
- **Wawona/Wayoa/Cocoa-Way style projects**: reference for macOS/iOS-native
  mapping of Wayland surfaces to host windows and Metal-backed presentation.

Before any reference project is vendored, record its license, language/runtime
requirements, build system, host dependencies, protocol scope, and whether it
can be used as source, design reference, or test fixture only.

## WSLg Lessons Without WSL

This project is not trying to run Linux binaries, Linux distributions, WSL, a
VM, or an RDP-backed desktop bridge. The useful part of WSLg is the shape of
the graphics boundary, not its product/runtime packaging.

Pieces worth carrying over as architecture:

- **Wayland-facing protocol boundary**: applications/toolkits can target a
  Wayland-like surface lifecycle while host-specific code remains below
  `libcrtgfx`.
- **One top-level surface maps to one host-native window**: avoid a single
  nested desktop window. A `xdg_toplevel`-like object should become a real
  Windows window on Windows, a real native window on macOS, and the simplest
  available native/compositor surface on Linux.
- **Buffer handoff is explicit**: compositor logic should receive frame/buffer
  metadata and pass it to the host presentation backend. Start with CPU
  buffers, then add GPU texture/direct-render handoff after correctness tests
  exist.
- **Host compositor remains the final presenter**: DWM, Quartz/Core Animation,
  or the Linux compositor should keep responsibility for actual desktop
  composition. `libcrtgfx` should avoid pretending to be an entire desktop.
- **Protocol and host adapters stay split**: keep Wayland-ish object/state
  handling in common code, and put Win32/D3D, Cocoa/Metal, and Linux-specific
  details under `src/arch/{windows,macos,linux}`.

Pieces to explicitly exclude:

- launching or hosting Linux binaries;
- depending on WSL, Hyper-V, a Linux kernel, or distro images;
- using RDP rail integration as a dependency;
- importing WSLg's vGPU/VA-API path as the first graphics backend;
- exposing Windows or Linux-specific handles as the default public API.

The resulting Windows shape should be:

```text
app/toolkit/JS
  -> Skia public drawing API
  -> crtgfx surface/frame API
  -> Wayland-compatible common surface state
  -> Windows host adapter
       - native window/event loop
       - software buffer blit first
       - Direct3D/Skia GPU surface later
  -> DWM presents the final window
```

## Boundary Decision

`libcrtgfx` should own a small compositor boundary, not a general host GUI API.

Public/project-owned headers should describe:

- display/runtime object;
- surface/toplevel lifecycle;
- frame callback and presentation lifecycle;
- input/event delivery shape;
- buffer submission shape;
- software/GPU backend choice.

They should not expose:

- Win32 `HWND`/Direct3D headers;
- Cocoa/Objective-C/Metal headers;
- Linux DRM/EGL/Vulkan/OpenGL headers as the default public API;
- full Wayland compositor implementation details;
- Skia drawing wrappers that replace Skia headers.

## Import Strategy

1. **Study-only phase**
   - read WSLg, Wawona/Wayoa, Weston, wlroots, and Wayland protocol sources;
   - summarize which pieces map cleanly to this project's Bionic-compatible
     model;
   - document any license/build-system blockers before copying code.

2. **Protocol-minimum phase**
   - decide whether to import Wayland protocol XML and code generation first;
   - build a tiny in-tree protocol/surface model if that is enough for the
     first smoke;
   - avoid importing full compositor policy until a real consumer needs it.

3. **Software-surface smoke**
   - create a `crtgfx_surface` with a CPU pixel buffer;
   - present a deterministic frame through a host-neutral test path;
   - keep this independent of Skia and Wayland until the frame lifecycle is
     stable.

4. **Skia bridge**
   - connect a `crtgfx_surface` to Skia's CPU raster surface first;
   - expose normal Skia headers and add a deterministic draw smoke;
   - only then evaluate GPU backends.

5. **Host-window prototype**
   - Windows: map a toplevel surface to a host window backend below
     `src/arch/windows`;
   - macOS: map a toplevel surface to a host window backend below
     `src/arch/macos`;
   - Linux: start with the simplest available Wayland/DRM/EGL/Vulkan/OpenGL
     path, chosen by what can be tested on real hardware.

6. **Compositor/library import**
   - import or adapt Wayland/wlroots/Weston pieces only after the small
     boundary above has real tests;
   - keep imported code in `libcrtgfx/third_party/` with provenance and local
     build glue, following the existing `shell/` and `third_party/` discipline.

## Risks

- Importing a full compositor too early could force Linux-specific assumptions
  into Windows/macOS and make `libcrtgfx` look like a port of that compositor
  rather than this project's own graphics boundary.
- Writing everything from scratch could waste effort on protocol details that
  existing projects have already solved.
- Event-loop integration is likely the hardest host boundary: Windows message
  loops, macOS run loops, Linux Wayland/epoll-style dispatch, and JS timers all
  eventually need a single scheduling contract.
- GPU zero-copy is a later optimization, not the first correctness target.

## Immediate Plan

- Keep `libcrtgfx/third_party/wayland/` empty until a specific import target is
  chosen.
- Add a study note summarizing candidate reference projects before vendoring
  any code.
- Implement the first `crtgfx` surface/frame API as a project-owned boundary.
- Let Skia draw into that surface before adding real Wayland protocol plumbing.
