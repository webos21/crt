# libcrtgfx Wayland Plan

This document records the initial plan for the Wayland-compatible compositor
part of `libcrtgfx`.

## Direction

Use existing Wayland/compositor projects as architecture references before
expanding the imported surface. A pinned Wayland core build now exists for
toolchain/protocol validation, while the Linux host adapter remains a small
project-owned client and no full compositor has been imported.

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

- **Wayland protocol libraries**: protocol vocabulary, scanner, and client
  library reference. The current external build covers the scanner/client
  slice; generated protocol state and full server/compositor policy should be
  added only for a concrete consumer.
- **wlroots/Weston**: use mainly as Linux reference designs for compositor
  roles, surface lifecycle, input dispatch, and `wl_shm`/buffer handling.
  They are not assumed to be directly portable to Windows/macOS.
- **WSLg**: reference for Windows integration ideas: Linux-side Wayland
  protocol, Windows-side individual window integration, and compositor/host
  handoff. This is architectural reference only; RDP/vGPU internals are not a
  dependency.
- **Wawona/Wayoa/Cocoa-Way style projects**: reference for macOS/iOS-native
  mapping of Wayland surfaces to host windows and Metal-backed presentation.

Before any additional reference project is vendored, record its license,
language/runtime requirements, build system, host dependencies, protocol
scope, and whether it is source, design reference, or test fixture only.

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

## Milestone Result And Next Plan

The original CPU/software milestone is complete as of 2026-08-25:

1. The software-frame contract is fixed and tested. Nested `begin_frame()` is
   rejected, repeated submission is exercised, Windows/macOS copy into
   host-owned storage, and Linux waits for real `wl_buffer::release` before
   reclaiming submitted `wl_shm` storage.
2. Skia `m148` builds with the CRT toolchain/imported libc++ and draws through
   an `SkSurface`/`SkCanvas` attached to the common software frame.
3. Wayland protocol and build policy are recorded under
   `libcrtgfx/third_party/wayland/`; fetched source and build output remain
   under the active preset's `out/` tree. The Linux host adapter still uses
   its small project-owned wire implementation rather than depending on the
   host `libwayland-client`.
4. Keyboard and pointer input are live on Linux Wayland, Win32, and Cocoa
   through the common `crtgfx_window_poll_event()` API. Linux text composition
   uses the project-built xkbcommon port.
5. Skia's FreeType-backed custom-directory font manager renders the bundled
   font on all three hosts, including typed text in the interactive demo.

The next tranche is broader deterministic Skia drawing coverage, followed by
a GPU buffer/surface handoff contract. Direct3D, Metal, Linux EGL/Vulkan/
dmabuf, full compositor policy, Chromium Ozone, and media texture handoff stay
outside the completed CPU-raster milestone.

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
- object ids are never recycled.

**Multi-window support (2026-08-29, Phase 1 of the window/event API
completion plan):** the previous "one Wayland connection per window, no
shared/global display object" cut is closed. `crtgfx_wl_connection` now
holds the one shared fd and every registry-bound singleton (wl_compositor/
wl_shm/xdg_wm_base/wl_seat/wl_keyboard/wl_pointer/xkb state) across every
live window; `crtgfx_host_window` holds only what is genuinely per-window
(its own wl_surface/xdg_surface/xdg_toplevel, its own wl_shm buffers).
`crtgfx_host_window_dispatch()` still takes no window parameter -- that
was never the actual limitation (it already matched Win32's own thread-
global message-queue shape correctly) -- but now pumps the one shared
connection and routes each event to the right window via real
`wl_pointer`/`wl_keyboard` `enter`/`leave` focus tracking (previously
unread from the wire at all, since there was only ever one possible
destination). Verified live against a real WSLg compositor: a second
window created while the first is still open reuses the existing
connection (confirmed via `CRTGFX_WAYLAND_DEBUG=1` tracing -- the
registry/seat/keymap negotiation sequence appears exactly once, not
twice), and destroying the second window leaves the first fully
functional.

