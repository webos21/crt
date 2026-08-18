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

The 2026-08-18 three-host window bring-up changed the practical next step:
`libcrtgfx` already has a project-owned Weston-style toplevel/surface boundary,
a BGRA8888 software frame path, and real host adapters on Windows, Linux, and
macOS. The next work should therefore keep Weston/Wayland import pressure small
until the frame and input contracts are stable.

1. Lock the software frame lifecycle first.
   - `begin_frame()` returns a writable, caller-owned-for-the-frame CPU buffer.
   - A second `begin_frame()` before `end_frame()` is invalid.
   - `end_frame()` submits the frame to the host backend; after it returns, the
     caller must not assume the submitted storage can be mutated by the host.
   - Backends may either copy the pixels into host-owned storage (current Win32
     and Cocoa policy) or keep submitted storage alive until a compositor
     release event (Linux Wayland `wl_buffer::release` policy).
2. Connect the locked software frame to Skia CPU raster drawing.
3. Study Weston/wlroots/Wayland protocol sources to decide whether to import
   protocol XML/generated helpers, a small protocol library, or no code yet.
4. Add input/event delivery across Linux Wayland, Win32, and Cocoa.
5. Add text/image/font staging once the Skia raster smoke is stable.
6. Defer GPU texture/direct-render and media handoff until frame/input
   semantics are stable on all three hosts.

`libcrtgfx/third_party/wayland/` stays empty until a specific import target is
chosen with license/provenance/build implications recorded.

## Linux Host Adapter (done, first cut)

`libcrtgfx/src/arch/linux/window_wayland.c` implements the "start with the
simplest available Wayland/DRM/EGL/Vulkan/OpenGL path" step from the Import
Strategy above, choosing plain core-protocol Wayland + `xdg-shell` over a
compositor library import (step 6 stays deferred) or DRM/EGL/Vulkan (a later,
GPU-path milestone). Hand-rolled wire protocol rather than linking
`libwayland-client`, matching the win32 adapter's own no-host-SDK-headers
style (`window_win32.c` never includes `<windows.h>` either) -- opcodes and
argument layouts were taken directly from the real upstream `wayland.xml`/
`xdg-shell.xml`, not guessed.

Covers: connect, `wl_registry` global enumeration + bind (`wl_compositor`/
`wl_shm`/`xdg_wm_base`), `wl_surface`/`xdg_surface`/`xdg_toplevel` creation,
the `xdg_surface::configure`/`ack_configure` handshake, `xdg_wm_base::ping`/
`pong`, `xdg_toplevel::close`, and `wl_shm`-backed software presentation
(`memfd_create()` + `mmap()` + `wl_shm_pool`/`wl_buffer`, matching the
BGRA8888-premultiplied `crtgfx_framebuffer` contract against
`WL_SHM_FORMAT_ARGB8888`, the same in-memory byte order).

Known scope cuts, documented in the file itself, not silent:
- one Wayland connection per window, no shared/global display object across
  multiple simultaneous windows yet (`crtgfx_host_window_dispatch()` has no
  window parameter at all, matching Win32's thread-global message queue, so
  it operates on a single process-wide "active window");
- no keyboard/pointer/`wl_seat` input;
- object ids are never recycled.

Implementation update (2026-08-18): presented `wl_buffer` lifetime is now gated
by the real `wl_buffer::release` event in code. Each submitted `wl_shm` buffer
remains mapped/open until the compositor releases it, then the backend destroys
the `wl_buffer` object and frees its storage. This is intended to close the
earlier tight-render-loop tear/use-after-free risk and make Linux match the
common `begin_frame()`/`end_frame()` lifecycle contract. This specific release-
tracking change has passed C99/`-Werror` syntax checking from macOS; it still
needs a real Linux compositor rerun before the scope cut can be moved to
`HISTORY.md` as fully verified.

