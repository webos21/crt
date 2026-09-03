// Real coverage for crtgfx/gpu.h (TODO.md's upper-runtime roadmap "Fix the
// common GPU resource contract" step, then "Enable Skia GPU rendering"'s
// own real Ganesh/Vulkan vertical slice, 2026-09-03) -- argument validation
// on every function, crtgfx_gpu_query_capabilities()'s own real, honest
// report, and a real, working, cross-thread crtgfx_gpu_fence wait/signal/
// timeout.
//
// This test is deliberately host/environment-adaptive rather than
// hardcoding one expected capability report: it asks crtgfx_gpu_query_
// capabilities() what is *actually* true on this host right now and
// exercises whichever real path that implies --
//   - device_count == 0 (Windows/macOS today; Linux without a real
//     libvulkan found at configure time): crtgfx_gpu_device_create() must
//     still, correctly report CRTGFX_ERROR_UNSUPPORTED -- the same "no
//     usable host backend right now" contract crtgfx_window_create()
//     already uses, not a crash/hang.
//   - device_count > 0 (Linux, real libvulkan found -- src/arch/linux/
//     gpu_vulkan.c, real device_index 0 is this host's own real, preferred
//     GPU): crtgfx_gpu_device_create()/_retain()/_release() and crtgfx_gpu_
//     fence_create() with a *real*, non-NULL device must now genuinely
//     succeed -- this is the first time in this project a real crtgfx_gpu_
//     device is actually reachable, not just argument-validated.
// Both branches are real, both are exercised on whichever host actually
// runs this binary -- never assumed from CRT_TARGET_OS at compile time,
// since even Linux itself only takes the real path when libvulkan-dev was
// actually present at configure time.
//
// Still deliberately does NOT attempt a real device->surface round trip:
// crtgfx_gpu_surface_create() stays CRTGFX_ERROR_UNSUPPORTED everywhere,
// including a host with a real device now (see gpu.h's own top comment) --
// no host can yet actually present a Ganesh-drawn surface to a real
// on-screen window. See libcrtgfx/tests/skia_gpu_offscreen_smoke.cc for
// this vertical slice's own real Ganesh/Vulkan *rendering* coverage
// (offscreen, not through crtgfx_gpu_surface at all).

#include "crtgfx/gpu.h"

#include <pthread.h>
#include <stdio.h>
#include <time.h>

static int failures = 0;

#define CHECK(cond, msg)                                                                     \
  do {                                                                                        \
    if (!(cond)) {                                                                            \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);                           \
      ++failures;                                                                             \
    }                                                                                          \
  } while (0)

static int64_t monotonic_now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000 + (int64_t)ts.tv_nsec / 1000;
}

static void sleep_us(long microseconds) {
  struct timespec ts;
  ts.tv_sec = microseconds / 1000000;
  ts.tv_nsec = (microseconds % 1000000) * 1000;
  nanosleep(&ts, NULL);
}

typedef struct signaler_args {
  crtgfx_gpu_fence* fence;
  int64_t delay_us;
} signaler_args;

static void* signaler_thread(void* arg) {
  signaler_args* args = (signaler_args*)arg;
  sleep_us(args->delay_us);
  crtgfx_gpu_fence_signal(args->fence);
  return NULL;
}

