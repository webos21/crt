/* Linux VA-API surface export for the zero-copy decode branch (Tranche 3,
 * docs/crtmedia_zero_copy_decode_acceptance.md). Kept in its own source so
 * only this file needs the host libva headers (-fcrt-real-linux-sdk);
 * codec.c stays free of them. */

#include "gpu_frame_vaapi.h"

#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>

#include <va/va.h>
#include <va/va_drmcommon.h>

#include <string.h>
#include <unistd.h>

void crtmedia_vaapi_gpu_frame_handle_close(crtmedia_vaapi_gpu_frame_handle* handle) {
  for (uint32_t i = 0; i < handle->num_objects && i < CRTMEDIA_VAAPI_MAX_OBJECTS; ++i) {
    if (handle->objects[i].fd >= 0) {
      close(handle->objects[i].fd);
      handle->objects[i].fd = -1;
    }
  }
  handle->num_objects = 0;
}

/* Waits for the decode to finish (vaSyncSurface -- a CPU wait, no pixel
 * copy) and exports the surface as DRM PRIME dma-bufs with separate
 * per-plane layers. Returns 0 on success, -1 on any failure (with no fds
 * left open). */
int crtmedia_vaapi_export_frame(const AVFrame* avframe, crtmedia_vaapi_gpu_frame_handle* out) {
  if (avframe == NULL || avframe->hw_frames_ctx == NULL || out == NULL) {
    return -1;
  }
  const AVHWFramesContext* frames = (const AVHWFramesContext*)avframe->hw_frames_ctx->data;
  if (frames == NULL || frames->device_ctx == NULL || frames->device_ctx->hwctx == NULL) {
    return -1;
  }
  const AVVAAPIDeviceContext* device = (const AVVAAPIDeviceContext*)frames->device_ctx->hwctx;
  VASurfaceID surface = (VASurfaceID)(uintptr_t)avframe->data[3];
  if (device == NULL || device->display == NULL) {
    return -1;
  }

  if (vaSyncSurface(device->display, surface) != VA_STATUS_SUCCESS) {
    return -1;
  }
  VADRMPRIMESurfaceDescriptor desc;
  memset(&desc, 0, sizeof(desc));
  if (vaExportSurfaceHandle(device->display, surface, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                            VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                            &desc) != VA_STATUS_SUCCESS) {
    return -1;
  }

  if (desc.num_objects > CRTMEDIA_VAAPI_MAX_OBJECTS || desc.num_layers > CRTMEDIA_VAAPI_MAX_LAYERS) {
    for (uint32_t i = 0; i < desc.num_objects && i < 4; ++i) {
      close(desc.objects[i].fd);
    }
    return -1;
  }
  memset(out, 0, sizeof(*out));
  out->fourcc = desc.fourcc;
  out->width = desc.width;
  out->height = desc.height;
  out->num_objects = desc.num_objects;
  for (uint32_t i = 0; i < desc.num_objects; ++i) {
    out->objects[i].fd = desc.objects[i].fd;
    out->objects[i].size = desc.objects[i].size;
    out->objects[i].drm_format_modifier = desc.objects[i].drm_format_modifier;
  }
  out->num_layers = desc.num_layers;
  for (uint32_t i = 0; i < desc.num_layers; ++i) {
    out->layers[i].drm_format = desc.layers[i].drm_format;
    out->layers[i].num_planes = desc.layers[i].num_planes;
    for (uint32_t p = 0; p < 4; ++p) {
      out->layers[i].object_index[p] = desc.layers[i].object_index[p];
      out->layers[i].offset[p] = desc.layers[i].offset[p];
      out->layers[i].pitch[p] = desc.layers[i].pitch[p];
    }
  }
  return 0;
}