**Verified on a real GNOME/Mutter Wayland session** (not just compiled):
`crtgfx_window_smoke` passes the full real path end to end (create, get real
compositor-assigned size, draw, present, pump, destroy); `crtgfx_window_demo`
ran continuously for multiple seconds/hundreds of frames with a stable open-fd
count (no leak) and no crash or compositor-side protocol kill; graceful
`CRTGFX_ERROR_UNSUPPORTED` fallback confirmed both with `$WAYLAND_DISPLAY`/
`$XDG_RUNTIME_DIR` unset (headless-CI shape) and pointed at a nonexistent
socket. Full `ctest` stays green throughout. Not yet exercised on a
non-GNOME/Mutter compositor (wlroots-based ones like Sway, or KDE's
KWin) -- the protocol used here is universal core+stable-xdg-shell, so it
should behave the same, but that is not independently confirmed yet.

## macOS Host Adapter (done, first cut)

`libcrtgfx/src/arch/macos/window_cocoa.c` (2026-08-18) implements the
"host-window prototype" step for macOS, taking the concrete technique
from the Wawona/Wayoa/Cocoa-Way reference class named in this document's
own "Candidate References" above: drive real Cocoa (NSWindow/NSView/
CALayer) from plain C via the Objective-C runtime's own C ABI
(`objc_msgSend`/`objc_getClass`/`sel_registerName`/
`objc_allocateClassPair`), not the Objective-C language -- no `.m` file,
no ARC, no Xcode project, matching the win32 adapter's own
no-host-SDK-headers style (this file never `#import`s `<Cocoa/Cocoa.h>`,
the same way `window_win32.c` never includes `<windows.h>`).

Covers: `NSApplication` bootstrap for a bundle-less command-line process
(`setActivationPolicy:`/`finishLaunching`, required for the window to
actually gain focus and receive input without an `Info.plist`),
`NSWindow` creation with a titled/closable/resizable style mask, a
layer-backed content `NSView` (`-setWantsLayer:YES`), a runtime-defined
`NSWindowDelegate` class (`objc_allocateClassPair`) implementing
`windowShouldClose:`/`windowWillClose:`/`windowDidResize:`, and
`CGImage`-into-`CALayer.contents` software presentation.

**Performance direction, the concrete lesson taken from the Wawona/
Cocoa-Way reference class**: present frames via `CALayer.contents`
(hardware-composited by the WindowServer, the same fast path a real
GPU-backed layer would use) rather than the naive `-drawRect:` +
`CGContextDrawImage` invalidation round trip. Also improves on this
project's own Win32 adapter, not just matches it:
`-nextEventMatchingMask:untilDate:inMode:dequeue:` takes a real
deadline, so `crtgfx_host_window_dispatch()` blocks the thread
efficiently in the OS's own run-loop wait instead of Win32's
`PeekMessage`+`Sleep(1)` busy-poll loop (Win32 has no
"wait-up-to-N-milliseconds" primitive to poll with).

Known scope cuts, documented in the file itself, not silent:
- single window per process, matching Win32/Linux's own thread-global
  dispatch shape (`crtgfx_host_window_dispatch()` takes no window
  parameter);
- no keyboard/mouse/trackpad input delivered to the caller yet (events
  are drained and dispatched to AppKit for correct window chrome/
  resize/close behavior, but not yet surfaced through the crtgfx public
  API) -- matching Linux's own "no keyboard/pointer/`wl_seat` input"
  cut;
- `crtgfx_host_window_present_software()` copies the caller's pixel
  buffer into its own allocation each frame rather than wrapping it in
  place, avoiding a real tear/use-after-free hazard the very next
  `crtgfx_window_begin_frame()` call could otherwise cause (mutating or
  reallocating the buffer before `CALayer`/WindowServer has actually
  consumed the previous frame) -- more conservative than Linux's own
  "torn down on the next present" documented cut above, at the cost of
  one `memcpy` per frame;
- `-frame`/`-bounds` (both `NSRect`-returning) are the one place this
  file's calling convention differs by architecture: AAPCS64 (arm64)
  resolves a struct return of this size through the ordinary
  `objc_msgSend` entry point, while x86_64's SysV ABI needs the
  dedicated `objc_msgSend_stret` entry point. Both paths are
  implemented; only the arm64 path has been run on real hardware this
  session.

