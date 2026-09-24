#pragma once

#include "crtgfx/skia.h"

#ifdef __cplusplus

#if CRTGFX_HAS_SKIA_HEADERS && (defined(CRTGFX_HAVE_METAL) || defined(CRTGFX_HAVE_D3D12) || defined(CRTGFX_HAVE_VULKAN))
// Zero-copy decoded-texture bridge (2026-09-23, "Zero-copy decoded
// textures" Tranches 1-3 -- docs/crtmedia_zero_copy_decode_acceptance.md has
// the full frozen contract). The one real, deliberately small, optional
// third component that document promises: depends on both libcrtmedia's
// public headers (crtmedia_gpu_frame) and libcrtgfx/Skia, so neither of
// those two libraries gains a build dependency on the other -- this
// header/skia_bridge.cc's Metal/D3D12/Vulkan branches are the only places
// that include both.
//
// Declared only when a real zero-copy import path exists for this host
// (macOS/Metal, Windows/D3D12, Linux/Vulkan, matching crtgfx/skia.h's own
// per-backend GPU-function precedent -- crtgfx_skia_make_gpu_context() et
// al. are declared the same conditional way).
#include "crtmedia/gpu_frame.h"
#include "include/core/SkImage.h"
#include "include/gpu/ganesh/GrDirectContext.h"

// Imports a real, hardware-decoded `crtmedia_gpu_frame` (memory_kind ==
// CRTMEDIA_GPU_MEMORY_GPU, produced by crtmedia_codec_dequeue_gpu_frame(),
// crtmedia/codec.h) into a real, GPU-backed SkImage -- no CPU readback: on
// macOS this wraps `frame->native_handle`'s own real CVPixelBufferRef
// directly via CVMetalTextureCache (Y and UV planes each become their own
// real MTLTexture sampling the same underlying IOSurface-backed storage
// the hardware decoder already wrote into); on Linux, `frame->native_handle`
// is a crtmedia-owned VA-API DRM PRIME descriptor (libcrtmedia/src/gpu_frame_
// vaapi.h): the decoded surface's dma-buf is imported directly into two
// single-plane VkImages (R8 Y, R8G8 UV) with VK_EXT_image_drm_format_modifier
// and sampled as YUV -- declined (null) when the crtgfx_gpu_device did not
// enable the dma-buf import extensions; on Windows, FFmpeg's own
// shared D3D11VA decode-pool texture (`frame->native_handle`'s real
// ID3D11Texture2D*/array-index pair, docs/crtmedia_zero_copy_decode_
// acceptance.md's own native_handle table) is not itself shareable with
// D3D12, and Skia's own GrD3DTextureResourceInfo has no multi-plane
// concept at all (confirmed by reading include/gpu/ganesh/d3d/
// GrD3DTypes.h directly), so this bridge extracts the Y/UV planes via a
// plane-sliced D3D11.3 ID3D11ShaderResourceView1 into two fresh,
// NT-handle-shareable single-format textures with one small compute-
// shader copy (real GPU-to-GPU, no CPU involvement -- still satisfies
// this tranche's own acceptance gate, which only excludes CPU readback,
// not an internal GPU copy), then opens those into D3D12 via
// IDXGIResource1::CreateSharedHandle/ID3D12Device::OpenSharedHandle. Both
// hosts then feed GrYUVABackendTextures -> SkImages::TextureFromYUVATextures
// to sample them as YUV directly -- no intermediate RGBA conversion
// texture, matching this tranche's own explicit "no RGBA intermediate"
// decision.
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
// same real id<MTLDevice> the texture cache is created against (macOS) or
// the same real ID3D12Device/IDXGIAdapter1 to open the shared textures
// into and check device affinity against (Windows -- see skia_bridge.cc's
// own D3D12 branch for the real LUID comparison this tranche's own
// "Device-affinity pairing" scope item requires). Returns null for any
// null argument or a non-GPU-memory frame.
sk_sp<SkImage> crtgfx_skia_import_media_frame(
    GrDirectContext* context, const crtgfx_gpu_device* device, crtmedia_gpu_frame* frame);
#endif

#endif
