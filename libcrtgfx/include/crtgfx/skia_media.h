#pragma once

#include "crtgfx/skia.h"

#ifdef __cplusplus

#if CRTGFX_HAS_SKIA_HEADERS && defined(CRTGFX_HAVE_METAL)
// Zero-copy decoded-texture bridge (2026-09-23, "Zero-copy decoded
// textures" Tranche 1 -- docs/crtmedia_zero_copy_decode_acceptance.md has
// the full frozen contract). The one real, deliberately small, optional
// third component that document promises: depends on both libcrtmedia's
// public headers (crtmedia_gpu_frame) and libcrtgfx/Skia, so neither of
// those two libraries gains a build dependency on the other -- this
// header/skia_bridge.cc's own Metal branch is the only place that
// includes both.
//
// Declared only when a real zero-copy import path actually exists for
// this host (today: macOS/Metal only, matching crtgfx/skia.h's own
// per-backend GPU-function precedent -- crtgfx_skia_make_gpu_context()
// et al. are declared the same conditional way). Windows/Linux gain their
// own real bodies, and this guard widens to include them, only once each
// host's own tranche (docs/crtmedia_zero_copy_decode_acceptance.md) lands
// a real implementation -- no speculative stub exists for a host that
// cannot back it yet.
#include "crtmedia/gpu_frame.h"
#include "include/core/SkImage.h"
#include "include/gpu/ganesh/GrDirectContext.h"

// Imports a real, hardware-decoded `crtmedia_gpu_frame` (memory_kind ==
// CRTMEDIA_GPU_MEMORY_GPU, produced by crtmedia_codec_dequeue_gpu_frame(),
// crtmedia/codec.h) into a real, GPU-backed SkImage -- no CPU readback: on
// macOS this wraps `frame->native_handle`'s own real CVPixelBufferRef
// directly via CVMetalTextureCache (Y and UV planes each become their own
// real MTLTexture sampling the same underlying IOSurface-backed storage
// the hardware decoder already wrote into), then GrYUVABackendTextures ->
// SkImages::TextureFromYUVATextures samples them as YUV directly -- no
// intermediate RGBA conversion texture, matching this tranche's own
// explicit "no RGBA intermediate" decision.
//
// Ownership (frozen, docs/crtmedia_zero_copy_decode_acceptance.md's own
// "Library boundary" section): on success, this function takes ownership
// of `*frame` -- it is moved into the returned SkImage's own release
// context and crtmedia_gpu_frame_release()d only once that SkImage's real
// GPU resources are actually destroyed, never before. On failure (null
// argument, `frame->memory_kind != CRTMEDIA_GPU_MEMORY_GPU`, or any real
// per-host import step failing), `frame` is untouched and still owned by
// the caller, which must release it itself.
//
// `device` must be the same crtgfx_gpu_device `context` was built from
// (crtgfx_skia_make_gpu_context(), crtgfx/skia.h) -- needed to borrow the
// same real id<MTLDevice> the texture cache is created against. Returns
// null for any null argument or a non-GPU-memory frame.
sk_sp<SkImage> crtgfx_skia_import_media_frame(
    GrDirectContext* context, const crtgfx_gpu_device* device, crtmedia_gpu_frame* frame);
#endif

#endif
