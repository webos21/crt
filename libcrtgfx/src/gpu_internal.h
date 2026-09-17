#pragma once

/* Private, non-installed header shared between src/gpu.c (host-independent
 * dispatch + refcounting + the real CPU fence) and this project's real
 * per-host GPU backend implementations under src/arch/{linux,...}/gpu_*.c
 * -- mirrors wayland_weston_internal.h's own role for crtgfx_window.
 *
 * gpu.c owns each common wrapper's lifetime (calloc/atomic refcount/free),
 * while the selected backend owns the object behind backend_state. Feature
 * macros select code, never this common allocation layout. */

#include "crtgfx/gpu.h"

#include <stdatomic.h>
#include <stdint.h>

struct crtgfx_gpu_backend_ops {
  crtgfx_gpu_backend backend;
  crtgfx_result (*query_capabilities)(crtgfx_gpu_capabilities* out_caps);
  crtgfx_result (*device_create)(uint32_t device_index, struct crtgfx_gpu_device* device);
  void (*device_destroy)(struct crtgfx_gpu_device* device);
  crtgfx_result (*surface_create)(
      struct crtgfx_gpu_device* device, crtgfx_window* window, struct crtgfx_gpu_surface* surface);
  void (*surface_destroy)(struct crtgfx_gpu_surface* surface);
  crtgfx_result (*surface_get_size)(
      struct crtgfx_gpu_surface* surface, uint32_t* out_width, uint32_t* out_height);
  crtgfx_result (*surface_acquire)(struct crtgfx_gpu_surface* surface, uint64_t timeout_us);
  crtgfx_result (*surface_clear)(
      struct crtgfx_gpu_surface* surface, float r, float g, float b, float a);
  crtgfx_result (*surface_resize)(struct crtgfx_gpu_surface* surface, uint32_t width, uint32_t height);
  crtgfx_result (*surface_present)(struct crtgfx_gpu_surface* surface);
};

struct crtgfx_gpu_device {
  atomic_int refcount;
  crtgfx_gpu_backend backend;
  const struct crtgfx_gpu_backend_ops* ops;
  void* backend_state;
};

/* A real GPU-backed presentation target -- gpu.c owns the allocation/
 * free the same way it owns crtgfx_gpu_device's, while a real backend fills
 * in and tears down its host-specific fields. Unlike crtgfx_gpu_device there
 * is no refcount: the public surface remains single-owner. During migration
 * the remaining legacy backend groups are unconditional for the same fixed-layout
 * reason as the device groups above. */
struct crtgfx_gpu_surface {
  crtgfx_gpu_backend backend;
  const struct crtgfx_gpu_backend_ops* ops;
  void* backend_state;
};

/* Every hook below is defined in a plain .c backend file (real C linkage),
 * but this header is also included directly by src/skia_bridge.cc (a .cc
 * TU) -- confirmed load-bearing for real, not just tidiness: crtgfx_gpu_
 * metal_surface_prepare_ganesh_present() is the first of these hooks
 * skia_bridge.cc actually *calls* rather than only accessing struct
 * fields of (Vulkan and D3D12 now use borrowed views), and
 * without this guard a C++ compile mangles this declaration while gpu_
 * metal.c's own C definition stays unmangled, so real macOS hardware
 * linking skia_bridge.cc against gpu_metal.c failed with a genuine
 * "symbol(s) not found" -- caught only once both were actually built and
 * linked together on real macOS hardware for the first time (2026-09-08),
 * matching crtgfx/gpu.h's own already-established extern "C" convention
 * for the identical reason. */
