/* crtgfx/gpu.h's own real, host-independent reference implementation --
 * see that header's own top comment for the full design reasoning
 * (why device/surface always, honestly report CRTGFX_ERROR_UNSUPPORTED
 * today, and why the fence gets a real, working implementation now).
 *
 * The common public wrapper/lifetime/dispatch implementation. It contains no
 * host GPU API calls or concrete GPU state. The temporary Metal adapter still
 * resolves its platform window handle here until its migration tranche;
 * Vulkan and D3D12 extraction live in their backend owners. */

#include "crtgfx/gpu.h"

#include "gpu_internal.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>

#if defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
/* Only for crtgfx_gpu_surface_create()'s own real macOS/Metal branch
 * below -- resolving a crtgfx_window's real live CAMetalLayer `id`
 * (window_cocoa.c, via crtgfx_cocoa_get_metal_layer()) and the shared
 * toplevel struct's own real width/height. Same reasoning as the Linux
 * branch above. */
#include "arch/macos/window_cocoa_gpu.h"
#include "wayland_weston_internal.h"
#endif

/* struct crtgfx_gpu_device/crtgfx_gpu_surface themselves now live in
 * gpu_internal.h, shared with src/arch/linux/gpu_vulkan.c (the real
 * Ganesh/Vulkan offscreen vertical slice, 2026-09-03, and the native-
 * Wayland-backend swapchain vertical slice, 2026-09-07) -- see that
 * header's own top comment for why. */

struct crtgfx_gpu_fence {
  pthread_mutex_t lock;
  pthread_cond_t cond;
  int signaled;
};

#if defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
static crtgfx_result crtgfx_gpu_legacy_surface_get_size(
    crtgfx_gpu_surface* surface, uint32_t* out_width, uint32_t* out_height) {
  *out_width = surface->width;
  *out_height = surface->height;
  return CRTGFX_OK;
}
#endif

#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)
static const struct crtgfx_gpu_backend_ops crtgfx_gpu_active_ops = {
    CRTGFX_GPU_BACKEND_VULKAN,
    crtgfx_gpu_vulkan_query_capabilities,
    crtgfx_gpu_vulkan_device_create,
    crtgfx_gpu_vulkan_device_destroy,
    crtgfx_gpu_vulkan_surface_create,
    crtgfx_gpu_vulkan_surface_destroy,
    crtgfx_gpu_vulkan_surface_get_size,
    crtgfx_gpu_vulkan_surface_acquire,
    crtgfx_gpu_vulkan_surface_clear,
    crtgfx_gpu_vulkan_surface_resize,
    crtgfx_gpu_vulkan_surface_present,
};
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
static const struct crtgfx_gpu_backend_ops crtgfx_gpu_active_ops = {
    CRTGFX_GPU_BACKEND_D3D12,
    crtgfx_gpu_win32_query_capabilities,
    crtgfx_gpu_win32_device_create,
    crtgfx_gpu_win32_device_destroy,
    crtgfx_gpu_win32_surface_create,
    crtgfx_gpu_win32_surface_destroy,
    crtgfx_gpu_win32_surface_get_size,
    crtgfx_gpu_win32_surface_acquire,
    crtgfx_gpu_win32_surface_clear,
    crtgfx_gpu_win32_surface_resize,
    crtgfx_gpu_win32_surface_present,
};
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
static crtgfx_result crtgfx_gpu_metal_surface_create_dispatch(
    crtgfx_gpu_device* device, crtgfx_window* window, crtgfx_gpu_surface* surface) {
  void* metal_layer_handle = NULL;
  if (crtgfx_cocoa_get_metal_layer(&window->toplevel, &metal_layer_handle) == 0) {
    return CRTGFX_ERROR_UNSUPPORTED;
  }
  return crtgfx_gpu_metal_surface_create(
      device, metal_layer_handle, window->toplevel.width, window->toplevel.height, surface);
}

static const struct crtgfx_gpu_backend_ops crtgfx_gpu_active_ops = {
    CRTGFX_GPU_BACKEND_METAL,
    crtgfx_gpu_metal_query_capabilities,
    crtgfx_gpu_metal_device_create,
    crtgfx_gpu_metal_device_destroy,
    crtgfx_gpu_metal_surface_create_dispatch,
    crtgfx_gpu_metal_surface_destroy,
    crtgfx_gpu_legacy_surface_get_size,
    crtgfx_gpu_metal_surface_acquire,
    crtgfx_gpu_metal_surface_clear,
    crtgfx_gpu_metal_surface_resize,
    crtgfx_gpu_metal_surface_present,
};
#endif

