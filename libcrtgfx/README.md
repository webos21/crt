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
existed on any host when this contract first landed (Windows' own private
per-window D3D11 device, `window_win32.c`, exists solely for
`CRTGFX_EVENT_FRAME_COMPLETE`'s own async-present signaling, not wired to
this contract) -- `crtgfx_gpu_query_capabilities()` honestly reported
`CRTGFX_GPU_BACKEND_NONE`/0 devices everywhere, and device/surface creation
correctly, always reported `CRTGFX_ERROR_UNSUPPORTED`, the same graceful
contract `crtgfx_window_create()` already uses (Linux and Windows now
have real backends -- see below). `crtgfx_gpu_fence`, unlike device/surface, is a
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

**Enable Skia GPU rendering -- Linux/Vulkan offscreen vertical slice**
(2026-09-03, TODO.md's next roadmap step): `src/arch/linux/gpu_vulkan.c` is
the first real `crtgfx_gpu_device` backend anywhere in this project --
`crtgfx_gpu_query_capabilities()`/`crtgfx_gpu_device_create()` are now
genuinely real on Linux (when a real `libvulkan` is found at configure
time; absent it, Linux keeps the prior honest `NONE`/`UNSUPPORTED`
behavior). Prefers a real hardware-backed device (confirmed against Mesa's
`dzn`, Vulkan-over-D3D12) over the always-available `llvmpipe` software
fallback. `crtgfx_gpu_surface_create()` deliberately still reports
`CRTGFX_ERROR_UNSUPPORTED` everywhere, including with a real device now --
no host can yet present a Ganesh-drawn surface to a real on-screen window
(`window_wayland.c` has no real `wl_surface*` to hand Vulkan's WSI, and
Windows/macOS have no real GPU backend yet either). Real Ganesh/Vulkan
*rendering* correctness is proven offscreen instead: `crtgfx_skia_make_
gpu_context()`/`crtgfx_skia_make_gpu_offscreen_surface()` (`skia_bridge.cc`,
declared in `crtgfx/skia.h` behind `CRTGFX_HAVE_VULKAN`) build a real
`GrDirectContext`/GPU-backed `SkSurface`; `crtgfx_skia_gpu_offscreen_smoke`
draws the same reference scene `skia_cpu_coverage_test.cc` can also draw
(`tests/skia_reference_scene.h`) and covers real draw+readback, resize,
and device-loss+recreation (the closest safe, portable equivalent Vulkan
offers to D3D12's clean `RemoveDevice()`). Landing this surfaced a real,
systemic ELF symbol-interposition bug, unrelated to Vulkan specifically:
this project's own statically-linked `readdir()`/`opendir()` were being
exported into every Linux executable's dynamic symbol table, silently
shadowing the real system Vulkan loader's own internal directory-scanning
calls -- fixed with `-Wl,--exclude-libs,ALL`, relevant to any future real
host-library integration on Linux. Verified for real on Linux (WSL) and
Windows (this slice's own Linux-only branches left Windows untouched);
macOS re-verification pending (no macOS code touched at all). Windows/
D3D12, macOS/Metal, and live on-screen presentation remain explicit,
separate follow-up steps. See `HISTORY.md`'s 2026-09-03 entry for the full
trail, including two further real findings (this project's own `dlopen()`
has no real ELF dynamic loading yet, and `skia_use_vma=false` silently
disabled Ganesh's own internal memory-allocator fallback entirely).

**Enable Skia GPU rendering -- Windows/D3D12 offscreen vertical slice**
(2026-09-04): `src/arch/windows/gpu_win32.c` (new, hand-declared, no host
headers) is the second real `crtgfx_gpu_device` backend, additive
alongside `window_win32.c`'s existing, untouched D3D11 presentation
pipeline -- real device/queue/adapter creation, hardware-preferred
ordering with a WARP fallback, real vtable slots/IIDs verified directly
against the local Windows SDK headers. Skia's own public `GrD3DTypes.h`
forces real `<d3d12.h>`/`<dxgi1_4.h>`, same as the Vulkan slice's own
Vulkan-header exception -- but the raw Microsoft Windows SDK's own
versions of those are a real, confirmed dead end under this project's
mingw-target clang (`winnt.h` assumes real MSVC-only architecture macros
and atomic intrinsics clang only implements for its `*-windows-msvc`
target), so this project vendors mingw-w64's own real, clang/gcc-native
header set instead (`tools/fetch_mingw_w64_headers.py`, pinned to its
`v14.0.0` tag) -- `libstdc++/third_party/win32_shim`'s existing minimal
shims now `#include_next`-forward to it when available, falling back to
their own narrow declarations otherwise (a true no-op for the unrelated
libcxx/libunwind bootstrap build, which never puts it on its own include
path). `crtgfx_skia_make_gpu_context()`/`crtgfx_skia_make_gpu_offscreen_
surface()` gained a real `#elif defined(CRTGFX_HAVE_D3D12)` branch
(`skia_bridge.cc`) mirroring the Vulkan one -- same shared declarations
in `crtgfx/skia.h`, same `tests/skia_gpu_offscreen_smoke.cc` draw/
readback/resize coverage, with D3D12's own real, clean
`ID3D12Device5::RemoveDevice()` + `GetDeviceRemovedReason()` standing in
for Vulkan's own double-`vkDestroyDevice()`-avoiding device-loss design.
Landing this also caught and fixed a real, previously-latent `tools/
crt-ar` bug (its own response-file expansion defeated the response
file's entire purpose once `libskia.a`'s object count -- Ganesh's D3D
backend plus `d3d12allocator`/`spirv-cross` -- grew past Windows' real
command-line length limit) and a real ownership bug in `skia_bridge.cc`
(a raw-pointer assignment into a `gr_cp<T>` COM smart-pointer field that
would have double-released the device, only caught once a real, complete
`<d3d12.h>` was finally available to compile against). Verified for real
on Windows (`crtgfx_skia_gpu_offscreen_smoke`'s full device-loss/recovery
cycle included) and Linux/WSL (confirming zero regression to the Vulkan
slice or the libcxx/libunwind bootstrap from the shared win32_shim/
crt-ar changes); macOS re-verification stays separately pending, same as
the Vulkan slice. See `HISTORY.md`'s 2026-09-04 entry for the full trail.

See `docs/libcrtgfx_api_policy.md` for the API boundary decision.
See `docs/libcrtgfx_wayland_plan.md` for the Wayland/compositor plan.
