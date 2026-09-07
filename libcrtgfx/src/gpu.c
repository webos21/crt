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

#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN) && defined(CRTGFX_HAVE_NATIVE_WAYLAND)
/* Only for crtgfx_gpu_surface_create()'s own real Linux/Vulkan branch
 * below -- resolving a native-backend crtgfx_window's real live wl_
 * display/wl_surface pair (window_wayland_native.c) and the shared
 * toplevel struct's own real width/height (wayland_weston_internal.h) it
 * needs to pass into crtgfx_gpu_vulkan_surface_create(). Guarded the same
 * way gpu_internal.h's own Vulkan-only struct fields are -- every other
 * host/config never sees these Linux-only includes at all. */
#include "arch/linux/window_wayland_native.h"
#include "wayland_weston_internal.h"
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
/* Only for crtgfx_gpu_surface_create()'s own real Windows/D3D12 branch
 * below -- resolving a crtgfx_window's real live HWND (window_win32.c,
 * via crtgfx_win32_get_hwnd()) and the shared toplevel struct's own real
 * width/height. Same reasoning as the Linux branch above. */
#include "arch/windows/window_win32_gpu.h"
#include "wayland_weston_internal.h"
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
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

crtgfx_result crtgfx_gpu_query_capabilities(crtgfx_gpu_capabilities* out_caps) {
  if (out_caps == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)
  /* Real backend, Linux only (src/arch/linux/gpu_vulkan.c) -- see that
   * file's own top comment. Windows/macOS fall through to the honest
   * NONE/0 report below, same as Linux did before this landed.
   *
   * Deliberately NOT also gated on CRTGFX_HAVE_NATIVE_WAYLAND (unlike
   * crtgfx_gpu_surface_create()'s own guard just below in this file):
   * crtgfx_gpu_vulkan_query_capabilities()/crtgfx_gpu_device_create()
   * (right below) only ever enumerate/create a plain Vulkan device --
   * genuinely no Wayland connection or window involved at all, matching
   * crtgfx_skia_gpu_offscreen_smoke's own real, working offscreen-only
   * usage. This guard used to require CRTGFX_HAVE_NATIVE_WAYLAND too,
   * which made this function report device_count=0/BACKEND_NONE on any
   * build with a real libvulkan found but crtgfx-wayland-build never run
   * (a fresh out/ directory's own first CMake workflow pass, exactly
   * CI's own always-fresh runner) -- while crtgfx_gpu_device_create()
   * right below (correctly never gated on native Wayland) still found
   * and created a real device anyway. That mismatch is exactly what
   * crtgfx_gpu_test caught: query_capabilities() reporting 0 devices,
   * then device_create(0, ...) succeeding regardless, contradicting its
   * own device_count == 0 report. */
  return crtgfx_gpu_vulkan_query_capabilities(out_caps);
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
  /* Real backend, Windows only (src/arch/windows/gpu_win32.c) -- see that
   * file's own top comment. */
  return crtgfx_gpu_win32_query_capabilities(out_caps);
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
  /* Real backend, macOS only (src/arch/macos/gpu_metal.c) -- see that
   * file's own top comment. */
  return crtgfx_gpu_metal_query_capabilities(out_caps);
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
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
  {
    crtgfx_gpu_device* device = (crtgfx_gpu_device*)calloc(1, sizeof(crtgfx_gpu_device));
    crtgfx_result result;
    if (device == NULL) {
      return CRTGFX_ERROR_UNSUPPORTED;
    }
    result = crtgfx_gpu_win32_device_create(device_index, device);
    if (result != CRTGFX_OK) {
      free(device);
      return result;
    }
    atomic_init(&device->refcount, 1);
    *out_device = device;
    return CRTGFX_OK;
  }
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
  {
    crtgfx_gpu_device* device = (crtgfx_gpu_device*)calloc(1, sizeof(crtgfx_gpu_device));
    crtgfx_result result;
    if (device == NULL) {
      return CRTGFX_ERROR_UNSUPPORTED;
    }
    result = crtgfx_gpu_metal_device_create(device_index, device);
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
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
    crtgfx_gpu_win32_device_destroy(device);
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
    crtgfx_gpu_metal_device_destroy(device);
#endif
    free(device);
  }
}

