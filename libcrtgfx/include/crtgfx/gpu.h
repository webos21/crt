#pragma once

#include <stdint.h>

#include "crtgfx/window.h"

#ifdef __cplusplus
extern "C" {
#endif

/* GPU resource contract (TODO.md's upper-runtime roadmap "Fix the common
 * GPU resource contract" step) -- the sequential gate before "Enable Skia
 * GPU rendering" and both hardware-decode steps can begin. Defines the
 * real, host-independent shape a later real GPU backend (Direct3D 11 on
 * Windows, Metal on macOS, Vulkan on Linux) will implement underneath,
 * matching this project's own established pattern of landing a real,
 * tested contract before a real backend exists behind it (crtmedia/
 * format.h+extractor.h+codec.h before FFmpeg decode was wired in;
 * crtmedia/frame.h+audio.h before any decoder existed at all).
 *
 * No host SDK type ever appears here -- matching this project's own
 * established "no host/upstream SDK type in a public header" policy
 * (docs/libcrtgfx_api_policy.md's own Non-Goals): crtgfx_gpu_backend
 * *names* Direct3D/Metal/Vulkan as enum tags (exactly how crtmedia_
 * pixel_format already names RGBA8888/YUV420P without importing any
 * FFmpeg type), it never includes a host header or typedefs a host
 * pointer type.
 *
 * Zero public GPU API exists anywhere in this project before this file
 * (confirmed by direct exploration, 2026-09-03): `crtgfx_framebuffer`
 * (crtgfx/window.h) is the only resource-handoff type that has ever
 * existed, entirely CPU-buffer based. Windows (window_win32.c) already
 * has a real, *private* D3D11 device + DXGI swap chain, but it exists
 * solely to make CRTGFX_EVENT_FRAME_COMPLETE genuinely asynchronous
 * (GDI has no completion signal) -- created per-window, never exposed,
 * not wired to this contract. macOS/Linux have no GPU objects at all
 * yet. That is why crtgfx_gpu_query_capabilities() honestly reports
 * CRTGFX_GPU_BACKEND_NONE/0 devices everywhere today, and crtgfx_gpu_
 * device_create()/crtgfx_gpu_surface_create() correctly, always fail
 * CRTGFX_ERROR_UNSUPPORTED as a result -- the same real "no usable host
 * backend right now" contract crtgfx_window_create() already uses, not
 * a stub that lies about capability. Real device/surface creation on
 * each host is explicitly later, separate roadmap steps.
 *
 * "Software fallback must remain a first-class path" (this step's own
 * documented requirement) is why crtgfx_gpu_fence -- unlike device/
 * surface -- gets a real, working, host-independent implementation now:
 * a fence has genuine standalone CPU-side synchronization utility with
 * no GPU device required at all (`device == NULL` is valid input to
 * crtgfx_gpu_fence_create(), not an error -- the only reachable case
 * today), built on this project's own already-proven pthread_mutex_t/
 * pthread_cond_t pattern (matching libcrtmedia/src/arch/macos/audio_
 * sink_coreaudio.c's own real producer/consumer wait/broadcast shape).
 *
 * Note for a future real backend: window_win32.c's existing private
 * per-window D3D11 device creation is a real, live candidate for
 * replacement once a real backend lands here -- Skia's GrDirectContext
 * and the presentation swap chain will very likely need to share one
 * real device, so crtgfx_gpu_surface_create(device, window, ...) is
 * intended to become the *sole* path a window acquires its presentation
 * device/swap chain through, not a second mechanism running alongside
 * the existing private one forever. */

/* The real backend actually selected on this host, if any -- naming
 * Direct3D/Metal/Vulkan as tags here does not violate this project's own
 * "no host SDK type in a public header" policy (see this file's own top
 * comment); CRTGFX_GPU_BACKEND_NONE is the only value any real code path
 * in this project can produce today. */
typedef enum crtgfx_gpu_backend {
  CRTGFX_GPU_BACKEND_NONE = 0,
  CRTGFX_GPU_BACKEND_D3D11 = 1,
  CRTGFX_GPU_BACKEND_METAL = 2,
  CRTGFX_GPU_BACKEND_VULKAN = 3,
} crtgfx_gpu_backend;

/* Where a real resource's own backing storage actually lives -- host-
 * visible CPU memory (the only kind this project can produce anywhere
 * today, matching this step's own "software fallback must remain a
 * first-class path" requirement) or opaque, device-resident GPU memory
 * (not yet reachable via any real code path in this project). */
typedef enum crtgfx_gpu_memory_kind {
  CRTGFX_GPU_MEMORY_CPU = 1,
  CRTGFX_GPU_MEMORY_GPU = 2,
} crtgfx_gpu_memory_kind;

/* Opaque, forward-declared only -- no internal fields defined until a
 * real backend actually needs them; adding unused fields speculatively
 * ahead of a real consumer would contradict this project's own
 * established "no speculative fields" convention (crtmedia/frame.h's
 * own CRTMEDIA_FRAME_MAX_PLANES comment states this same policy
 * explicitly). A real device is shared, reference-counted storage (see
 * crtgfx_gpu_device_retain() below) -- a future decode thread and a
 * future render thread may both need to hold it for real zero-copy
 * interop once a real backend exists. */
typedef struct crtgfx_gpu_device crtgfx_gpu_device;

/* A real GPU-backed presentation target tied to one crtgfx_window and
 * one crtgfx_gpu_device -- the eventual real replacement for window_
 * win32.c's own private per-window swap chain (see this file's own top
 * comment). Single-owner (create/release, no retain -- unlike a device,
 * a surface has exactly one real owner, the window it presents to). */
typedef struct crtgfx_gpu_surface crtgfx_gpu_surface;

/* A real, working, host-independent CPU synchronization primitive today
 * (see this file's own top comment for why) -- a future real backend's
 * own device-affine fence (wrapping a real D3D12/Vulkan fence object
 * internally) is a distinct, later addition, not a redesign of this
 * contract's own real CPU-fallback behavior. */
typedef struct crtgfx_gpu_fence crtgfx_gpu_fence;

typedef struct crtgfx_gpu_capabilities {
  crtgfx_gpu_backend backend;
  /* Real, enumerable GPU device count on this host -- 0 today
   * everywhere (see this file's own top comment); crtgfx_gpu_device_
   * create()'s own `device_index` argument is only ever valid in
   * [0, device_count). */
  uint32_t device_count;
} crtgfx_gpu_capabilities;

/* Fills `*out_caps` with this host's own real, current GPU capability --
 * CRTGFX_GPU_BACKEND_NONE/device_count=0 on every real code path in this
 * project today (see this file's own top comment), a real, honest report
 * a caller can act on to fall back to the already-working CPU
 * crtgfx_framebuffer path, not a placeholder that will silently start
 * lying once a real backend lands (a real backend updates this function
 * itself). Returns CRTGFX_ERROR_INVALID_ARGUMENT for a null out_caps. */
crtgfx_result crtgfx_gpu_query_capabilities(crtgfx_gpu_capabilities* out_caps);

/* Creates a real device for `device_index` (see crtgfx_gpu_capabilities::
 * device_count above for the valid range) with an initial refcount of 1
 * -- matching crtgfx_window_create()'s own established "no usable host
 * backend right now" graceful-degradation contract exactly: returns
 * CRTGFX_ERROR_UNSUPPORTED (never a crash/hang) on every real code path
 * in this project today, since crtgfx_gpu_query_capabilities() always
 * reports 0 real devices to request one of. Returns CRTGFX_ERROR_
 * INVALID_ARGUMENT for a null out_device. */
crtgfx_result crtgfx_gpu_device_create(uint32_t device_index, crtgfx_gpu_device** out_device);

/* Real, atomic shared-ownership increment (a future decode thread and a
 * future render thread may both need to hold the same real device for
 * zero-copy interop) -- returns CRTGFX_ERROR_INVALID_ARGUMENT for a null
 * device, CRTGFX_OK otherwise. */
crtgfx_result crtgfx_gpu_device_retain(crtgfx_gpu_device* device);

/* Real, atomic decrement; frees the device's own real storage once the
 * count reaches 0. A NULL device is a safe no-op. */
void crtgfx_gpu_device_release(crtgfx_gpu_device* device);

/* Creates a real GPU-backed presentation surface for `window`, backed by
 * `device`. Same real "no usable backend right now" contract as crtgfx_
 * gpu_device_create() -- CRTGFX_ERROR_UNSUPPORTED on every real code
 * path in this project today (device_create() itself already never
 * succeeds, so this is correctly, structurally unreachable in practice
 * until a real backend lands, but its own real argument validation is
 * exercised regardless). Returns CRTGFX_ERROR_INVALID_ARGUMENT for a
 * null device/window/out_surface. */
crtgfx_result crtgfx_gpu_surface_create(
    crtgfx_gpu_device* device, crtgfx_window* window, crtgfx_gpu_surface** out_surface);

/* A NULL surface is a safe no-op. */
void crtgfx_gpu_surface_release(crtgfx_gpu_surface* surface);

/* Creates a real, working fence. `device` may be NULL -- a host-CPU
 * fence with no device affinity, the only reachable case today (see
 * this file's own top comment); a non-NULL device is accepted
 * structurally for a future real backend, unreachable until one exists
 * (device_create() never succeeds today). Returns CRTGFX_ERROR_INVALID_
 * ARGUMENT for a null out_fence. */
crtgfx_result crtgfx_gpu_fence_create(crtgfx_gpu_device* device, crtgfx_gpu_fence** out_fence);

/* Blocks the calling thread for up to `timeout_us` microseconds until
 * `fence` is signaled (crtgfx_gpu_fence_signal(), below) -- a real,
 * level-triggered wait: a fence signaled before this call still returns
 * CRTGFX_OK immediately, not just one signaled during the wait. Returns
 * CRTGFX_OK once signaled, CRTGFX_ERROR_TIMEOUT if `timeout_us` elapses
 * first (a real, expected, non-fatal outcome, not a device failure), or
 * CRTGFX_ERROR_INVALID_ARGUMENT for a null fence. */
crtgfx_result crtgfx_gpu_fence_wait(crtgfx_gpu_fence* fence, uint64_t timeout_us);

/* Signals `fence`, waking every real thread currently blocked in crtgfx_
 * gpu_fence_wait() on it (and every future wait call, until the fence is
 * released -- level-triggered, not a one-shot edge). Returns CRTGFX_
 * ERROR_INVALID_ARGUMENT for a null fence. */
crtgfx_result crtgfx_gpu_fence_signal(crtgfx_gpu_fence* fence);

/* A NULL fence is a safe no-op. */
void crtgfx_gpu_fence_release(crtgfx_gpu_fence* fence);

#ifdef __cplusplus
}
#endif
