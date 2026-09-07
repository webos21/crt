#pragma once

/* Private, non-installed header shared between src/gpu.c (host-independent
 * dispatch + refcounting + the real CPU fence) and this project's real
 * per-host GPU backend implementations under src/arch/{linux,...}/gpu_*.c
 * -- mirrors wayland_weston_internal.h's own role for crtgfx_window.
 *
 * crtgfx_gpu_device's own concrete fields live here, not in gpu.c alone,
 * because a real backend (today: src/arch/linux/gpu_vulkan.c, the Ganesh/
 * Vulkan offscreen vertical slice landed 2026-09-03 -- see gpu.h's own top
 * comment and libcrtgfx/README.md) needs to both fill them in at real
 * device-creation time and read them back at release time to tear the real
 * device down. gpu.c owns the struct's own lifetime (calloc/atomic
 * refcount/free); a backend only owns what is inside it once its own
 * CRTGFX_HAVE_* compile-time gate is actually defined (CMakeLists.txt only
 * defines CRTGFX_HAVE_VULKAN when a real libvulkan was actually found to
 * link against -- absent that, this struct carries no Linux-specific
 * fields at all and gpu.c's own dispatch never calls into this backend). */

#include "crtgfx/gpu.h"

#include <stdatomic.h>
#include <stdint.h>

struct crtgfx_gpu_device {
  atomic_int refcount;
#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)
  /* Real Vulkan handles. VkInstance/VkPhysicalDevice/VkDevice/VkQueue are
   * all opaque, pointer-sized "dispatchable handle" types per the Vulkan
   * spec's own VK_DEFINE_HANDLE macro -- void* here avoids naming any real
   * Vulkan type in a header gpu.c itself also includes, matching this
   * project's own "no host SDK type where this project hasn't already
   * committed to one" habit even in a private, non-installed header. Real
   * ownership (fill-in at create, teardown at release) lives in
   * src/arch/linux/gpu_vulkan.c, not here. */
  void* vk_instance;
  void* vk_physical_device;
  void* vk_device;
  void* vk_queue;
  uint32_t vk_queue_family_index;
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
  /* Real D3D12/DXGI handles (2026-09-03, the Windows/D3D12 offscreen
   * vertical slice -- the Linux/Vulkan slice's own sibling). The real
   * ID3D12Device, ID3D12CommandQueue, and IDXGIAdapter1 COM interfaces
   * are all real, pointer-sized COM object pointers -- void* here for
   * the identical reason the Vulkan fields above use it:
   * real COM pointers are ABI-identical regardless of which header
   * declared the type name (this header never includes the real
   * <d3d12.h>/<dxgi1_4.h> Skia's own consumer code, skia_bridge.cc,
   * separately does -- gpu_win32.c itself stays real-host-header-free,
   * hand-declaring only what it needs, matching window_win32.c's own
   * established D3D11 convention). Real ownership (fill-in at create,
   * teardown at release) lives in src/arch/windows/gpu_win32.c, not here.
   */
  void* d3d12_device;
  void* d3d12_command_queue;
  void* dxgi_adapter;
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
  /* Real Metal handles (2026-09-04, the Windows/D3D12 and Linux/Vulkan
   * offscreen vertical slices' own macOS sibling). `id<MTLDevice>`/
   * `id<MTLCommandQueue>` are real, opaque Objective-C object pointers --
   * void* here for the identical reason the Vulkan/D3D12 fields above
   * use it: an Objective-C `id` is ABI-identical regardless of which
   * header (or no header at all) declared the type name. This header
   * never includes the real <Metal/Metal.h> Skia's own consumer code,
   * skia_bridge.cc, separately does (via GrMtlBackendContext.h) --
   * src/arch/macos/gpu_metal.c itself stays real-host-header-free,
   * driving the Objective-C runtime directly via objc_msgSend, matching
   * window_cocoa.c's own established convention. Real ownership (fill-in
   * at create, teardown at release) lives in src/arch/macos/gpu_metal.c,
   * not here. */
  void* mtl_device;
  void* mtl_command_queue;
#endif
};