crtgfx_result crtgfx_gpu_surface_create(
    crtgfx_gpu_device* device, crtgfx_window* window, crtgfx_gpu_surface** out_surface) {
  if (device == NULL || window == NULL || out_surface == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  /* CRTGFX_HAVE_NATIVE_WAYLAND, not just CRTGFX_HAVE_VULKAN, matching the
   * #include guard at the top of this file: crtgfx_native_wl_get_surface_
   * handles() (window_wayland_native.h) and struct crtgfx_window's own
   * full definition (wayland_weston_internal.h) are only actually visible
   * to this translation unit when the native Wayland backend was built
   * (libwayland-client.a exists, libcrtgfx/CMakeLists.txt's own EXISTS
   * check). A build with a real libvulkan found but crtgfx-wayland-build
   * never run (CRTGFX_HAVE_VULKAN=1 alone -- exactly a fresh `out/`
   * wipe's own CMake workflow, before the Wayland one-time bootstrap
   * step has ever run) has no business calling into a function this
   * translation unit never declared: `error: call to undeclared
   * function 'crtgfx_native_wl_get_surface_handles'` plus `incomplete
   * definition of type 'struct crtgfx_window'` (both real, reproduced on
   * a genuinely fresh out/ directory, matching GitHub CI's own always-
   * fresh runner). The already-correct honest-CRTGFX_ERROR_UNSUPPORTED
   * fallback right below was already written for exactly this case but
   * was unreachable: an `#elif` with the identical condition as this
   * `#if` can never be taken. */
#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN) && defined(CRTGFX_HAVE_NATIVE_WAYLAND)
  {
    void* wl_display_handle = NULL;
    void* wl_surface_handle = NULL;
    crtgfx_gpu_surface* surface;
    crtgfx_result result;

    /* `window` is only ever a real, live target here if it was created
     * with CRTGFX_WINDOW_GPU_PRESENTATION (crtgfx/window.h) -- the native
     * Wayland backend's own crtgfx_native_wl_get_surface_handles() is the
     * single real source of truth for that (checks the backend tag
     * itself, see window_wayland_native.h's own comment), so this
     * function never needs to duplicate that check. A legacy (software)
     * window, or any other real reason a live connection is not
     * available, both correctly fail here with the same honest CRTGFX_
     * ERROR_UNSUPPORTED this function already returned for every window
     * before today. */
    if (crtgfx_native_wl_get_surface_handles(&window->toplevel, &wl_display_handle, &wl_surface_handle) == 0) {
      return CRTGFX_ERROR_UNSUPPORTED;
    }

    surface = (crtgfx_gpu_surface*)calloc(1, sizeof(crtgfx_gpu_surface));
    if (surface == NULL) {
      return CRTGFX_ERROR_UNSUPPORTED;
    }
    result = crtgfx_gpu_vulkan_surface_create(
        device, wl_display_handle, wl_surface_handle, window->toplevel.width, window->toplevel.height, surface);
    if (result != CRTGFX_OK) {
      free(surface);
      return result;
    }
    *out_surface = surface;
    return CRTGFX_OK;
  }
#elif defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)
  (void)device;
  (void)window;
  (void)out_surface;
  return CRTGFX_ERROR_UNSUPPORTED;
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
  {
    void* hwnd_handle = NULL;
    crtgfx_gpu_surface* surface;
    crtgfx_result result;

    /* Only GPU windows expose their HWND here: software windows already
     * own a D3D11 swapchain and cannot accept another flip swapchain. */
    if (crtgfx_win32_get_hwnd(&window->toplevel, &hwnd_handle) == 0) {
      return CRTGFX_ERROR_UNSUPPORTED;
    }

    surface = (crtgfx_gpu_surface*)calloc(1, sizeof(crtgfx_gpu_surface));
    if (surface == NULL) {
      return CRTGFX_ERROR_UNSUPPORTED;
    }
    result = crtgfx_gpu_win32_surface_create(
        device, hwnd_handle, window->toplevel.width, window->toplevel.height, surface);
    if (result != CRTGFX_OK) {
      free(surface);
      return result;
    }
    *out_surface = surface;
    return CRTGFX_OK;
  }
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
  {
    void* metal_layer_handle = NULL;
    crtgfx_gpu_surface* surface;
    crtgfx_result result;

    /* window_cocoa.c only gives a window a real CAMetalLayer-backed
     * content view when it was created with CRTGFX_WINDOW_GPU_PRESENTATION
     * (crtgfx/window.h) -- crtgfx_cocoa_get_metal_layer() is the single
     * real source of truth for that, so this function never needs to
     * duplicate the check. A software-presented window (the ordinary
     * plain-CALayer content view) correctly fails here with the same
     * honest CRTGFX_ERROR_UNSUPPORTED this function already returned for
     * every window before today. */
    if (crtgfx_cocoa_get_metal_layer(&window->toplevel, &metal_layer_handle) == 0) {
      return CRTGFX_ERROR_UNSUPPORTED;
    }

    surface = (crtgfx_gpu_surface*)calloc(1, sizeof(crtgfx_gpu_surface));
    if (surface == NULL) {
      return CRTGFX_ERROR_UNSUPPORTED;
    }
    result = crtgfx_gpu_metal_surface_create(
        device, metal_layer_handle, window->toplevel.width, window->toplevel.height, surface);
    if (result != CRTGFX_OK) {
      free(surface);
      return result;
    }
    *out_surface = surface;
    return CRTGFX_OK;
  }
#else
  (void)device;
  (void)window;
  /* Real, structurally unreachable today on this host: crtgfx_gpu_device_
   * create() never succeeds here, so no real caller can ever obtain a
   * real `device` to pass at all (see gpu.h's own top comment) -- this
   * argument validation is the only part of this function any real
   * caller can exercise until a future backend lands. */
  return CRTGFX_ERROR_UNSUPPORTED;
#endif
}

void crtgfx_gpu_surface_release(crtgfx_gpu_surface* surface) {
  if (surface == NULL) {
    return;
  }
#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)
  crtgfx_gpu_vulkan_surface_destroy(surface);
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
  crtgfx_gpu_win32_surface_destroy(surface);
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
  crtgfx_gpu_metal_surface_destroy(surface);
#endif
  free(surface);
}

crtgfx_result crtgfx_gpu_surface_get_size(crtgfx_gpu_surface* surface, uint32_t* out_width, uint32_t* out_height) {
  if (surface == NULL || out_width == NULL || out_height == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
#if (defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)) || \
    (defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)) || \
    (defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL))
  /* width/height are named identically across all three real per-host
   * branches of struct crtgfx_gpu_surface (gpu_internal.h) -- one shared
   * read, no per-host dispatch needed. */
  *out_width = surface->width;
  *out_height = surface->height;
  return CRTGFX_OK;
#else
  return CRTGFX_ERROR_UNSUPPORTED;
#endif
}

