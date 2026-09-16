# Host ABI Firewall

TODO.md's "Allocator baseline validation before Upper Runtime" tranche 7
("freeze the Host ABI firewall before hardware decode"). This document
fixes the ownership rule for every boundary where CRT code transports an
**opaque object created by a host library** (a real system Vulkan driver,
a real Wayland compositor, real FFmpeg/libav\* structures, and the
upcoming VA-API/EGL/PipeWire boundaries hardware decode and zero-copy
will add) before that next tranche of work begins, per this project's own
"freeze the boundary before extending it" discipline (the same discipline
that produced the CRT_1.0 ELF symbol-versioning fix and the static/shared
CRT-runtime-mixing stage gate documented in `HISTORY.md`).

## The rule

> A host-library opaque object is created, retained, synchronized, and
> destroyed **only** by the host library (and allocator domain) that
> created it. CRT adapter code may store the pointer/handle and pass it
> back into that same host library's own API. CRT adapter code must
> never: reinterpret the object's private layout, call CRT's own
> `free()`/`malloc()`-family functions on it, or otherwise assume it is
> memory CRT's own allocator owns.

A CRT-owned wrapper struct that merely *contains* a host handle (e.g.
`struct crtgfx_native_wl_window` holding a `wl_surface*`) is not itself a
host object -- CRT allocated it, so CRT's own `free()` on *that* struct is
correct, as long as every host handle it held was released through its
own host API first, in a sensible teardown order. The audit below checks
exactly that ordering.

## Ownership matrix

