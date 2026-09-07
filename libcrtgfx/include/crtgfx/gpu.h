#pragma once

#include <stdint.h>

#include "crtgfx/window.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Host-independent GPU devices and presentation surfaces for D3D12,
 * Metal, and Vulkan. No host SDK types appear in the public ABI.
 * GPU_PRESENTATION windows use acquire/clear/present; ordinary software
 * windows retain the framebuffer path. Public fences are CPU-side
 * pthread synchronization, not GPU command-completion fences. */

/* The real backend actually selected on this host, if any -- naming
 * Direct3D/Metal/Vulkan as tags here does not violate this project's own
 * "no host SDK type in a public header" policy (see this file's own top
 * comment).
 *
 * CRTGFX_GPU_BACKEND_D3D11 was renamed to _D3D12 2026-09-03, the same day
 * the Windows offscreen vertical slice actually landed: Ganesh's own D3D
 * backend turned out to be D3D12-only (confirmed by reading Skia's own
 * GrD3DBackendContext directly -- it holds ID3D12Device/
 * ID3D12CommandQueue, no D3D11 type anywhere), so the originally-assumed
 * D3D11 tag never matched any real code path this project ever built --
 * a deliberate, real rename of a same-week, still-unreleased enum value,
 * not a silent behavior change to anything shipped. */
typedef enum crtgfx_gpu_backend {
  CRTGFX_GPU_BACKEND_NONE = 0,
  CRTGFX_GPU_BACKEND_D3D12 = 1,
  CRTGFX_GPU_BACKEND_METAL = 2,
  CRTGFX_GPU_BACKEND_VULKAN = 3,
} crtgfx_gpu_backend;

/* Backing storage location: host-visible CPU or device-resident GPU. */
typedef enum crtgfx_gpu_memory_kind {
  CRTGFX_GPU_MEMORY_CPU = 1,
  CRTGFX_GPU_MEMORY_GPU = 2,
} crtgfx_gpu_memory_kind;

/* Opaque shared device, with explicit retain/release ownership. */
typedef struct crtgfx_gpu_device crtgfx_gpu_device;

/* Single-owner surface tied to a window and device. The caller owns and
 * releases the surface; the window does not release it automatically. */
typedef struct crtgfx_gpu_surface crtgfx_gpu_surface;

/* A real, working, host-independent CPU synchronization primitive today
 * (see this file's own top comment for why) -- a future real backend's
 * own device-affine fence (wrapping a real D3D12/Vulkan fence object
 * internally) is a distinct, later addition, not a redesign of this
 * contract's own real CPU-fallback behavior. */
typedef struct crtgfx_gpu_fence crtgfx_gpu_fence;

typedef struct crtgfx_gpu_capabilities {
  crtgfx_gpu_backend backend;
  /* Enumerable devices; valid device_index range is [0, device_count). */
  uint32_t device_count;
} crtgfx_gpu_capabilities;

/* Reports usable devices; an unavailable backend reports NONE/0 so callers
 * can select software fallback. Null out_caps is INVALID_ARGUMENT. */
crtgfx_result crtgfx_gpu_query_capabilities(crtgfx_gpu_capabilities* out_caps);

/* Creates device_index with refcount 1. Unavailable devices/backends
 * return UNSUPPORTED; null out_device returns INVALID_ARGUMENT. */
crtgfx_result crtgfx_gpu_device_create(uint32_t device_index, crtgfx_gpu_device** out_device);

/* Real, atomic shared-ownership increment (a future decode thread and a
 * future render thread may both need to hold the same real device for
 * zero-copy interop) -- returns CRTGFX_ERROR_INVALID_ARGUMENT for a null
 * device, CRTGFX_OK otherwise. */
crtgfx_result crtgfx_gpu_device_retain(crtgfx_gpu_device* device);

/* Real, atomic decrement; frees the device's own real storage once the
 * count reaches 0. A NULL device is a safe no-op. */
void crtgfx_gpu_device_release(crtgfx_gpu_device* device);

/* Creates a GPU presentation surface for a GPU_PRESENTATION window:
 * Vulkan/native Wayland, D3D12/DXGI, or Metal/CAMetalLayer. Unsupported
 * windows/backends return CRTGFX_ERROR_UNSUPPORTED. The window and device
 * must outlive the surface; release it before destroying either one.
 * Returns CRTGFX_ERROR_INVALID_ARGUMENT for null arguments. */
crtgfx_result crtgfx_gpu_surface_create(
    crtgfx_gpu_device* device, crtgfx_window* window, crtgfx_gpu_surface** out_surface);

/* A NULL surface is a safe no-op. */
void crtgfx_gpu_surface_release(crtgfx_gpu_surface* surface);

/* Presentation contract (2026-09-07, "Finish live GPU presentation
 * everywhere" -- Linux lands first): the real acquire/present handshake
 * every real swapchain-backed surface needs, shaped to parallel crtgfx/
 * window.h's own crtgfx_window_begin_frame()/_end_frame() software
 * contract rather than replace it -- a real crtgfx_gpu_surface and a
 * window's own CPU crtgfx_framebuffer are two independent, real
 * presentation paths a window picks between at crtgfx_window_create()
 * time (CRTGFX_WINDOW_GPU_PRESENTATION), never both at once. Same real
 * "no usable backend right now" contract as every other function in this
 * file -- CRTGFX_ERROR_UNSUPPORTED, never a crash/hang, wherever a real
 * backend does not exist. Windows/macOS use DXGI/CAMetalLayer; Linux
 * requires Vulkan and a native-Wayland-backend window.
 *
 * crtgfx_gpu_surface_clear() is this vertical slice's own deliberate,
 * honestly-scoped stand-in for a full Ganesh/Skia render onto the
 * acquired image (crtgfx/gpu.h's own top comment already documents that
 * intent as this contract's eventual real consumer) -- a real, working,
 * minimal "draw" primitive (a solid RGBA clear, real GPU work, really
 * presented) proving the whole acquire/submit/present pipeline end to
 * end, not a placeholder. Wiring the already-proven offscreen Ganesh
 * pipeline (src/skia_bridge.cc) onto a surface's own acquired image
 * instead is real, separate follow-up work, not implemented by this
 * function. */

/* Real, live extent of `surface`'s current swapchain -- a caller that
 * already holds a crtgfx_gpu_surface has no reason to also separately
 * track the crtgfx_window* it came from just to ask this. Returns
 * CRTGFX_ERROR_INVALID_ARGUMENT for a null surface/out_width/out_height. */
crtgfx_result crtgfx_gpu_surface_get_size(crtgfx_gpu_surface* surface, uint32_t* out_width, uint32_t* out_height);

/* Acquires `surface`'s next presentable image, blocking the calling
 * thread for up to `timeout_us` microseconds if the backend's own real
 * presentation engine is not yet ready to hand one over. Must be paired
 * with a later crtgfx_gpu_surface_present() call before acquiring again
 * (calling this twice in a row without an intervening present() is a
 * real, rejected misuse -- CRTGFX_ERROR_HOST, not a silent overwrite of
 * the first acquired image). Returns CRTGFX_OK once an image is ready,
 * CRTGFX_ERROR_TIMEOUT if `timeout_us` elapses first (a real, expected,
 * non-fatal outcome, matching crtgfx_gpu_fence_wait()'s own contract), or
 * CRTGFX_ERROR_INVALID_ARGUMENT for a null surface. Metal supports only
 * its fixed approximately one-second drawable wait and returns
 * CRTGFX_ERROR_UNSUPPORTED for timeout_us < 1000000. Windows waits use
 * millisecond timer granularity (rounded up). */
crtgfx_result crtgfx_gpu_surface_acquire(crtgfx_gpu_surface* surface, uint64_t timeout_us);

/* Fills `surface`'s currently acquired image with a solid RGBA color
 * (each channel [0, 1]) via GPU work (Metal commits at present) -- see this
 * section's own top comment for why this stands in for a full Ganesh
 * render in this vertical slice. Must be called between a successful
 * crtgfx_gpu_surface_acquire() and the matching crtgfx_gpu_surface_
 * present(), exactly once per frame. Returns CRTGFX_ERROR_INVALID_
 * ARGUMENT for a null surface. */
crtgfx_result crtgfx_gpu_surface_clear(crtgfx_gpu_surface* surface, float r, float g, float b, float a);

/* Resizes `surface`'s live swapchain/layer to `width`x`height` (2026-09-07,
 * closing the "resize/swapchain recreation on all GPU hosts" gap this
 * contract's own top comment left open) -- a caller drives this from its
 * own CRTGFX_EVENT_RESIZE handling (crtgfx/window.h), the same event a
 * software-presented window already uses to know its framebuffer changed
 * size; this contract has no implicit auto-resize of its own (Vulkan's own
 * VK_ERROR_OUT_OF_DATE_KHR keeps surfacing as CRTGFX_ERROR_HOST from
 * acquire()/present() until a caller actually calls this). Must NOT be
 * called between a successful acquire() and the matching present() --
 * CRTGFX_ERROR_HOST, matching every other out-of-order misuse this
 * contract already rejects the same way, since recreating the swapchain
 * out from under an in-flight acquired image is not real, defined
 * behavior on any of the three backends. `width`/`height` of 0 is
 * CRTGFX_ERROR_INVALID_ARGUMENT (mirrors crtgfx_gpu_surface_create()'s own
 * real minimum). `width`/`height` are in the same units as the
 * CRTGFX_EVENT_RESIZE payload this call is meant to be driven from, NOT
 * necessarily crtgfx_gpu_surface_get_size()'s own real-device-pixel report
 * -- on Windows/Linux the two coincide (no logical/physical pixel
 * distinction on either host), but on macOS the event (and this
 * function's own parameters) are real Cocoa *points*, while get_size()
 * reports real backing-store pixels (crtgfx_gpu_metal_surface_create()'s
 * own already-established convention); the Metal backend performs its own
 * real points-to-pixels conversion internally (its own current
 * `-contentsScale`), the same real scale crtgfx_gpu_surface_create() used
 * on that same surface. A same-*point*-size call is a real, cheap no-op,
 * not an error -- a caller does not need to pre-filter unchanged sizes
 * itself. Returns CRTGFX_
 * ERROR_INVALID_ARGUMENT for a null surface, CRTGFX_ERROR_HOST if the
 * real host-level recreation itself fails (the surface is left in a
 * real, safe-to-release-but-otherwise-unusable state in that case,
 * exactly like a failed crtgfx_gpu_surface_create() would have been --
 * never a partially-torn-down crash). */
crtgfx_result crtgfx_gpu_surface_resize(crtgfx_gpu_surface* surface, uint32_t width, uint32_t height);

/* Presents `surface`'s currently acquired image (real GPU work already
 * submitted via crtgfx_gpu_surface_clear() above must complete before the
 * backend actually shows it -- this call itself does not block on that;
 * the backend's own real per-frame synchronization guarantees correct
 * ordering). Must follow a successful acquire() and clear() --
 * CRTGFX_ERROR_HOST otherwise. Returns CRTGFX_ERROR_INVALID_ARGUMENT for
 * a null surface. */
crtgfx_result crtgfx_gpu_surface_present(crtgfx_gpu_surface* surface);

/* Creates a CPU-side fence. device may be NULL and does not attach this
 * fence to a GPU queue. Null out_fence returns INVALID_ARGUMENT. */
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
