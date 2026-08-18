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

Current bring-up:

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
- `crtgfx_window_smoke` creates a hidden window, fills a software frame, and
  presents it for automated testing.
- `crtgfx_window_demo` opens a visible window and animates the software
  frame path for manual bring-up, on all three hosts.
- Skia integration is staged as a CRT-built dependency, not a host SDK link.
  Use `crtgfx-skia-fetch` to fetch the selected Skia milestone/ref and
  `crtgfx-skia-build` to configure/build/install Skia with `tools/crt-c++`
  against this project's sysroot. After that install exists, reconfigure with
  `CRTGFX_ENABLE_SKIA=ON` (or let the default auto-enable when the installed
  library is found) to register `crtgfx_skia_raster_smoke`.

See `docs/libcrtgfx_api_policy.md` for the API boundary decision.
See `docs/libcrtgfx_wayland_plan.md` for the Wayland/compositor plan.