crtgfx_result crtgfx_gpu_surface_acquire(crtgfx_gpu_surface* surface, uint64_t timeout_us) {
  if (surface == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)
  return crtgfx_gpu_vulkan_surface_acquire(surface, timeout_us);
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
  return crtgfx_gpu_win32_surface_acquire(surface, timeout_us);
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
  return crtgfx_gpu_metal_surface_acquire(surface, timeout_us);
#else
  (void)timeout_us;
  return CRTGFX_ERROR_UNSUPPORTED;
#endif
}

crtgfx_result crtgfx_gpu_surface_clear(crtgfx_gpu_surface* surface, float r, float g, float b, float a) {
  if (surface == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)
  return crtgfx_gpu_vulkan_surface_clear(surface, r, g, b, a);
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
  return crtgfx_gpu_win32_surface_clear(surface, r, g, b, a);
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
  return crtgfx_gpu_metal_surface_clear(surface, r, g, b, a);
#else
  (void)r;
  (void)g;
  (void)b;
  (void)a;
  return CRTGFX_ERROR_UNSUPPORTED;
#endif
}

crtgfx_result crtgfx_gpu_surface_present(crtgfx_gpu_surface* surface) {
  if (surface == NULL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
#if defined(CRT_TARGET_OS_LINUX) && defined(CRTGFX_HAVE_VULKAN)
  return crtgfx_gpu_vulkan_surface_present(surface);
#elif defined(CRT_TARGET_OS_WINDOWS) && defined(CRTGFX_HAVE_D3D12)
  return crtgfx_gpu_win32_surface_present(surface);
#elif defined(CRT_TARGET_OS_MACOS) && defined(CRTGFX_HAVE_METAL)
  return crtgfx_gpu_metal_surface_present(surface);
#else
  return CRTGFX_ERROR_UNSUPPORTED;
#endif
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