#ifdef __cplusplus
extern "C" {
#endif

#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)
struct crtgfx_gpu_vulkan_device_view {
  void* instance;
  void* physical_device;
  void* device;
  void* queue;
  uint32_t queue_family_index;
};

struct crtgfx_gpu_vulkan_surface_view {
  void* image;
  uint32_t format;
  uint32_t image_usage_flags;
  void* image_available_semaphore;
  void* render_finished_semaphore;
  uint32_t queue_family_index;
  uint32_t width;
  uint32_t height;
};

/* Real backend hooks -- src/arch/linux/gpu_vulkan.c. Mirror crtgfx_gpu_
 * query_capabilities()/crtgfx_gpu_device_create()'s own public contract in
 * spirit, but gpu.c itself still does every real argument validation and
 * owns the crtgfx_gpu_device allocation/refcount/free directly -- these
 * hooks create, publish, use, and tear down the opaque Vulkan state on an
 * already-validated, already-allocated wrapper. */
crtgfx_result crtgfx_gpu_vulkan_query_capabilities(crtgfx_gpu_capabilities* out_caps);
crtgfx_result crtgfx_gpu_vulkan_device_create(uint32_t device_index, struct crtgfx_gpu_device* device);
void crtgfx_gpu_vulkan_device_destroy(struct crtgfx_gpu_device* device);
/* Real surface/swapchain hooks (2026-09-07) -- gpu.c validates and allocates,
 * while the Vulkan owner resolves the opaque public window to its real native
 * Wayland handles and creates/tears down the private surface state. */
crtgfx_result crtgfx_gpu_vulkan_surface_create(
    struct crtgfx_gpu_device* device, crtgfx_window* window, struct crtgfx_gpu_surface* surface);
void crtgfx_gpu_vulkan_surface_destroy(struct crtgfx_gpu_surface* surface);
crtgfx_result crtgfx_gpu_vulkan_surface_acquire(struct crtgfx_gpu_surface* surface, uint64_t timeout_us);
crtgfx_result crtgfx_gpu_vulkan_surface_clear(struct crtgfx_gpu_surface* surface, float r, float g, float b, float a);
crtgfx_result crtgfx_gpu_vulkan_surface_present(struct crtgfx_gpu_surface* surface);
/* Real swapchain recreation (2026-09-07) -- see crtgfx/gpu.h's own
 * crtgfx_gpu_surface_resize() comment for the full contract. */
crtgfx_result crtgfx_gpu_vulkan_surface_resize(struct crtgfx_gpu_surface* surface, uint32_t width, uint32_t height);
crtgfx_result crtgfx_gpu_vulkan_surface_get_size(
    struct crtgfx_gpu_surface* surface, uint32_t* out_width, uint32_t* out_height);
int crtgfx_gpu_vulkan_borrow_device(
    const struct crtgfx_gpu_device* device, struct crtgfx_gpu_vulkan_device_view* out_view);
int crtgfx_gpu_vulkan_begin_ganesh(
    struct crtgfx_gpu_surface* surface, struct crtgfx_gpu_vulkan_surface_view* out_view);
int crtgfx_gpu_vulkan_get_ganesh_present_view(
    struct crtgfx_gpu_surface* surface, struct crtgfx_gpu_vulkan_surface_view* out_view);
void crtgfx_gpu_vulkan_end_ganesh(struct crtgfx_gpu_surface* surface);
void crtgfx_gpu_vulkan_test_force_device_loss(struct crtgfx_gpu_device* device);
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
struct crtgfx_gpu_win32_device_view {
  void* adapter;
  void* device;
  void* command_queue;
};

struct crtgfx_gpu_win32_surface_view {
  void* back_buffer;
  uint32_t width;
  uint32_t height;
};

/* Real backend hooks -- src/arch/windows/gpu_win32.c. Same real shape as
 * the Vulkan hooks above (gpu.c still owns all argument validation and
 * the crtgfx_gpu_device allocation/refcount/free; these only fill in or
 * tear down the real D3D12-specific fields declared above). */
crtgfx_result crtgfx_gpu_win32_query_capabilities(crtgfx_gpu_capabilities* out_caps);
crtgfx_result crtgfx_gpu_win32_device_create(uint32_t device_index, struct crtgfx_gpu_device* device);
void crtgfx_gpu_win32_device_destroy(struct crtgfx_gpu_device* device);
/* Real surface/swap-chain hooks. The Windows owner resolves the opaque public
 * window to its HWND and owns the private D3D12 surface state. */
crtgfx_result crtgfx_gpu_win32_surface_create(
    struct crtgfx_gpu_device* device, crtgfx_window* window, struct crtgfx_gpu_surface* surface);
void crtgfx_gpu_win32_surface_destroy(struct crtgfx_gpu_surface* surface);
crtgfx_result crtgfx_gpu_win32_surface_get_size(
    struct crtgfx_gpu_surface* surface, uint32_t* out_width, uint32_t* out_height);
crtgfx_result crtgfx_gpu_win32_surface_acquire(struct crtgfx_gpu_surface* surface, uint64_t timeout_us);
crtgfx_result crtgfx_gpu_win32_surface_clear(struct crtgfx_gpu_surface* surface, float r, float g, float b, float a);
crtgfx_result crtgfx_gpu_win32_surface_present(struct crtgfx_gpu_surface* surface);
/* Real swap-chain recreation (2026-09-07) -- see crtgfx/gpu.h's own
 * crtgfx_gpu_surface_resize() comment for the full contract. */
crtgfx_result crtgfx_gpu_win32_surface_resize(struct crtgfx_gpu_surface* surface, uint32_t width, uint32_t height);
int crtgfx_gpu_win32_borrow_device(
    const struct crtgfx_gpu_device* device, struct crtgfx_gpu_win32_device_view* out_view);
int crtgfx_gpu_win32_begin_ganesh(
    struct crtgfx_gpu_surface* surface, struct crtgfx_gpu_win32_surface_view* out_view);
int crtgfx_gpu_win32_is_ganesh_wrapped(const struct crtgfx_gpu_surface* surface);
void crtgfx_gpu_win32_end_ganesh(struct crtgfx_gpu_surface* surface);
crtgfx_result crtgfx_gpu_win32_submit_ganesh(
    struct crtgfx_gpu_surface* surface, void* resource, uint32_t resource_state_before);
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
struct crtgfx_gpu_metal_device_view {
  void* device;
  void* command_queue;
};

struct crtgfx_gpu_metal_surface_view {
  void* texture;
  uint32_t width;
  uint32_t height;
};

/* Real backend hooks -- src/arch/macos/gpu_metal.c. Same real shape as
 * the Vulkan/D3D12 hooks above (gpu.c still owns all argument validation
 * and the crtgfx_gpu_device allocation/refcount/free; these only fill in
 * or tear down the real Metal-specific fields declared above). */
crtgfx_result crtgfx_gpu_metal_query_capabilities(crtgfx_gpu_capabilities* out_caps);
crtgfx_result crtgfx_gpu_metal_device_create(uint32_t device_index, struct crtgfx_gpu_device* device);
void crtgfx_gpu_metal_device_destroy(struct crtgfx_gpu_device* device);
/* The Metal owner resolves the opaque public window to its CAMetalLayer and
 * owns the private drawable state. */
crtgfx_result crtgfx_gpu_metal_surface_create(
    struct crtgfx_gpu_device* device, crtgfx_window* window, struct crtgfx_gpu_surface* surface);
void crtgfx_gpu_metal_surface_destroy(struct crtgfx_gpu_surface* surface);
crtgfx_result crtgfx_gpu_metal_surface_get_size(
    struct crtgfx_gpu_surface* surface, uint32_t* out_width, uint32_t* out_height);
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
 * buffer from the owner-local device queue into the owner-local surface
 * state, mirroring exactly what
 * crtgfx_gpu_metal_surface_clear() already does at its own last step --
 * so the existing, unchanged crtgfx_gpu_metal_surface_present() can then
 * present it. Kept in gpu_metal.c rather than skia_bridge.cc's own Metal
 * branch (unlike the Vulkan/D3D12 siblings, which do this inline using
 * real host headers already force-included there) because gpu_metal.c
 * already owns 100% of this project's real Objective-C/Metal ABI
 * knowledge and skia_bridge.cc's own Metal branch hand-declares none of
 * it -- keeping that knowledge centralized here matches this project's
 * own established per-backend ownership boundary. Returns CRTGFX_ERROR_
 * HOST if no drawable is acquired, the frame was not handed to Ganesh,
 * or the command buffer cannot be created. */
int crtgfx_gpu_metal_borrow_device(
    const struct crtgfx_gpu_device* device, struct crtgfx_gpu_metal_device_view* out_view);
int crtgfx_gpu_metal_begin_ganesh(
    struct crtgfx_gpu_surface* surface, struct crtgfx_gpu_metal_surface_view* out_view);
int crtgfx_gpu_metal_is_ganesh_wrapped(const struct crtgfx_gpu_surface* surface);
void crtgfx_gpu_metal_end_ganesh(struct crtgfx_gpu_surface* surface);
crtgfx_result crtgfx_gpu_metal_surface_prepare_ganesh_present(struct crtgfx_gpu_surface* surface);
void crtgfx_gpu_metal_test_force_device_loss(struct crtgfx_gpu_device* device);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif
