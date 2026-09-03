#pragma once

#include <stdint.h>

#include "crtmedia/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* GPU-frame contract (TODO.md's upper-runtime roadmap "Fix the common GPU
 * resource contract" step, crtmedia's own half of it -- see crtgfx/gpu.h
 * for the crtgfx device/surface/fence half). Defines the real,
 * host-independent shape a later real hardware-decode backend (TODO.md's
 * "Add hardware decode" steps) will produce output into, matching this
 * project's own established pattern of landing a real, tested contract
 * before a real producer exists behind it (crtmedia_frame/crtmedia_
 * audio_buffer both landed the same way, before any real FFmpeg decoder
 * existed at all).
 *
 * Deliberately its own, independent contract, not built on crtgfx/gpu.h's
 * types at all: libcrtmedia has no build dependency on libcrtgfx today
 * (confirmed directly, 2026-09-03 -- no target_link_libraries/
 * add_subdirectory edge exists), and even once a real cross-library
 * interop point is needed (TODO.md's "Connect decoder-owned textures
 * directly to Skia" step), this project's own established sibling-
 * contract precedent (CRTMEDIA_AUDIO_TIMESTAMP_NONE is its own,
 * separately-named constant from CRTMEDIA_FRAME_TIMESTAMP_NONE, same
 * value, same reasoning, kept apart "so a future divergence in either
 * contract's own timestamp semantics does not silently couple the two"
 * -- crtmedia/audio.h) is to mirror shape, not literally share a type,
 * across a real library boundary.
 *
 * A transparent struct, not opaque -- matching crtmedia_frame/crtmedia_
 * audio_buffer's own established shape exactly (crtmedia/frame.h,
 * crtmedia/audio.h): neither sibling is opaque, and neither has an
 * accessor-function family. crtgfx_gpu_device/_surface/_fence (crtgfx/
 * gpu.h) are opaque instead, because *their* own real sibling precedent
 * inside libcrtgfx (crtgfx_window) is opaque -- each library follows its
 * own already-established convention, not a project-wide rule that
 * happens to conflict here.
 *
 * `memory_kind` is always CRTMEDIA_GPU_MEMORY_CPU on every real code
 * path in this project today: no real hardware-decode producer exists
 * anywhere yet (confirmed by direct exploration, 2026-09-03 -- libcrtmedia/
 * src/arch/{linux,macos,windows}/ contain only audio sinks so far), so
 * `native_handle` is always NULL and `device_id` is always 0. crtmedia_
 * gpu_frame_create_cpu() (below) is the one real, working reference
 * producer that exists today -- the software-fallback path this step's
 * own "software fallback must remain a first-class path" requirement
 * calls for -- built directly on the already-existing, already-tested
 * crtmedia_frame_describe_planes() (crtmedia/frame.h) for its own real
 * plane-layout math, not a second implementation of the same stride
 * arithmetic. A real hardware-decode producer (a later, separate roadmap
 * step) fills the exact same struct with memory_kind ==
 * CRTMEDIA_GPU_MEMORY_GPU, a real native_handle, and plane_count == 0
 * (no real CPU-addressable plane data exists for that case). */

/* Where a real crtmedia_gpu_frame's own backing storage actually lives --
 * own copy of crtgfx_gpu_memory_kind's shape (crtgfx/gpu.h), deliberately
 * not that literal type (see this file's own top comment). */
typedef enum crtmedia_gpu_memory_kind {
  CRTMEDIA_GPU_MEMORY_CPU = 1,
  CRTMEDIA_GPU_MEMORY_GPU = 2,
} crtmedia_gpu_memory_kind;

typedef struct crtmedia_gpu_frame crtmedia_gpu_frame;

/* Called exactly once by crtmedia_gpu_frame_release() (below) to free
 * whatever backing storage this frame owns -- same ownership contract as
 * crtmedia_frame_release_fn (crtmedia/frame.h): `release == NULL` means a
 * non-owning *view* over caller-managed storage. */
typedef void (*crtmedia_gpu_frame_release_fn)(crtmedia_gpu_frame* frame, void* release_context);

struct crtmedia_gpu_frame {
  crtmedia_pixel_format format; /* crtmedia/frame.h -- reused, not redefined */
  uint32_t width;
  uint32_t height;
  crtmedia_gpu_memory_kind memory_kind;
  int64_t timestamp_us; /* CRTMEDIA_FRAME_TIMESTAMP_NONE (crtmedia/frame.h) if unknown */
  /* Real device affinity -- mirrors crtgfx_gpu_device_create()'s own
   * `device_index` shape (crtgfx/gpu.h), deliberately its own separate
   * field, not a shared type (see this file's own top comment). 0 means
   * "no device affinity" -- the only real value any code path in this
   * project produces today (memory_kind is always CRTMEDIA_GPU_MEMORY_CPU
   * today, so device affinity does not yet apply to anything real). */
  uint32_t device_id;
  /* Type-erased -- where a real D3D11/Metal/Vulkan texture handle would
   * eventually go for a real memory_kind == CRTMEDIA_GPU_MEMORY_GPU
   * frame, never touched by this contract itself (matching this
   * project's own established "no host/upstream SDK type in a public
   * header" policy). Always NULL today. */
  void* native_handle;
  /* Only meaningful when memory_kind == CRTMEDIA_GPU_MEMORY_CPU -- 0 for
   * a real CRTMEDIA_GPU_MEMORY_GPU frame (no real CPU-addressable plane
   * data exists for that case). */
  uint32_t plane_count;
  crtmedia_frame_plane planes[CRTMEDIA_FRAME_MAX_PLANES];
  crtmedia_gpu_frame_release_fn release;
  void* release_context;
};

/* Real CPU-fallback allocator -- computes real plane layout via the
 * already-existing crtmedia_frame_describe_planes() (crtmedia/frame.h)
 * and mallocs real, addressable, zero-initialized backing storage for
 * every plane in one single allocation. Always produces memory_kind ==
 * CRTMEDIA_GPU_MEMORY_CPU, device_id == 0, native_handle == NULL --
 * exactly the software-fallback path a real hardware-decode producer
 * (a later, separate roadmap step) will eventually sit next to, not
 * replace. Returns CRTMEDIA_ERROR_INVALID_ARGUMENT for a null out_frame,
 * an unrecognized format, or a 0 width/height; CRTMEDIA_ERROR_UNSUPPORTED
 * if the real allocation itself fails. */
crtmedia_result crtmedia_gpu_frame_create_cpu(
    crtmedia_pixel_format format, uint32_t width, uint32_t height, int64_t timestamp_us,
    crtmedia_gpu_frame* out_frame);

/* Calls frame->release(frame, frame->release_context) if release is
 * non-NULL, then zeroes *frame (format becomes 0, matching crtmedia_
 * frame_release()'s own use-after-release-crashes-cleanly reasoning). A
 * NULL frame is a no-op. */
void crtmedia_gpu_frame_release(crtmedia_gpu_frame* frame);

#ifdef __cplusplus
}
#endif
