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
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
/* Real backend hooks -- src/arch/windows/gpu_win32.c. Same real shape as
 * the Vulkan hooks above (gpu.c still owns all argument validation and
 * the crtgfx_gpu_device allocation/refcount/free; these only fill in or
 * tear down the real D3D12-specific fields declared above). */
crtgfx_result crtgfx_gpu_win32_query_capabilities(crtgfx_gpu_capabilities* out_caps);
crtgfx_result crtgfx_gpu_win32_device_create(uint32_t device_index, struct crtgfx_gpu_device* device);
void crtgfx_gpu_win32_device_destroy(struct crtgfx_gpu_device* device);
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
/* Real backend hooks -- src/arch/macos/gpu_metal.c. Same real shape as
 * the Vulkan/D3D12 hooks above (gpu.c still owns all argument validation
 * and the crtgfx_gpu_device allocation/refcount/free; these only fill in
 * or tear down the real Metal-specific fields declared above). */
crtgfx_result crtgfx_gpu_metal_query_capabilities(crtgfx_gpu_capabilities* out_caps);
crtgfx_result crtgfx_gpu_metal_device_create(uint32_t device_index, struct crtgfx_gpu_device* device);
void crtgfx_gpu_metal_device_destroy(struct crtgfx_gpu_device* device);
#endif