/* A real GPU-backed presentation target -- gpu.c owns the allocation/
 * free the same way it owns crtgfx_gpu_device's, a real backend (today:
 * src/arch/linux/gpu_vulkan.c, 2026-09-07) only fills in and tears down
 * its own host-specific fields. Unlike crtgfx_gpu_device, there is no
 * host-independent field at all here (no refcount -- crtgfx_gpu_surface
 * is single-owner, see crtgfx/gpu.h's own comment on crtgfx_gpu_surface_
 * create()) -- every field is behind the same real per-host #if/#elif
 * chain crtgfx_gpu_device's own Vulkan/D3D12/Metal fields already use. */
struct crtgfx_gpu_surface {
#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)
  /* Not retained -- a surface's own lifetime is documented (crtgfx/gpu.h)
   * to never outlive the device it was created against; this is a plain
   * back-reference for crtgfx_gpu_vulkan_surface_*() to reach the real
   * VkInstance/VkPhysicalDevice/VkDevice/VkQueue it needs, not a second
   * ownership relationship. */
  crtgfx_gpu_device* device;
  /* Real VkSurfaceKHR/VkSwapchainKHR -- non-dispatchable handles, void*
   * for the same reason crtgfx_gpu_device's own Vulkan fields above are:
   * pointer-sized and ABI-identical regardless of which header declared
   * the type name, on this project's 64-bit-only real targets. */
  void* vk_surface;
  void* vk_swapchain;
  /* malloc'd array of `vk_image_count` real VkImage handles -- each one
   * owned by vk_swapchain itself (never individually destroyed, per the
   * real Vulkan spec's own VkSwapchainKHR contract); only the array
   * storage itself is this struct's own to free. */
  void** vk_images;
  uint32_t vk_image_count;
  uint32_t vk_format; /* real VkFormat the swapchain was created with */
  uint32_t width;
  uint32_t height;
  /* Real per-frame synchronization: one semaphore each side of a real
   * vkCmdClearColorImage() submission (signaled by acquire, waited-on by
   * the submit; signaled by the submit, waited-on by present), and one
   * fence gating command-buffer reuse (signaled once the GPU has actually
   * finished the previous frame's submission) -- the same minimal, real,
   * single-frame-in-flight synchronization shape any introductory real
   * Vulkan swapchain sample uses; a future multi-frame-in-flight
   * improvement is real, later work, not attempted in this first vertical
   * slice. */
  void* vk_image_available_semaphore;
  void* vk_render_finished_semaphore;
  void* vk_command_pool;
  void* vk_command_buffer;
  void* vk_frame_fence;
  uint32_t vk_current_image_index;
  /* 1 between a successful crtgfx_gpu_vulkan_surface_acquire() and the
   * matching crtgfx_gpu_vulkan_surface_present() -- lets _clear()/
   * _present() reject being called out of order with a real, honest
   * CRTGFX_ERROR_HOST rather than passing a stale/undefined image index
   * to the driver. */
  int vk_image_acquired;
  /* 1 between a successful crtgfx_skia_wrap_gpu_surface() (crtgfx/skia.h,
   * 2026-09-07 -- wiring the offscreen Ganesh pipeline onto this surface's
   * own acquired image) and the matching crtgfx_skia_gpu_surface_present()
   * -- lets crtgfx_gpu_surface_clear()/the plain crtgfx_gpu_surface_
   * present() reject being called directly on a Ganesh-wrapped frame
   * (mixing the two real presentation paths mid-frame is real, rejected
   * misuse, not a silent corruption), matching every other out-of-order
   * guard in this struct. Reset to 0 by _surface_acquire(); cleared by
   * crtgfx_skia_gpu_surface_present() itself just before it defers to the
   * plain crtgfx_gpu_surface_present() at its own last step. */
  int ganesh_wrapped;
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
  /* Real D3D12/DXGI swap-chain presentation (2026-09-07, the Windows leg
   * of "Finish live GPU presentation everywhere", following the Linux
   * native-Wayland-backend vertical slice) -- same real per-field shape
   * as the Vulkan branch above, adapted to D3D12's own real conventions
   * (an explicit RTV descriptor heap instead of Vulkan's plain VkImage
   * array; per-back-buffer fence *values* against one shared ID3D12Fence,
   * D3D12's own real synchronization primitive, rather than Vulkan's
   * separate per-frame VkFence objects). Not retained, same reasoning as
   * the Vulkan branch's own `device` field. */
  crtgfx_gpu_device* device;
  /* Real IDXGISwapChain3* -- void* for the same "no host SDK type in a
   * shared header" reason every other Windows/Vulkan handle in this file
   * already uses. */
  void* dxgi_swapchain;
  /* malloc'd array of `d3d12_buffer_count` real ID3D12Resource* back
   * buffers (GetBuffer adds references, individually released during
   * surface teardown) and one RTV descriptor heap (a single, small,
   * CPU-visible heap sized to the same buffer count) so crtgfx_gpu_
   * win32_surface_clear() can create/reuse each buffer's own render
   * target view without a new heap per frame. */
  void** d3d12_back_buffers;
  void* d3d12_rtv_heap;
  size_t d3d12_rtv_descriptor_size;
  uint32_t d3d12_buffer_count;
  uint32_t width;
  uint32_t height;
  /* One command allocator + one command list, reset and re-recorded each
   * frame (D3D12's own real, standard single-buffered command-recording
   * shape -- matches the Vulkan branch's own single vk_command_buffer). */
  void* d3d12_command_allocator;
  void* d3d12_command_list;
  /* Monotonic submit fence. Per-image values record submissions, but
   * acquire waits for the latest value because all images share the
   * same command allocator/list. */
  void* d3d12_fence;
  uint64_t* d3d12_fence_values;
  uint64_t d3d12_fence_next_value;
  void* d3d12_fence_event;
  uint32_t d3d12_current_buffer_index;
  /* Same real out-of-order-call guard as the Vulkan branch's own
   * vk_image_acquired. */
  int d3d12_image_acquired;
  int d3d12_frame_submitted;
  /* Same real Ganesh-wrap-in-progress guard as the Vulkan branch's own
   * ganesh_wrapped field above -- see that field's own comment. */
  int ganesh_wrapped;
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
  /* Real CAMetalLayer drawable presentation (2026-09-07, the macOS leg of
   * "Finish live GPU presentation everywhere"). Simpler than the Vulkan/
   * D3D12 branches above -- Metal's own `-presentDrawable:`/command-buffer
   * completion handles GPU/CPU synchronization internally, so no explicit
   * fence/semaphore fields are needed here at all, matching gpu_metal.c's
   * own established "Metal's real ABI is Objective-C messages, no hand-
   * declared vtable/struct layout needed" precedent. Every `id`/`void*`
   * field is a real, opaque Objective-C object pointer, ABI-identical
   * regardless of which header (or none) declared the type name, same
   * reasoning as crtgfx_gpu_device's own mtl_device/mtl_command_queue
   * fields. Not retained beyond this surface's own real Cocoa retain/
   * release discipline (crtgfx_gpu_metal_surface_create()'s own comment). */
  crtgfx_gpu_device* device;
  /* CAMetalLayer retained by this surface and the window's content view. */
  void* mtl_layer;
  uint32_t width;
  uint32_t height;
  /* Held between a successful crtgfx_gpu_metal_surface_acquire() and the
   * matching crtgfx_gpu_metal_surface_present() -- id<CAMetalDrawable>,
   * id<MTLTexture>, and the id<MTLCommandBuffer> _clear() records into
   * and _present() commits. */
  void* mtl_drawable;
  void* mtl_drawable_texture;
  void* mtl_command_buffer;
  void* mtl_last_submission;
  /* Same real out-of-order-call guard as the Vulkan/D3D12 branches' own
   * acquired flags. */
  int mtl_drawable_acquired;
  /* Same real Ganesh-wrap-in-progress guard as the Vulkan branch's own
   * ganesh_wrapped field above -- see that field's own comment. */
  int ganesh_wrapped;
#else
  int reserved;
#endif
};

#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)
/* Real backend hooks -- src/arch/linux/gpu_vulkan.c. Mirror crtgfx_gpu_
 * query_capabilities()/crtgfx_gpu_device_create()'s own public contract in
 * spirit, but gpu.c itself still does every real argument validation and
 * owns the crtgfx_gpu_device allocation/refcount/free directly -- these
 * hooks only ever fill in (or tear down) the real Vulkan-specific fields
 * declared above, on an already-validated, already-allocated device. */
crtgfx_result crtgfx_gpu_vulkan_query_capabilities(crtgfx_gpu_capabilities* out_caps);
crtgfx_result crtgfx_gpu_vulkan_device_create(uint32_t device_index, struct crtgfx_gpu_device* device);
void crtgfx_gpu_vulkan_device_destroy(struct crtgfx_gpu_device* device);
/* Real surface/swapchain hooks (2026-09-07) -- same real "gpu.c validates
 * and allocates, this only fills in/tears down its own host-specific
 * fields" shape as the device hooks just above. `wl_display`/`wl_surface`
 * are real, live `struct wl_display*`/`struct wl_surface*` from the
 * native Wayland backend (src/arch/linux/window_wayland_native.c, via
 * crtgfx_native_wl_get_surface_handles() -- gpu.c's own caller resolves
 * this, not this header, to keep this header buildable without ever
 * needing <wayland-client.h> itself); void* here for the same "no host
 * SDK type leaked into a shared header" reason every other Vulkan handle
 * in this file already uses. */