static const struct crtgfx_gpu_backend_ops* crtgfx_gpu_get_active_ops(void) {
#if (defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)) || \
    (defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)) || \
    (defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL))
  return &crtgfx_gpu_active_ops;
#else
  return NULL;
#endif
}

crtgfx_result crtgfx_gpu_query_capabilities(crtgfx_gpu_capabilities* out_caps) {
  const struct crtgfx_gpu_backend_ops* ops;
  if (out_caps == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  ops = crtgfx_gpu_get_active_ops();
  if (ops != NULL) {
    return ops->query_capabilities(out_caps);
  }
  out_caps->backend = CRTGFX_GPU_BACKEND_NONE;
  out_caps->device_count = 0;
  return CRTGFX_OK;
}

crtgfx_result crtgfx_gpu_device_create(uint32_t device_index, crtgfx_gpu_device** out_device) {
  const struct crtgfx_gpu_backend_ops* ops;
  crtgfx_gpu_device* device;
  crtgfx_result result;
  if (out_device == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  ops = crtgfx_gpu_get_active_ops();
  if (ops == NULL) return CRTGFX_ERROR_UNSUPPORTED;
  device = (crtgfx_gpu_device*)calloc(1, sizeof(crtgfx_gpu_device));
  if (device == NULL) return CRTGFX_ERROR_UNSUPPORTED;
  device->backend = ops->backend;
  device->ops = ops;
  result = ops->device_create(device_index, device);
  if (result != CRTGFX_OK) {
    free(device);
    return result;
  }
  atomic_init(&device->refcount, 1);
  *out_device = device;
  return CRTGFX_OK;
}

crtgfx_result crtgfx_gpu_device_retain(crtgfx_gpu_device* device) {
  if (device == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  atomic_fetch_add_explicit(&device->refcount, 1, memory_order_relaxed);
  return CRTGFX_OK;
}

void crtgfx_gpu_device_release(crtgfx_gpu_device* device) {
  if (device == NULL) {
    return;
  }
  /* atomic_fetch_sub_explicit() returns the value *before* the
   * subtraction -- 1 means this was the last real reference. */
  if (atomic_fetch_sub_explicit(&device->refcount, 1, memory_order_acq_rel) == 1) {
    device->ops->device_destroy(device);
    free(device);
  }
}

crtgfx_result crtgfx_gpu_surface_create(
    crtgfx_gpu_device* device, crtgfx_window* window, crtgfx_gpu_surface** out_surface) {
  crtgfx_gpu_surface* surface;
  crtgfx_result result;
  if (device == NULL || window == NULL || out_surface == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  surface = (crtgfx_gpu_surface*)calloc(1, sizeof(crtgfx_gpu_surface));
  if (surface == NULL) return CRTGFX_ERROR_UNSUPPORTED;
  surface->backend = device->backend;
  surface->ops = device->ops;
  result = surface->ops->surface_create(device, window, surface);
  if (result != CRTGFX_OK) {
    free(surface);
    return result;
  }
  *out_surface = surface;
  return CRTGFX_OK;
}

void crtgfx_gpu_surface_release(crtgfx_gpu_surface* surface) {
  if (surface == NULL) {
    return;
  }
  surface->ops->surface_destroy(surface);
  free(surface);
}

crtgfx_result crtgfx_gpu_surface_get_size(crtgfx_gpu_surface* surface, uint32_t* out_width, uint32_t* out_height) {
  if (surface == NULL || out_width == NULL || out_height == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  return surface->ops->surface_get_size(surface, out_width, out_height);
}

crtgfx_result crtgfx_gpu_surface_acquire(crtgfx_gpu_surface* surface, uint64_t timeout_us) {
  if (surface == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  return surface->ops->surface_acquire(surface, timeout_us);
}

crtgfx_result crtgfx_gpu_surface_clear(crtgfx_gpu_surface* surface, float r, float g, float b, float a) {
  if (surface == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  return surface->ops->surface_clear(surface, r, g, b, a);
}

crtgfx_result crtgfx_gpu_surface_resize(crtgfx_gpu_surface* surface, uint32_t width, uint32_t height) {
  if (surface == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  if (width == 0u || height == 0u) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  return surface->ops->surface_resize(surface, width, height);
}

crtgfx_result crtgfx_gpu_surface_present(crtgfx_gpu_surface* surface) {
  if (surface == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  return surface->ops->surface_present(surface);
}

crtgfx_result crtgfx_gpu_fence_create(crtgfx_gpu_device* device, crtgfx_gpu_fence** out_fence) {
  /* `device` is intentionally unused here: NULL is the only real,
   * reachable value today (see gpu.h's own top comment) -- a real
   * device-affine fence implementation is a future backend's own
   * addition, not this contract's own reference implementation's job. */
  (void)device;
  if (out_fence == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  crtgfx_gpu_fence* fence = (crtgfx_gpu_fence*)calloc(1, sizeof(crtgfx_gpu_fence));
  if (fence == NULL) {
    return CRTGFX_ERROR_UNSUPPORTED;
  }
  pthread_mutex_init(&fence->lock, NULL);
  /* A real, confirmed-for-real libc gap (2026-09-02) was found while
   * first building this file: pthread_condattr_setclock(PTHREAD_COND_
   * CLOCK_MONOTONIC) stored the requested clock but pthread_cond_
   * timedwait() (libc/src/pthread.c) never actually consulted it,
   * silently always treating `abstime` as CLOCK_REALTIME -- confirmed
   * directly via crtgfx_gpu_test's own real timing assertions on real
   * Windows hardware (a CLOCK_MONOTONIC-based deadline read back as
   * already far in CLOCK_REALTIME's own past, an instant, wrong
   * CRTGFX_ERROR_TIMEOUT every real time). That earlier version of this
   * function used the plain default clock (CLOCK_REALTIME, no condattr
   * at all) as a real, deliberate workaround. The underlying bug is now
   * fixed for real (2026-09-03, libc/src/pthread.c's own CRT_COND_CLOCK_
   * WORD -- pthread_cond_init()/pthread_cond_timedwait() now genuinely
   * honor a cond var's own configured clock; verified via tests/
   * pthread_cond_test.c's own new, real, elapsed-wall-time-measuring
   * PTHREAD_COND_CLOCK_MONOTONIC coverage), so this reverts to the
   * originally-intended, more principled CLOCK_MONOTONIC -- immune to a
   * real wall-clock adjustment mid-wait, matching this project's own
   * usual preference elsewhere (crtmedia/player.c, src/arch/linux/
   * audio_sink_linux.c). */
  pthread_condattr_t attr;
  pthread_condattr_init(&attr);
  pthread_condattr_setclock(&attr, PTHREAD_COND_CLOCK_MONOTONIC);
  pthread_cond_init(&fence->cond, &attr);
  pthread_condattr_destroy(&attr);
  fence->signaled = 0;
  *out_fence = fence;
  return CRTGFX_OK;
}

crtgfx_result crtgfx_gpu_fence_wait(crtgfx_gpu_fence* fence, uint64_t timeout_us) {
  if (fence == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  struct timespec deadline;
  /* CLOCK_MONOTONIC, matching crtgfx_gpu_fence_create()'s own cond var
   * (see that function's own comment above). */
  clock_gettime(CLOCK_MONOTONIC, &deadline);
  deadline.tv_sec += (time_t)(timeout_us / 1000000);
  deadline.tv_nsec += (long)((timeout_us % 1000000) * 1000);
  if (deadline.tv_nsec >= 1000000000L) {
    deadline.tv_nsec -= 1000000000L;
    deadline.tv_sec += 1;
  }

  crtgfx_result result = CRTGFX_OK;
  pthread_mutex_lock(&fence->lock);
  /* Real, level-triggered wait: a fence already signaled before this
   * call is entered returns CRTGFX_OK immediately (the while condition
   * is false on the very first check, matching crtgfx_gpu_fence_signal()'s
   * own documented "signal-before-wait still succeeds" contract) --
   * never even reaches pthread_cond_timedwait() in that real case. */
  while (!fence->signaled) {
    int wait_result = pthread_cond_timedwait(&fence->cond, &fence->lock, &deadline);
    if (wait_result == ETIMEDOUT) {
      result = CRTGFX_ERROR_TIMEOUT;
      break;
    }
    /* A real spurious wakeup (or an equivalent real interruption) --
     * re-check fence->signaled via the loop condition rather than
     * assuming a real signal actually happened. */
  }
  pthread_mutex_unlock(&fence->lock);
  return result;
}

crtgfx_result crtgfx_gpu_fence_signal(crtgfx_gpu_fence* fence) {
  if (fence == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  pthread_mutex_lock(&fence->lock);
  fence->signaled = 1;
  pthread_cond_broadcast(&fence->cond); /* wakes every real thread currently in crtgfx_gpu_fence_wait() */
  pthread_mutex_unlock(&fence->lock);
  return CRTGFX_OK;
}

void crtgfx_gpu_fence_release(crtgfx_gpu_fence* fence) {
  if (fence == NULL) {
    return;
  }
  pthread_cond_destroy(&fence->cond);
  pthread_mutex_destroy(&fence->lock);
  free(fence);
}
