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
#endif