crtgfx_result crtgfx_gpu_vulkan_surface_create(
    struct crtgfx_gpu_device* device, void* wl_display, void* wl_surface, uint32_t width, uint32_t height,
    struct crtgfx_gpu_surface* surface);
void crtgfx_gpu_vulkan_surface_destroy(struct crtgfx_gpu_surface* surface);
crtgfx_result crtgfx_gpu_vulkan_surface_acquire(struct crtgfx_gpu_surface* surface, uint64_t timeout_us);
crtgfx_result crtgfx_gpu_vulkan_surface_clear(struct crtgfx_gpu_surface* surface, float r, float g, float b, float a);
crtgfx_result crtgfx_gpu_vulkan_surface_present(struct crtgfx_gpu_surface* surface);
/* Real swapchain recreation (2026-09-07) -- see crtgfx/gpu.h's own
 * crtgfx_gpu_surface_resize() comment for the full contract. */
crtgfx_result crtgfx_gpu_vulkan_surface_resize(struct crtgfx_gpu_surface* surface, uint32_t width, uint32_t height);
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
/* Real backend hooks -- src/arch/windows/gpu_win32.c. Same real shape as
 * the Vulkan hooks above (gpu.c still owns all argument validation and
 * the crtgfx_gpu_device allocation/refcount/free; these only fill in or
 * tear down the real D3D12-specific fields declared above). */
crtgfx_result crtgfx_gpu_win32_query_capabilities(crtgfx_gpu_capabilities* out_caps);
crtgfx_result crtgfx_gpu_win32_device_create(uint32_t device_index, struct crtgfx_gpu_device* device);
void crtgfx_gpu_win32_device_destroy(struct crtgfx_gpu_device* device);
/* Real surface/swap-chain hooks (2026-09-07) -- same real shape as the
 * Vulkan surface hooks above. `hwnd` is a real, live HWND from window_
 * win32.c (via crtgfx_win32_get_hwnd() -- window_win32_gpu.h); gpu.c's
 * own caller resolves this, not this header, matching the Vulkan branch's
 * own wl_display/wl_surface resolution split. */
crtgfx_result crtgfx_gpu_win32_surface_create(
    struct crtgfx_gpu_device* device, void* hwnd, uint32_t width, uint32_t height,
    struct crtgfx_gpu_surface* surface);
