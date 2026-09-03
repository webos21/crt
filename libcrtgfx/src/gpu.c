/* crtgfx/gpu.h's own real, host-independent reference implementation --
 * see that header's own top comment for the full design reasoning
 * (why device/surface always, honestly report CRTGFX_ERROR_UNSUPPORTED
 * today, and why the fence gets a real, working implementation now).
 *
 * A plain, host-independent source file -- unlike libcrtgfx's own
 * per-host window backends (src/arch/windows/window_win32.c and
 * friends), this compiles identically on every host via CRTGFX_COMMON_
 * SOURCES (libcrtgfx/CMakeLists.txt), using this project's own libc
 * headers normally (no host-SDK-avoidance concern here at all -- there
 * is no host GPU API call anywhere in this file yet). */

#include "crtgfx/gpu.h"

#include "gpu_internal.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>

/* struct crtgfx_gpu_device itself now lives in gpu_internal.h, shared with
 * src/arch/linux/gpu_vulkan.c (the real Ganesh/Vulkan offscreen vertical
 * slice, 2026-09-03) -- see that header's own top comment for why. */

/* No real fields yet -- crtgfx_gpu_surface_create() never actually
 * constructs one today (see gpu.h's own top comment: unreachable until a
 * real crtgfx_gpu_device exists). Declared as a real, if currently
 * unused, struct rather than left fully opaque with no definition at
 * all, so crtgfx_gpu_surface_release()'s own real free() below has a
 * real, correctly-sized object to release once a future backend does
 * start allocating one. */
struct crtgfx_gpu_surface {
  int reserved;
};

struct crtgfx_gpu_fence {
  pthread_mutex_t lock;
  pthread_cond_t cond;
  int signaled;
};

crtgfx_result crtgfx_gpu_query_capabilities(crtgfx_gpu_capabilities* out_caps) {
  if (out_caps == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)
  /* Real backend, Linux only (src/arch/linux/gpu_vulkan.c) -- see that
   * file's own top comment. Windows/macOS fall through to the honest
   * NONE/0 report below, same as Linux did before this landed. */
  return crtgfx_gpu_vulkan_query_capabilities(out_caps);
#else
  /* Real, honest report -- see this file's own top comment and gpu.h's
   * own top comment for why this is CRTGFX_GPU_BACKEND_NONE/0 on every
   * real code path on this host today, not a placeholder. */
  out_caps->backend = CRTGFX_GPU_BACKEND_NONE;
  out_caps->device_count = 0;
  return CRTGFX_OK;
#endif
}

crtgfx_result crtgfx_gpu_device_create(uint32_t device_index, crtgfx_gpu_device** out_device) {
  if (out_device == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)
  {
    crtgfx_gpu_device* device = (crtgfx_gpu_device*)calloc(1, sizeof(crtgfx_gpu_device));
    crtgfx_result result;
    if (device == NULL) {
      return CRTGFX_ERROR_UNSUPPORTED;
    }
    result = crtgfx_gpu_vulkan_device_create(device_index, device);
    if (result != CRTGFX_OK) {
      free(device);
      return result;
    }
    atomic_init(&device->refcount, 1);
    *out_device = device;
    return CRTGFX_OK;
  }
#else
  (void)device_index;
  /* crtgfx_gpu_query_capabilities() reports 0 real devices on this host
   * today (no real backend exists here yet -- see this file's own top
   * comment), so `device_index` is never in a real valid range and there
   * is nothing real to construct -- matching crtgfx_window_create()'s
   * own established "no usable host backend right now" contract, not a
   * crash/hang. */
  return CRTGFX_ERROR_UNSUPPORTED;
#endif
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
#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)
    crtgfx_gpu_vulkan_device_destroy(device);
#endif
    free(device);
  }
}

crtgfx_result crtgfx_gpu_surface_create(
    crtgfx_gpu_device* device, crtgfx_window* window, crtgfx_gpu_surface** out_surface) {
  if (device == NULL || window == NULL || out_surface == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  /* Real, structurally unreachable today: crtgfx_gpu_device_create()
   * never succeeds, so no real caller can ever obtain a real `device` to
   * pass here at all (see gpu.h's own top comment) -- this argument
   * validation is the only part of this function any real caller can
   * exercise until a future backend lands. */
  return CRTGFX_ERROR_UNSUPPORTED;
}

void crtgfx_gpu_surface_release(crtgfx_gpu_surface* surface) {
  if (surface == NULL) {
    return;
  }
  free(surface);
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
