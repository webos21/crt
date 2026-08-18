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
- `src/common/wayland_weston.c` owns the first Weston-style toplevel/surface
  state.
- `src/arch/windows/window_win32.c` is now only the host adapter underneath
  that Weston-style boundary.
- `src/arch/linux/window_wayland.c` is the real Linux host adapter: a
  hand-rolled Wayland client (no `libwayland-client` dependency, matching
  this project's no-host-SDK ethos) speaking the real core `wl_display`/
  `wl_registry`/`wl_compositor`/`wl_shm`/`wl_surface` protocol plus the
  stable `xdg_wm_base`/`xdg_surface`/`xdg_toplevel` shell extension
  directly over the `$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY` Unix socket.
  Software-only (`wl_shm`) for now, matching the Windows GDI path. Returns
  `CRTGFX_ERROR_UNSUPPORTED` (not a hard error) when no compositor is
  reachable at all, so headless CI keeps working exactly as before. See
  `docs/libcrtgfx_wayland_plan.md` for the design and known scope cuts.
- `crtgfx_window_begin_frame()`/`crtgfx_window_end_frame()` expose the first
  software buffer present path, using BGRA8888 premultiplied pixels. Windows
  presents this buffer through the host adapter.
- `crtgfx` and `crtgfx_shared` are default runtime artifacts installed into
  the sysroot; the shared runtime is also copied into the rootfs.
- `crtgfx_window_smoke` creates a hidden window, fills a software frame, and
  presents it for automated testing.
- `crtgfx_window_demo` opens a visible Windows window and animates the software
  frame path for manual bring-up.

See `docs/libcrtgfx_api_policy.md` for the API boundary decision.
See `docs/libcrtgfx_wayland_plan.md` for the Wayland/compositor plan.