void crtgfx_gpu_win32_surface_destroy(struct crtgfx_gpu_surface* surface);
crtgfx_result crtgfx_gpu_win32_surface_acquire(struct crtgfx_gpu_surface* surface, uint64_t timeout_us);
crtgfx_result crtgfx_gpu_win32_surface_clear(struct crtgfx_gpu_surface* surface, float r, float g, float b, float a);
crtgfx_result crtgfx_gpu_win32_surface_present(struct crtgfx_gpu_surface* surface);
/* Real swap-chain recreation (2026-09-07) -- see crtgfx/gpu.h's own
 * crtgfx_gpu_surface_resize() comment for the full contract. */
crtgfx_result crtgfx_gpu_win32_surface_resize(struct crtgfx_gpu_surface* surface, uint32_t width, uint32_t height);
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
/* Real backend hooks -- src/arch/macos/gpu_metal.c. Same real shape as
 * the Vulkan/D3D12 hooks above (gpu.c still owns all argument validation
 * and the crtgfx_gpu_device allocation/refcount/free; these only fill in
 * or tear down the real Metal-specific fields declared above). */
crtgfx_result crtgfx_gpu_metal_query_capabilities(crtgfx_gpu_capabilities* out_caps);
crtgfx_result crtgfx_gpu_metal_device_create(uint32_t device_index, struct crtgfx_gpu_device* device);
void crtgfx_gpu_metal_device_destroy(struct crtgfx_gpu_device* device);
/* Real surface/drawable hooks (2026-09-07) -- same real shape as the
 * Vulkan/D3D12 surface hooks above. `mtl_layer` is a real, live
 * CAMetalLayer `id` from window_cocoa.c (via crtgfx_cocoa_get_metal_
 * layer() -- window_cocoa_gpu.h); gpu.c's own caller resolves this, not
 * this header, matching the other two platforms' own split. */
crtgfx_result crtgfx_gpu_metal_surface_create(
    struct crtgfx_gpu_device* device, void* mtl_layer, uint32_t width, uint32_t height,
    struct crtgfx_gpu_surface* surface);
void crtgfx_gpu_metal_surface_destroy(struct crtgfx_gpu_surface* surface);
crtgfx_result crtgfx_gpu_metal_surface_acquire(struct crtgfx_gpu_surface* surface, uint64_t timeout_us);
crtgfx_result crtgfx_gpu_metal_surface_clear(struct crtgfx_gpu_surface* surface, float r, float g, float b, float a);
crtgfx_result crtgfx_gpu_metal_surface_present(struct crtgfx_gpu_surface* surface);
/* Real drawable-size update (2026-09-07) -- see crtgfx/gpu.h's own
 * crtgfx_gpu_surface_resize() comment for the full contract. */
crtgfx_result crtgfx_gpu_metal_surface_resize(struct crtgfx_gpu_surface* surface, uint32_t width, uint32_t height);
/* Real hook for crtgfx_skia_gpu_surface_present() (crtgfx/skia.h,
 * src/skia_bridge.cc, 2026-09-07 -- wiring the offscreen Ganesh pipeline
 * onto this surface's own acquired image). Metal has no image-layout/
 * resource-state concept a caller must transition before presenting
 * (unlike the Vulkan/D3D12 siblings), so the only real per-host work
 * needed after Ganesh's own flush+submit is handing a fresh command
 * buffer (from the same shared crtgfx_gpu_device::mtl_command_queue) into
 * this surface's own mtl_command_buffer field, mirroring exactly what
 * crtgfx_gpu_metal_surface_clear() already does at its own last step --
 * so the existing, unchanged crtgfx_gpu_metal_surface_present() can then
 * present it. Kept in gpu_metal.c rather than skia_bridge.cc's own Metal
 * branch (unlike the Vulkan/D3D12 siblings, which do this inline using
 * real host headers already force-included there) because gpu_metal.c
 * already owns 100% of this project's real Objective-C/Metal ABI
 * knowledge and skia_bridge.cc's own Metal branch hand-declares none of
 * it -- keeping that knowledge centralized here matches this project's
 * own established per-backend ownership boundary. Returns CRTGFX_ERROR_
 * HOST if surface->mtl_drawable_acquired is false or the command buffer
 * cannot be created. */
crtgfx_result crtgfx_gpu_metal_surface_prepare_ganesh_present(struct crtgfx_gpu_surface* surface);
#endif
