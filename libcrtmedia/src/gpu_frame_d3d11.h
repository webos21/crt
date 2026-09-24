#pragma once

#include <stdint.h>

/* Windows-only, project-private interop shape for `crtmedia_gpu_frame.
 * native_handle` (crtmedia/gpu_frame.h) when `memory_kind ==
 * CRTMEDIA_GPU_MEMORY_GPU` on this host ("Zero-copy decoded textures"
 * Tranche 2, 2026-09-23 -- docs/crtmedia_zero_copy_decode_acceptance.md has
 * the full frozen contract). Not installed, not a public header -- same
 * precedent as this directory's own codec_test_control.h: a real,
 * project-owned agreement between crtmedia (the producer, src/codec.c) and
 * crtgfx_skia_media (the one real consumer, libcrtgfx/src/skia_bridge.cc's
 * own CRTGFX_HAVE_D3D12 branch), which reaches this file the identical way
 * codec_test_control.h's own top comment already documents (an explicit
 * extra -I onto this directory on the consumer's own target, not a general
 * crtmedia->crtgfx or crtgfx->crtmedia build dependency -- docs/crtmedia_
 * zero_copy_decode_acceptance.md's own "Library boundary" section). A
 * generic caller of crtmedia_codec_dequeue_gpu_frame() must still treat
 * native_handle as opaque; only these two project-owned files interpret it.
 *
 * Why a struct at all, unlike macOS's plain CVPixelBufferRef: FFmpeg's own
 * hwcontext_d3d11va.c (confirmed by reading libavutil/hwcontext_d3d11va.c
 * directly, function wrap_texture_buf()) places a decoded D3D11VA frame's
 * real backing as `AVFrame->data[0] = (uint8_t*)texture` (an
 * ID3D11Texture2D*, real type erased to void* here, matching this
 * project's own "no host graphics header in libcrtmedia" policy) and
 * `AVFrame->data[1] = (uint8_t*)index` (which *array slice* of that shared
 * decode-pool texture this particular frame occupies) -- a single void*
 * genuinely cannot carry both. crtmedia_codec.c heap-allocates one of these
 * per delivered GPU frame, embedded inside the same allocation as the
 * backing's own retained AVFrame* (see fill_gpu_video_frame_d3d11()'s own
 * comment, src/codec.c) so one release() call frees both together. */
typedef struct crtmedia_d3d11_gpu_frame_handle {
  /* ID3D11Texture2D*, opaque here. Real identity confirmed directly
   * against FFmpeg 8.1.2's own hwcontext_d3d11va.c -- this is the *shared
   * decode-pool* texture (D3D11_TEXTURE2D_DESC::ArraySize == the pool
   * size), not a single-frame-dedicated one; array_index below picks the
   * one real slice this frame occupies. Its lifetime is exactly the
   * retained AVFrame's own lifetime (crtmedia_gpu_frame.release_context) --
   * this field is never independently retained/released. */
  void* texture;
  /* Which array slice of *texture this frame is (FFmpeg's own
   * AVFrame->data[1], reinterpreted from an (intptr_t) back to a real
   * integer here since this project's own producer/consumer are both C/
   * C++, not the raw pointer-shaped storage FFmpeg itself uses only to fit
   * its own generic AVFrame->data[] slot). */
  int64_t array_index;
} crtmedia_d3d11_gpu_frame_handle;
