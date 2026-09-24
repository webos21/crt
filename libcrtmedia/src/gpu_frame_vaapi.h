#pragma once

#include <stdint.h>

/* Linux-only, project-private interop shape for `crtmedia_gpu_frame.
 * native_handle` (crtmedia/gpu_frame.h) when `memory_kind ==
 * CRTMEDIA_GPU_MEMORY_GPU` on this host ("Zero-copy decoded textures"
 * Tranche 3 -- docs/crtmedia_zero_copy_decode_acceptance.md has the frozen
 * contract and the FFmpeg-Vulkan-mapping vs direct-DRM-PRIME decision).
 * Not installed, not a public header -- same precedent as gpu_frame_d3d11.h
 * and codec_test_control.h: an agreement between crtmedia (producer,
 * src/codec.c + src/gpu_frame_vaapi.c) and crtgfx_skia_media (the one real
 * consumer, libcrtgfx/src/skia_bridge.cc's CRTGFX_HAVE_VULKAN branch),
 * reached through an explicit extra -I, not a crtmedia<->crtgfx build
 * dependency. A generic caller must still treat native_handle as opaque.
 *
 * This is a plain copy of libva's VADRMPRIMESurfaceDescriptor (exported by
 * vaExportSurfaceHandle(DRM_PRIME_2, SEPARATE_LAYERS)), redeclared here so
 * libcrtgfx never needs libva headers. The dma-buf file descriptors are
 * OWNED by this struct (closed by the frame's release()); a consumer that
 * imports them into Vulkan must dup() before vkAllocateMemory takes
 * ownership. The exported surface is fully decoded and synchronised
 * (vaSyncSurface) before the handle is handed out, so a consumer needs no
 * additional fence. */
#define CRTMEDIA_VAAPI_MAX_OBJECTS 4
#define CRTMEDIA_VAAPI_MAX_LAYERS 4

typedef struct crtmedia_vaapi_gpu_frame_object {
  int fd;
  uint32_t size;
  uint64_t drm_format_modifier;
} crtmedia_vaapi_gpu_frame_object;

typedef struct crtmedia_vaapi_gpu_frame_layer {
  uint32_t drm_format;   /* DRM fourcc of this layer (R8 / GR88 for NV12) */
  uint32_t num_planes;
  uint32_t object_index[4];
  uint32_t offset[4];
  uint32_t pitch[4];
} crtmedia_vaapi_gpu_frame_layer;

typedef struct crtmedia_vaapi_gpu_frame_handle {
  uint32_t fourcc;       /* VA surface fourcc (NV12) */
  uint32_t width;
  uint32_t height;
  uint32_t num_objects;
  crtmedia_vaapi_gpu_frame_object objects[CRTMEDIA_VAAPI_MAX_OBJECTS];
  uint32_t num_layers;
  crtmedia_vaapi_gpu_frame_layer layers[CRTMEDIA_VAAPI_MAX_LAYERS];
} crtmedia_vaapi_gpu_frame_handle;

#ifdef __cplusplus
extern "C" {
#endif
struct AVFrame;
/* Implemented in gpu_frame_vaapi.c (libcrtmedia-internal; libcrtgfx does
 * not call these). */
int crtmedia_vaapi_export_frame(const struct AVFrame* avframe, crtmedia_vaapi_gpu_frame_handle* out);
void crtmedia_vaapi_gpu_frame_handle_close(crtmedia_vaapi_gpu_frame_handle* handle);
#ifdef __cplusplus
}
#endif
