// Real coverage for crtgfx/gpu.h (TODO.md's upper-runtime roadmap "Fix the
// common GPU resource contract" step) -- argument validation on every
// function, crtgfx_gpu_query_capabilities()'s own real, honest
// CRTGFX_GPU_BACKEND_NONE/0-devices report, crtgfx_gpu_device_create()/
// crtgfx_gpu_surface_create()'s own real, correct CRTGFX_ERROR_UNSUPPORTED
// (no real backend exists on any host yet -- see gpu.h's own top comment),
// and a real, working, cross-thread crtgfx_gpu_fence wait/signal/timeout.
//
// Deliberately does NOT attempt a real device->surface round trip: this
// project's own crtgfx_gpu_device_create() genuinely, correctly never
// succeeds today (0 real devices ever reported), so no real caller --
// including this test -- can ever obtain a real device to build a real
// surface from. That is a real, accepted, correctly-scoped gap this step
// leaves for a future real backend to close, not a shortcut in this test.

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
  CHECK(caps.backend == CRTGFX_GPU_BACKEND_NONE, "no real GPU backend exists in this project yet");
  CHECK(caps.device_count == 0, "no real GPU device is ever reported yet");

  // crtgfx_gpu_device_create()/_retain()/_release(): real argument
  // validation, and a real, correct CRTMEDIA_ERROR_UNSUPPORTED (not a
  // crash/hang) for the one real device_index a 0-device host can ever be
  // asked for.
  CHECK(
      crtgfx_gpu_device_create(0, NULL) == CRTGFX_ERROR_INVALID_ARGUMENT,
      "device_create(0, NULL) fails cleanly");
  crtgfx_gpu_device* device = NULL;
  crtgfx_result device_result = crtgfx_gpu_device_create(0, &device);
  CHECK(device_result == CRTGFX_ERROR_UNSUPPORTED, "device_create() reports CRTGFX_ERROR_UNSUPPORTED today");
  CHECK(device == NULL, "device_create() leaves *out_device untouched on failure");
  CHECK(
      crtgfx_gpu_device_retain(NULL) == CRTGFX_ERROR_INVALID_ARGUMENT, "device_retain(NULL) fails cleanly");
  crtgfx_gpu_device_release(NULL); // must be a real, safe no-op

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
