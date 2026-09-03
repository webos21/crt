# libcrtgfx

Graphics and compositor-facing upper runtime layer.

This library is intentionally separate from `libc`: host windows, GPU APIs,
surface composition, Skia integration, and Wayland-compatible protocol policy
belong here rather than in the Bionic-compatible low-level runtime.

Initial direction:

- expose normal Skia headers as the public 2D drawing API;
- keep project-owned `crtgfx` headers focused on runtime initialization,
  host-independent surfaces, frame presentation, event-loop integration, and
  backend selection;
- start by creating a real host window/toplevel surface, then add a software
  frame path, and connect that surface to Skia once the import is in place;
- keep Wayland as a compatibility boundary, not as public libc surface;
- place host adapters under `src/arch/{linux,macos,windows}`.

Current baseline:

- `include/crtgfx/window.h` exposes the first host-independent window API.
- `src/wayland_weston.c` owns the first Weston-style toplevel/surface state,
  directly under `src/` -- host adapters underneath live in
  `src/arch/{linux,macos,windows}`, no separate `src/common/` layer.
- `src/arch/windows/window_win32.c` is the real Windows host adapter, only
  the host layer underneath the Weston-style boundary.
- `src/arch/linux/window_wayland.c` is the real Linux host adapter: a
  hand-rolled Wayland client (no `libwayland-client` dependency, matching
  this project's no-host-SDK ethos) speaking the real core `wl_display`/
  `wl_registry`/`wl_compositor`/`wl_shm`/`wl_surface` protocol plus the
  stable `xdg_wm_base`/`xdg_surface`/`xdg_toplevel` shell extension
  directly over the `$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY` Unix socket.
  Software-only (`wl_shm`) for now, matching the Windows GDI path. Returns
  `CRTGFX_ERROR_UNSUPPORTED` (not a hard error) when no compositor is
  reachable at all, so headless CI keeps working exactly as before.
- `src/arch/macos/window_cocoa.c` is the real macOS host adapter: drives
  real Cocoa (`NSWindow`/`NSView`/`CALayer`) from plain C via the
  Objective-C runtime's own C ABI (`objc_msgSend`/`objc_getClass`/...), no
  `.m` file, matching the same no-host-SDK-headers ethos. Presents frames
  through `CALayer.contents` (hardware-composited), not `-drawRect:`.
  See `docs/libcrtgfx_wayland_plan.md` for both adapters' design, known
  scope cuts, and real-hardware verification record.
- `crtgfx_window_begin_frame()`/`crtgfx_window_end_frame()` expose the first
  software buffer present path, using BGRA8888 premultiplied pixels. Every
  host adapter presents this same buffer shape through its own backend.
- `crtgfx` and `crtgfx_shared` are default runtime artifacts installed into
  the sysroot; the shared runtime is also copied into the rootfs.
- `crtgfx_window_smoke` creates a window, rejects a nested frame, and presents
  repeated software frames for automated lifecycle testing.
- `crtgfx_window_demo` opens a visible window and animates the software
  frame path for manual bring-up, on all three hosts.
- `crtgfx_window_poll_event()` delivers keyboard/text/pointer input through a
  common queue on all three hosts. Linux uses Wayland seat devices plus the
  project-built xkbcommon port; Win32 and Cocoa translate their native events.
- Skia integration is a CRT-built dependency, not a host SDK link.
  Use `crtgfx-skia-fetch` to fetch the selected Skia milestone/ref and
  `crtgfx-skia-build` to configure/build/install Skia with `tools/crt-c++`
  against this project's sysroot and imported libc++. With
  `CRTGFX_ENABLE_SKIA=ON`, `crtgfx_skia_raster_smoke` verifies a real CPU
  `SkSurface`/`SkCanvas` plus FreeType-backed text, and
  `crtgfx_keyboard_interactive` draws typed text with the same path. This is
  verified on Linux, macOS, and Windows; host libc++ is not a substitute.

`include/crtgfx/gpu.h` (TODO.md's upper-runtime roadmap "Fix the common GPU
resource contract" step) is the real, host-independent shape a later real GPU
backend will implement underneath -- `crtgfx_gpu_device`/`_surface`/`_fence`
(opaque), `crtgfx_gpu_backend`/`crtgfx_gpu_memory_kind` (real enums, no host
SDK type ever named as anything but a symbolic tag), capability queries,
real atomic device retain/release, and device affinity. No real GPU backend
exists on any host yet (Windows' own private per-window D3D11 device,
`window_win32.c`, exists solely for `CRTGFX_EVENT_FRAME_COMPLETE`'s own
async-present signaling, not wired to this contract) -- `crtgfx_gpu_query_
capabilities()` honestly reports `CRTGFX_GPU_BACKEND_NONE`/0 devices
everywhere today, and device/surface creation correctly, always reports
`CRTGFX_ERROR_UNSUPPORTED`, the same graceful contract `crtgfx_window_
create()` already uses. `crtgfx_gpu_fence`, unlike device/surface, is a
real, working, host-independent CPU synchronization primitive right now
(built on this project's own `pthread_mutex_t`/`pthread_cond_t`) --
"software fallback must remain a first-class path" is exactly what a real,
working CPU fence is. `crtgfx_gpu_test` covers real argument validation,
the honest capability report, and a real cross-thread wait/signal/timeout
-- verified on Linux, Windows, and macOS. Landing this also surfaced and
fixed a real, previously-latent libc bug: `pthread_cond_timedwait()` never
actually honored `pthread_condattr_setclock(PTHREAD_COND_CLOCK_MONOTONIC)`,
always treating the deadline as `CLOCK_REALTIME` -- fixed in
`libc/src/pthread.c`, re-verified for real (elapsed wall time, not just
the return code) on all three hosts. See `HISTORY.md`'s 2026-09-03 entry
for the full trail.

See `docs/libcrtgfx_api_policy.md` for the API boundary decision.
See `docs/libcrtgfx_wayland_plan.md` for the Wayland/compositor plan.