**A real bug process-health checks alone could not catch, and one wrong
fix before the real one**: the first working build passed every
automated check (smoke test "ok", demo running stably for 8+ seconds,
full `ctest` green) while the window actually shown on screen displayed
one frame and then never visibly updated again -- caught only because
the user was watching the real screen and reported exactly what
Windows/Linux do differently. The first hypothesis (a bare
`[layer setContents:img]` only scheduling an *implicit* `CATransaction`
that never gets flushed without `-[NSApplication run]`'s own run-loop-
idle observer) was reasoned carefully and fixed (explicit
`[CATransaction begin]`/`[CATransaction commit]`), but **did not
actually fix the freeze** -- caught by re-testing rather than trusting
the reasoning.

The user then granted this session Screen Recording permission directly
("TCC 권한 주었으니 직접 모니터링 하면 된다"), which let this session
see the real bug for itself: two `screencapture` shots a second apart
were byte-identical, and a live `lldb attach`+`bt` on the (healthy-
looking, 0% CPU) running process showed it blocked inside
`-nextEventMatchingMask:untilDate:...` with `until_date` a real date
**roughly two and a half weeks in the future**. Temporary `dprintf()`
instrumentation traced this to `clock_gettime()` returning wildly
inconsistent, non-advancing timestamps into
`crtgfx_host_window_dispatch()`'s own deadline math.

Real root cause: a **symbol collision**, not an ABI or logic bug.
`crtgfx_window_demo`/`crtgfx_window_smoke` link this project's own libc
(`c`) as their C runtime, and that static archive's own
`clock_gettime()` (`libc/src/time.c`) wins symbol resolution over real
Darwin libSystem's at link time. `window_cocoa.c` had called
`clock_gettime()` with Darwin's *real* raw `CLOCK_MONOTONIC` value (`6`)
-- correct for libSystem's own implementation, but this project's own
macOS `__crt_sys_clock_gettime()` only recognizes its own, differently-
numbered clock ids (`include/time.h`: `CLOCK_REALTIME=0`/
`CLOCK_MONOTONIC=1`) and returns `-EINVAL` for anything else, **leaving
the output `struct timespec` completely unwritten -- stack garbage --
on every single call**. Fixed by using this project's own
`CLOCK_MONOTONIC` value (`1`) instead of Darwin's raw one, since the
symbol that actually links is this project's own implementation, not
the real Darwin one the original code assumed.

**Verified on this project's own real macOS aarch64 build host**, twice
over: before any of this landed in the real backend, a standalone C
probe using the identical technique (same extern declarations, same
frameworks) created a real `NSWindow`/`NSView`/`CALayer`, queried
`-frame`/`-bounds`, built a `CGImage` from a raw pixel buffer and set it
as `layer.contents`, and showed the window -- all without an
Objective-C exception -- and a second probe confirmed runtime
`objc_allocateClassPair`-based delegate classes correctly receive
method dispatch and round-trip an ivar. `crtgfx_window_smoke` passed the
full path end to end throughout (create, get real content-view size,
draw, present, pump, destroy, no `CRTGFX_ERROR_UNSUPPORTED` fallback) at
every stage of this investigation, including the two stages where the
window was actually frozen -- a reminder that this particular smoke
test cannot, by itself, distinguish a correctly-animating window from a
frozen one. After the real fix and with the newly-granted Screen
Recording permission, this session confirmed the animation directly for
itself: three `screencapture` shots one second apart during a
`crtgfx_window_demo` run are all pixel-different from each other (three
distinct `md5` hashes), showing the same shifting gradient pattern
Windows/Linux produce. Full `ctest` suite: 103/103 throughout. x86_64
macOS gets the identical fix (the symbol-collision mechanism is
link-time, not architecture-specific) but has not been independently
run on real Intel/Rosetta hardware this session. See `HISTORY.md`.