| Boundary | Opaque object(s) | Create | Retain/reference | Synchronization owner | Destroy | CRT `free()` |
| --- | --- | --- | --- | --- | --- | --- |
| Wayland | `wl_display*`, `wl_registry*`, `wl_compositor*`, `wl_surface*`, `wl_seat*`, `wl_keyboard*`, `wl_pointer*`, `xdg_wm_base*`, `xdg_surface*`, `xdg_toplevel*` | `wl_display_connect()`, `wl_registry_bind()`, `wl_compositor_create_surface()`, `xdg_*_get_*()` | implicit (single-owner in this project's usage; no manual `wl_proxy` refcount) | libwayland event queue; CRT follows `prepare_read`/`flush`/poll/`read_events`/`dispatch_pending`, and compositor release/configure events govern reuse | `wl_*_destroy()` / `xdg_*_destroy()` / `wl_display_disconnect()` | **NEVER** |
| xkbcommon (used by the Wayland adapter) | `xkb_context*`, `xkb_keymap*`, `xkb_state*` | `xkb_context_new()`, `xkb_keymap_new_from_string()`/`_from_names()`, `xkb_state_new()` | `xkb_*_ref()` if ownership is shared; current adapter keeps one owner | adapter event thread updates state from Wayland keymap/modifier events; callers do not access a state after teardown begins | matching `xkb_*_unref()` in state -> keymap -> context order | **NEVER** |
| Vulkan | `VkInstance`, `VkDevice`, `VkSurfaceKHR`, `VkSwapchainKHR`, `VkImage` (swapchain-owned), `VkSemaphore`, `VkFence`, `VkCommandPool`, `VkCommandBuffer` | `vkCreateInstance()`, `vkCreateDevice()`, platform `vkCreate*SurfaceKHR()`, `vkCreateSwapchainKHR()`, `vkGetSwapchainImagesKHR()` (retrieves handles), `vkCreateSemaphore()`/`vkCreateFence()`/`vkCreateCommandPool()`, `vkAllocateCommandBuffers()` | driver-internal | Vulkan queue/fence/semaphore contract; CRT waits the relevant fence and calls `vkDeviceWaitIdle()` before teardown/replacement | matching `vkDestroy*()`; command buffers die with their pool and swapchain `VkImage` handles die with `vkDestroySwapchainKHR()` | **NEVER** (a CRT-allocated array holding handle values remains CRT-owned) |
| FFmpeg (software + hardware decode) | `AVFrame*`, `AVPacket*`, `AVCodecContext*`, `AVBufferRef*` (including HW device/frames refs), `SwrContext*`, `AVHWFramesContext*` | `av_frame_alloc()`, `av_packet_alloc()`, `avcodec_alloc_context3()`, `av_hwdevice_ctx_create()`, `av_hwframe_ctx_alloc()` + `av_hwframe_ctx_init()`, `swr_alloc()` | `av_frame_ref()`/`av_frame_clone()`, `av_buffer_ref()` | libavcodec send/receive contract plus referenced-buffer lifetime; hardware surfaces remain live until their `AVFrame`/`AVBufferRef` owners are unreferenced, with explicit transfer/sync APIs used before CPU access | `av_frame_unref()`/`av_frame_free()`, `av_packet_free()`, `avcodec_free_context()`, `av_buffer_unref()`, `swr_free()` | **NEVER** |
| VA-API (not yet implemented) | `VADisplay`, `VAContextID`, `VASurfaceID`, `VAConfigID` | `vaGetDisplay*()`, `vaCreateSurfaces()`, `vaCreateContext()`, `vaCreateConfig()` | driver-internal (IDs, not refcounted pointers) | libva/driver; `vaSyncSurface()` (or an explicitly documented equivalent) before CPU access, reuse, export, or destruction | `vaDestroySurfaces()`, `vaDestroyContext()`, `vaDestroyConfig()`, `vaTerminate()` | **NEVER** |
| EGL (not yet implemented) | `EGLDisplay`, `EGLContext`, `EGLSurface`, `EGLImageKHR`, `EGLSyncKHR` | `eglGetDisplay()`/`eglGetPlatformDisplay()`, `eglCreateContext()`, `eglCreateWindowSurface()`, `eglCreateImageKHR()`, `eglCreateSyncKHR()` | implementation-internal | EGL/graphics queue; current-context rules plus `EGLSyncKHR`/client wait or the chosen native-fence interop before cross-API reuse/destruction | `eglDestroyContext()`, `eglDestroySurface()`, `eglDestroyImageKHR()`, `eglDestroySyncKHR()`, `eglTerminate()` | **NEVER** |
| PipeWire (not yet implemented) | `pw_context*`, `pw_core*`, `pw_stream*`, `pw_buffer*`, `spa_buffer*` | `pw_context_new()`, `pw_context_connect()`, `pw_stream_new()`; buffers arrive borrowed via stream callbacks | borrowed buffers are returned with `pw_stream_queue_buffer()` | PipeWire stream thread/callback and buffer-queue contract; CRT must not access a returned buffer until PipeWire lends it again | `pw_stream_destroy()`, `pw_core_disconnect()`, `pw_context_destroy()`; borrowed `pw_buffer`/`spa_buffer` objects are returned, not freed | **NEVER** |

Every "not yet implemented" row above is intentionally filled in *before*
any code exists for that boundary -- see "Future-boundary acceptance
checklist" below for how this document gates that work, matching this
tranche's own explicit purpose: fix the rule and the detection mechanism
*before* hardware decode and zero-copy extend the number of boundaries
that could violate it, not after.

## Audit of existing boundaries (2026-09-16)

Sampled every destroy/cleanup path for the two real, already-implemented
host boundaries in this project (Wayland-native + Vulkan in `libcrtgfx`)
and the one real, already-implemented FFmpeg hardware-decode boundary in
`libcrtmedia`. All three are compliant with the rule above; no violation
found, none fixed (there was nothing to fix):

- `libcrtgfx/src/arch/linux/window_wayland_native.c`:
  - `crtgfx_native_wl_window_destroy()` (~line 734): `xdg_toplevel_destroy()`,
    `xdg_surface_destroy()`, `wl_surface_destroy()` all called on their
    respective host handles before `free(w)` -- `w` is `struct crtgfx_
    native_wl_window*`, a CRT-owned wrapper, not a host object.
  - `crtgfx_native_wl_connection_destroy()` (~line 547): `xkb_state_unref()`,
    `xkb_keymap_unref()`, `xkb_context_unref()`, `wl_keyboard_destroy()`,
    `wl_pointer_destroy()`, `wl_seat_destroy()`, `xdg_wm_base_destroy()`,
    `wl_compositor_destroy()`, `wl_registry_destroy()`, `wl_display_
    disconnect()` -- every host handle released through its own real API,
    in dependency order, before `free(conn)` (again a CRT-owned wrapper).
  - (`window_wayland.c`, the separate hand-rolled-wire-protocol software
    path, was also checked and found to call `free()` only on its own
    `crtgfx_*`-prefixed CRT-owned structs -- it implements the Wayland
    wire protocol directly over a raw socket rather than linking real
    libwayland-client at all, so there is no separate host allocator
    domain in that specific path to violate in the first place; see
    `TODO.md`'s own "split the Linux Wayland backend into two independent
    implementations" item for why this project has two such paths.)
- `libcrtgfx/src/arch/linux/gpu_vulkan.c` (~line 1260, surface teardown):
  `vkDestroySemaphore()` (x2), `vkDestroySwapchainKHR()`,
  `vkDestroySurfaceKHR()` all called on their real Vulkan handles;
  `free(surface->vk_images)` only frees the CRT-allocated *array* that
  holds `VkImage` handle values, never the images themselves (which
  `vkDestroySwapchainKHR()` already destroyed implicitly, per the real
  Vulkan API contract -- individual swapchain images are never destroyed
  separately).
- `libcrtmedia/src/codec.c` (`crtmedia_codec_release()`, ~line 297):
  `av_buffer_unref(&codec->hw_device_ctx)`, `swr_free()`, `av_frame_free()`,
  `av_packet_free()`, `avcodec_free_context()` all called on their real
  FFmpeg-owned objects before `free(codec)` (the CRT-owned wrapper).

## Mechanical detection

Two new CTest targets make the rule itself testable without needing real
GPU/compositor/decoder hardware, reusing `CRT_DEBUG_MALLOC`'s own
cross-instance owner-mismatch detection (TODO.md tranche 6) rather than
inventing a second mechanism:

- **`host_abi_firewall_test`** (`libc/tests/host_abi_firewall_test.c`):
  a synthetic fake host static library (`host_abi_firewall_fake_host`,
  implemented by `libc/tests/host_abi_firewall_fake_host.c`) with an opaque
  object type and its own private allocator domain
  (via the same preprocessor-renamed-`#include` trick tranche 6's
  `malloc_fault_instance_b.c` established), and `create()`/`retain()`/
  `release()` plus create/destroy counters. A simulated CRT adapter only
  stores/passes the opaque pointer and calls the host's own retain/
  release API -- exactly what real `crtgfx`/`crtmedia` adapter code does.
  Asserts `create_count == destroy_count` and `live_count == 0` after a
  create/retain/release cycle across 1000 objects. This is the
  "architecture is even *capable* of being correct" proof, registered in
  every host's routine CTest suite, with no real host library needed at all.
- **`host_abi_firewall_fault_test`** (`libc/tests/host_abi_firewall_
  fault_test.c` + `host_abi_firewall_fault_victim.c`): the deliberate
  negative case, matching tranche 6's own `malloc_fault_test`/`malloc_
  fault_victim` shape exactly. The victim allocates a fake-host object
  (through the fake host library's own private allocator domain) and then
  violates the rule on purpose -- calls CRT's own ordinary `free()` on it
  instead of the host's `fake_host_release()` -- and this is expected to
  trap via the identical cross-instance owner-mismatch mechanism tranche
  6 already validated. This is the "a real violation gets caught
  mechanically, not just by code review" proof.

Both are real CTest targets (`host_abi_firewall_test_runs`, `host_abi_
firewall_fault_test_runs`), not one-off scripts. They are registered in every
host's routine `ctest` invocation; Windows/x86_64 and WSL Linux/x86_64 passed
them repeatedly while this tranche was implemented, and the normal host/CI
matrix will exercise the same portable test pair on the remaining hosts.

## Existing real-host regression evidence

The real Wayland/Vulkan boundary above is not new code needing new
end-to-end verification for this tranche -- it already has extensive,
dated, real-hardware evidence in `HISTORY.md` (`presented=1`/`presented=
3/3` against real `libvulkan.so.1`/`libwayland-client.so.0`, repeated
across many entries). Most directly relevant, `HISTORY.md`'s 2026-09-16
"Made the packaged Linux Vulkan/Skia demo directly
runnable" entry fixed a real ELF symbol-version boundary violation found
via exactly the kind of binary-level audit this tranche calls for
(`LD_DEBUG=bindings` showed the host Vulkan loader's own `dlopen()` call
incorrectly binding to this project's own unversioned `libdl.so` instead
of real glibc) and reverified `device_count=1`/`presented=1` after the
fix. That is direct, dated evidence the real-host path both can break
this way and is actively monitored for it.

Binary-dependency auditing itself is not new tooling this tranche needed
to add either: `tools/crt_binary_dependencies.py` (ELF/PE/Mach-O
dependency classification against the SDK manifest) is already a
mandatory, non-skippable step of `tools/verify_dist.py`, which every
`crt-*-dist` CMake target already runs. The allocator-domain-specific Linux
gate is `tools/build_stage_04_gfx_media.py`'s
`validate_example_runtime_dependencies()`: a static example is rejected if
it directly gains any shared CRT `libc`/`libm`/`libdl`/libc++/libunwind
dependency. `tools/test_build_stage_04_cache.py` carries positive and negative
fixtures for that rule. The recent fresh native Linux/aarch64 isolated-stage
acceptance passed this gate and the direct packaged-binary smoke; this
tranche's Windows/x86_64 re-run of the fresh `04-gfx-media` distribution also
passes `verify_dist.py`. A future exported-symbol allowlist/ABI-baseline lint
remains separately planned in `TODO.md`; this tranche does not falsely claim
that broader work is already complete.

## Future-boundary acceptance checklist

Before VA-API, EGL, or PipeWire support is implemented, that work's own
acceptance must show:

- [ ] Create API and owning domain documented (add a row above if this
      file does not already have one, or refine the existing row with the
      real API once it is known precisely).
- [ ] Retain/reference API documented (or explicitly "none -- borrowed
      only", as PipeWire's `pw_buffer`/`spa_buffer` row already is).
- [ ] Release/destroy API documented.
- [ ] Synchronization owner documented (which side's API must be called
      to know the object is safe to reuse/destroy -- e.g. a fence/
      semaphore wait, not just "the function returned").
- [ ] CRT adapter code never calls `free()`/`malloc()`-family functions on
      the host object itself (only on CRT-owned wrapper structs that
      merely contain a handle to it).
- [ ] A real-host smoke test added, matching this project's own existing
      `presented=1`-style evidence discipline in `HISTORY.md` -- policy
      alone is not acceptance.