int main(void) {
  // crtgfx_gpu_query_capabilities(): real, honest, host-independent report.
  CHECK(
      crtgfx_gpu_query_capabilities(NULL) == CRTGFX_ERROR_INVALID_ARGUMENT,
      "query_capabilities(NULL) fails cleanly");
  crtgfx_gpu_capabilities caps;
  CHECK(crtgfx_gpu_query_capabilities(&caps) == CRTGFX_OK, "query_capabilities() succeeds");
  // Real, host-independent invariant: a real backend tag and a real
  // device_count agree in both directions, on every host.
  CHECK(
      (caps.backend == CRTGFX_GPU_BACKEND_NONE) == (caps.device_count == 0),
      "backend == NONE exactly when device_count == 0");

  // crtgfx_gpu_device_create()/_retain()/_release(): real argument
  // validation on every host, then either the real success path (a real
  // device is actually available -- today: Linux with a real libvulkan
  // found) or the real, correct CRTGFX_ERROR_UNSUPPORTED path (no real
  // backend on this host/config -- not a crash/hang), whichever
  // crtgfx_gpu_query_capabilities() actually just reported above.
  CHECK(
      crtgfx_gpu_device_create(0, NULL) == CRTGFX_ERROR_INVALID_ARGUMENT,
      "device_create(0, NULL) fails cleanly");
  CHECK(
      crtgfx_gpu_device_retain(NULL) == CRTGFX_ERROR_INVALID_ARGUMENT, "device_retain(NULL) fails cleanly");
  crtgfx_gpu_device_release(NULL); // must be a real, safe no-op

  crtgfx_gpu_device* device = NULL;
  if (caps.device_count == 0) {
    crtgfx_result device_result = crtgfx_gpu_device_create(0, &device);
    CHECK(device_result == CRTGFX_ERROR_UNSUPPORTED, "device_create() reports CRTGFX_ERROR_UNSUPPORTED today");
    CHECK(device == NULL, "device_create() leaves *out_device untouched on failure");
  } else {
    // A real, reachable device on this host -- device_index == device_count
    // (one past the real, valid [0, device_count) range) must still fail
    // cleanly; device_index 0 is this host's own real, preferred device
    // (src/arch/linux/gpu_vulkan.c orders hardware-backed devices first).
    CHECK(
        crtgfx_gpu_device_create(caps.device_count, &device) == CRTGFX_ERROR_INVALID_ARGUMENT,
        "device_create(device_count, ...) -- one past the valid range -- fails cleanly");
    CHECK(device == NULL, "device_create() leaves *out_device untouched on an out-of-range index");

    CHECK(crtgfx_gpu_device_create(0, &device) == CRTGFX_OK, "device_create(0, ...) genuinely succeeds");
    CHECK(device != NULL, "device_create() produces a real device");
    if (device != NULL) {
      CHECK(crtgfx_gpu_device_retain(device) == CRTGFX_OK, "device_retain() succeeds on a real device");
      crtgfx_gpu_device_release(device); // undoes the retain() just above; `device` itself still valid

      // crtgfx_gpu_surface_create() stays CRTGFX_ERROR_UNSUPPORTED even
      // with a real, non-NULL device now (see this file's own top
      // comment) -- no host can yet actually present through it.
      crtgfx_gpu_surface* surface = NULL;
      CHECK(
          crtgfx_gpu_surface_create(device, NULL, &surface) == CRTGFX_ERROR_INVALID_ARGUMENT,
          "surface_create(device, NULL window, ...) fails cleanly even with a real device");

      // crtgfx_gpu_fence_create() with a real, non-NULL device: gpu.h's
      // own contract has always accepted this structurally ("a non-NULL
      // device is accepted... unreachable until one exists") -- now that
      // one does, exercise it for real: today's fence implementation does
      // not yet special-case device affinity, so this must behave exactly
      // like the NULL-device fence below.
      crtgfx_gpu_fence* device_fence = NULL;
      CHECK(
          crtgfx_gpu_fence_create(device, &device_fence) == CRTGFX_OK,
          "fence_create() succeeds with a real, non-NULL device");
      CHECK(device_fence != NULL, "fence_create(device, ...) produces a real fence");
      if (device_fence != NULL) {
        CHECK(crtgfx_gpu_fence_signal(device_fence) == CRTGFX_OK, "fence_signal() succeeds on a device-affine fence");
        CHECK(
            crtgfx_gpu_fence_wait(device_fence, 5000000) == CRTGFX_OK,
            "an already-signaled device-affine fence returns CRTGFX_OK immediately");
        crtgfx_gpu_fence_release(device_fence);
      }
    }
    crtgfx_gpu_device_release(device);
    device = NULL;
  }

  // crtgfx_gpu_surface_create()/_release(): real argument validation;
  // structurally unreachable past that (see this file's own top comment).
  CHECK(
      crtgfx_gpu_surface_create(NULL, NULL, NULL) == CRTGFX_ERROR_INVALID_ARGUMENT,
      "surface_create(NULL, NULL, NULL) fails cleanly");
  crtgfx_gpu_surface_release(NULL); // must be a real, safe no-op

  // crtgfx_gpu_fence: real argument validation.
  CHECK(
      crtgfx_gpu_fence_create(NULL, NULL) == CRTGFX_ERROR_INVALID_ARGUMENT,
      "fence_create(NULL device, NULL out_fence) fails cleanly");
  CHECK(
      crtgfx_gpu_fence_wait(NULL, 0) == CRTGFX_ERROR_INVALID_ARGUMENT, "fence_wait(NULL, ...) fails cleanly");
  CHECK(crtgfx_gpu_fence_signal(NULL) == CRTGFX_ERROR_INVALID_ARGUMENT, "fence_signal(NULL) fails cleanly");
  crtgfx_gpu_fence_release(NULL); // must be a real, safe no-op

  // A real fence, device == NULL (the only reachable case today).
  crtgfx_gpu_fence* fence = NULL;
  CHECK(crtgfx_gpu_fence_create(NULL, &fence) == CRTGFX_OK, "fence_create(NULL device, ...) succeeds");
  CHECK(fence != NULL, "fence_create() produces a real fence");
  if (fence == NULL) {
    fprintf(stderr, "crtgfx_gpu_test: %d failure(s)\n", failures);
    return 1;
  }

  // Signal-before-wait: a real, level-triggered fence, not an edge --
  // already-signaled must return immediately.
  CHECK(crtgfx_gpu_fence_signal(fence) == CRTGFX_OK, "fence_signal() succeeds");
  int64_t before_immediate = monotonic_now_us();
  CHECK(
      crtgfx_gpu_fence_wait(fence, 5000000) == CRTGFX_OK,
      "an already-signaled fence returns CRTGFX_OK from wait() immediately");
  CHECK(
      monotonic_now_us() - before_immediate < 200000,
      "waiting on an already-signaled fence does not actually block");
  crtgfx_gpu_fence_release(fence);
  fence = NULL;

  // A real, genuine timeout: a fence that is never signaled.
  CHECK(crtgfx_gpu_fence_create(NULL, &fence) == CRTGFX_OK, "fence_create() succeeds again");
  int64_t before_timeout = monotonic_now_us();
  CHECK(
      crtgfx_gpu_fence_wait(fence, 50000) == CRTGFX_ERROR_TIMEOUT,
      "a never-signaled fence reports CRTGFX_ERROR_TIMEOUT, not CRTGFX_OK or a hang");
  int64_t timeout_elapsed = monotonic_now_us() - before_timeout;
  CHECK(timeout_elapsed >= 40000, "the real timeout genuinely waited close to the requested duration");
  CHECK(timeout_elapsed < 2000000, "the real timeout did not wait wildly longer than requested");
  crtgfx_gpu_fence_release(fence);
  fence = NULL;

  // A real, genuine cross-thread wait/signal -- a real pthread_create()'d
  // thread sleeps, then signals; the main thread's own wait() must
  // actually block for a real, measurable amount of wall time and then
  // return CRTGFX_OK, proving a genuine blocking wait/wake (this file's
  // own real pthread_cond_broadcast() in gpu.c), not a busy-poll or an
  // instant, coincidentally-correct return.
  CHECK(crtgfx_gpu_fence_create(NULL, &fence) == CRTGFX_OK, "fence_create() succeeds a third time");
  signaler_args args;
  args.fence = fence;
  args.delay_us = 100000; // 100ms -- generous relative to real scheduler jitter
  pthread_t thread;
  CHECK(pthread_create(&thread, NULL, signaler_thread, &args) == 0, "a real signaler thread starts");
  int64_t before_cross_thread = monotonic_now_us();
  crtgfx_result wait_result = crtgfx_gpu_fence_wait(fence, 5000000);
  int64_t cross_thread_elapsed = monotonic_now_us() - before_cross_thread;
  pthread_join(thread, NULL);
  CHECK(wait_result == CRTGFX_OK, "a real cross-thread signal wakes a real, blocked wait()");
  CHECK(
      cross_thread_elapsed >= 50000,
      "the real wait() genuinely blocked until the real signaler thread actually ran, not before");
  CHECK(
      cross_thread_elapsed < 3000000,
      "the real wait() returned promptly once genuinely signaled, not after some unrelated delay");
  crtgfx_gpu_fence_release(fence);

  if (failures != 0) {
    fprintf(stderr, "crtgfx_gpu_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtgfx_gpu_test: ok\n");
  return 0;
}