Input is no longer a scope cut: `wl_seat`/`wl_keyboard`/`wl_pointer` events
feed the common queue, and xkbcommon converts keymap/modifier state into UTF-8
text. This path has been exercised against a real compositor with keyboard
and pointer activity.

Implementation update (2026-08-18): presented `wl_buffer` lifetime is now gated
by the real `wl_buffer::release` event in code. Each submitted `wl_shm` buffer
remains mapped/open until the compositor releases it, then the backend destroys
the `wl_buffer` object and frees its storage. This is intended to close the
earlier tight-render-loop tear/use-after-free risk and make Linux match the
common `begin_frame()`/`end_frame()` lifecycle contract. The release-tracking
path has since been exercised on real Linux together with repeated software
presentation and input dispatch.

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

## Linux Native Wayland Backend + GPU Presentation (2026-09-07, first cut)

TODO.md's "Finish live GPU presentation everywhere" roadmap step starts on
Linux, driven by a real structural gap the Linux Host Adapter section above
does not have an answer for: Vulkan's own `vkCreateWaylandSurfaceKHR()`
needs a *live* `struct wl_display*`/`struct wl_surface*` from a real
`libwayland-client` connection, and the hand-rolled backend's own wire-
protocol object ids can never be retrofitted into one after the fact (two
independent client-side object-id allocators cannot safely share one wire
connection to the same compositor).

**Chosen design, the user's own explicit direction: a dual-backend
strangler-fig migration, not a special-cased GPU exception bolted onto the
existing backend.** Two fully independent Linux window backends now exist
behind the same public `crtgfx/window.h` API:

- **Legacy backend** (`window_wayland.c`, the hand-rolled wire-protocol
  client documented above): frozen as of this date. Continues to own every
  software-presented window exactly as before -- this pass added only a
  small dispatch check at the very top of `crtgfx_host_window_create()`
  (does `desc->flags` have `CRTGFX_WINDOW_GPU_PRESENTATION`?) and a
  `backend_tag` first field on `struct crtgfx_host_window` purely so the
  other four shared entry points (`_destroy`/`_show`/`_get_size`/
  `_present_software`) can tell which backend actually owns a given opaque
  `crtgfx_host_window*` before touching it -- every other line of this
  file is untouched.
- **Native backend** (`window_wayland_native.c`, new): real
  `libwayland-client` + real `xdg-shell` protocol bindings (both built by
  `tools/build_wayland.py`/`tools/fetch_wayland.py`, already documented
  above as "Wayland core... not wired into libcrtgfx's own backend" --
  that gap is now closed, and `xdg-shell.xml` itself is now also fetched,
  via a real, separately pinned sparse checkout of `wayland-protocols`,
  the same `tools/build_wayland.py`). Owns a GPU-presenting window's
  *entire* lifecycle from creation -- its own connection, registry
  (`wl_compositor`/`xdg_wm_base`/`wl_seat`, all bound at protocol version
  1), surface/`xdg_surface`/`xdg_toplevel`, real keyboard input (same
  xkbcommon keymap-compile-then-translate shape the legacy backend already
  uses) and real pointer input (motion/button/scroll). A window selects
  this backend at `crtgfx_window_create()` time via the new
  `CRTGFX_WINDOW_GPU_PRESENTATION` desc flag (`crtgfx/window.h`) -- see
  that flag's own doc comment for the full "why" and for the deliberate
  WSL bypass (`/proc/sys/kernel/osrelease` containing "microsoft" makes
  `crtgfx_host_window_create()` return `CRTGFX_ERROR_UNSUPPORTED`
  immediately, before ever attempting a real connection -- verified for
  real on this session's own WSL host: `crtgfx_gpu_window_demo` exits
  promptly with a clear message, no hang, no crash).

Both backends can coexist in the same process (a legacy software window and
a native GPU window open at once); `crtgfx_host_window_dispatch()` (the one
entry point with no `host` parameter, since it is genuinely global) now
checks which backend(s) have a live connection and pumps accordingly --
delegates the full timeout budget to whichever single backend is actually in
use (the common case, and the only one any real demo in this project
exercises so far), or splits the budget (non-blocking-drain the legacy
connection, then block on the native one) if both happen to be live at
once, a real, documented, honest limitation rather than a genuine two-fd
`poll()` -- see that function's own comment for the exact policy.

**Real GPU plumbing landed alongside the native backend, not left for
later**: `src/arch/linux/gpu_vulkan.c` (previously offscreen-only, see
TODO.md's "Enable Skia GPU rendering" step) now probes and, when available,
enables `VK_KHR_surface`/`VK_KHR_wayland_surface` on every instance it
creates and `VK_KHR_swapchain` on every device -- probed, not forced, so a
host with no real WSI support (headless CI, a pure-compute ICD) gets
exactly the same zero-extension instance/device this file always created
before today, and the already-verified offscreen Ganesh vertical slice
(`crtgfx_skia_gpu_offscreen_smoke`) is unaffected. `crtgfx_gpu_surface_
create()` (`src/gpu.c`, `crtgfx/gpu.h`) gained a real Linux/Vulkan branch:
resolves the native backend's own live `wl_display`/`wl_surface` pair
(`crtgfx_native_wl_get_surface_handles()`), creates a real `VkSurfaceKHR`,
checks real presentation-queue support, and builds a real `VkSwapchainKHR`
sized to the surface's own current extent. A new, small, additive
presentation contract exists in `crtgfx/gpu.h`, paralleling
`crtgfx_window_begin_frame()`/`_end_frame()`'s own shape rather than
replacing it: `crtgfx_gpu_surface_get_size()`/`_acquire()`/`_clear()`/
`_present()`. `crtgfx_gpu_surface_clear()` is this vertical slice's own
deliberate, honestly-scoped stand-in for a full Ganesh/Skia render onto the
acquired image -- a real, minimal "draw" primitive (a solid RGBA clear,
real submitted GPU work, really presented) proving the whole real
acquire/submit/present pipeline end to end. Wiring the already-proven
offscreen Ganesh pipeline (`src/skia_bridge.cc`) onto a surface's own
acquired image instead of an offscreen image is real, separate, deferred
follow-up work, not attempted in this pass.

A new manual demo, `crtgfx_gpu_window_demo` (`libcrtgfx/tools/
gpu_window_demo.c`, same "run it yourself, needs a real compositor" shape
as `crtgfx_window_demo`/`crtgfx_keyboard_interactive` -- not wired into
`ctest`), opens a real `CRTGFX_WINDOW_GPU_PRESENTATION` window and presents
a continuously animated solid color through the full real pipeline above.

**Verification status, stated honestly**: this session's own real
environment (Windows+WSL2) compiled and linked everything end to end --
`crtgfx`/`crtgfx_shared` both build clean with `window_wayland_native.c`
genuinely compiled in, the real `libwayland-client`+`xdg-shell` build now
lands in the *main* build tree (not just the standalone `crtgfx-wayland-
smoke` shadow directory that already existed), `crtgfx_gpu_window_demo`
links and runs, and the WSL bypass was confirmed for real (prompt, graceful
exit, no hang). Full `ctest`: 117/117, zero regression to the legacy
backend or anything else. Real **on-screen** verification (does a window
actually appear and animate on a real GNOME/Mutter or wlroots compositor)
is explicitly the user's own separate step on real Linux hardware, not
WSL -- mirroring the macOS Metal slice's own real-hardware verification
precedent recorded above. Also explicitly not attempted this pass, matching
the approved plan's own non-goals: software (`wl_shm`) presentation,
multi-window, or clipboard support in the native backend (a later Phase 3,
once the native backend needs real parity with the legacy one); retiring
the legacy backend (Phase 4, only once the native backend has real parity);
real swapchain recreation on resize (`VK_ERROR_OUT_OF_DATE_KHR` currently
surfaces as a real, honest `CRTGFX_ERROR_HOST` from `crtgfx_gpu_surface_
acquire()`/`_present()` rather than being handled); Windows/macOS live GPU
presentation wiring (their own real, separate, lower-risk follow-up -- no
structural gap on either host, just wiring `crtgfx_gpu_surface_create()` to
each host's own already-existing swap chain/layer code).

A real CMake dependency-graph cycle was found and fixed along the way:
`crtgfx-wayland-build` (needed to build the real `libwayland-client`/
`xdg-shell` bindings used by `window_wayland_native.c`) transitively needs
`port-build-expat`/`port-build-libffi`, which need the root `sysroot`
target, which itself `DEPENDS` on `crtgfx`/`crtgfx_shared` (to stage
`libcrtgfx.so` into the rootfs) -- wiring `crtgfx`/`crtgfx_backend_objects`
to `add_dependencies()` on `crtgfx-wayland-build` directly closed that into
a real cycle CMake correctly refused to generate. Fixed the same way root
`CMakeLists.txt` already documents fixing an identical-shaped problem for
`crtgfx_skia_objects`/`crt-libcxx-sysroot` (see that file's own comment
right after `add_custom_target(sysroot ...)`): sidestep the ninja
dependency graph entirely via a configure-time `EXISTS` check on the real,
already-built `.a` file, degrading gracefully (the native backend simply
is not compiled in) on a from-scratch configure that has never run
`crtgfx-wayland-build`, rather than either a hard cycle or a `FATAL_ERROR`
(this feature is on by default whenever a real `libvulkan` is found, unlike
Skia's own opt-in `CRTGFX_ENABLE_SKIA` flag, so a `FATAL_ERROR` would break
every existing from-scratch Linux+Vulkan configure, not just one a user
deliberately opted into). Run `cmake --build . --target crtgfx-wayland-
build` once, then reconfigure, to pick up the native backend on a build
tree that has never built it before.

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
- **Multi-window support closed 2026-08-30** (Phase 1 of the window/event
  API completion plan, same day as Linux and Windows): `crtgfx_cocoa_
  windows` (a linked list, replacing the previous single `crtgfx_cocoa_
  active` pointer) tracks every live window, and `crtgfx_host_window_
  dispatch()` now resolves which one a given `NSEvent` actually belongs to
  via `-[NSEvent window]` (`crtgfx_cocoa_find_window()`) before routing
  it. `crtgfx_host_window_dispatch()` itself still takes no window
  parameter -- that was never the actual limitation on any host (it
  matches Cocoa's own real per-process `NSApplication` run loop, exactly
  as Linux's shared connection and Win32's thread-global message queue
  also do). Also added the same day: `CRTGFX_EVENT_FOCUS_IN`/`FOCUS_OUT`
  via real `windowDidBecomeKey:`/`windowDidResignKey:` delegate callbacks,
  and `CRTGFX_EVENT_POINTER_SCROLL` via real `NSEventTypeScrollWheel`.
  Same verification status as the original 2026-08-25 keyboard/mouse
  input work: reasoned from Apple's own long-published AppKit ABI, not
  independently run on real macOS hardware this session (still no macOS
  host access) -- flagged in the code itself, real-hardware confirmation
  still open. A real, previously-latent build risk was found and fixed
  along the way, in code that predates this pass and had already passed
  on real hardware before: this file's whole `objc_msgSend` cast-and-call
  pattern (the standard, unavoidable way to invoke it from plain C) trips
  a genuinely new Clang diagnostic, `-Wcast-function-type-mismatch`, under
  `-Werror` on at least one real, current LLVM.org build (22.1.8) --
  suppressed at the build level (`libcrtgfx/CMakeLists.txt`, matching the
  same cross-Clang-version-safe pattern this same day's mksh fix
  established) rather than by rewriting the cast pattern, since there is
  no alternative pattern that avoids it;
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

Keyboard, modifier, text, and mouse events are now translated from `NSEvent`
into the common crtgfx event queue and have been verified live on real macOS
hardware. Trackpad-specific gestures remain outside the current public event
surface.

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
