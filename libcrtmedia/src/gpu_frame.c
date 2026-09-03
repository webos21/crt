/* crtmedia/gpu_frame.h's own real reference implementation -- see that
 * header's own top comment for the full design reasoning. A plain,
 * host-independent source file (no FFmpeg, no host GPU type at all) --
 * compiled unconditionally into crtmedia/crtmedia_shared, the same slot
 * frame.c/frame_convert.c/audio.c already occupy. */

#include "crtmedia/gpu_frame.h"

#include <stdlib.h>
#include <string.h>

static void crtmedia_gpu_frame_release_cpu_storage(crtmedia_gpu_frame* frame, void* release_context) {
  (void)frame;
  free(release_context);
}

crtmedia_result crtmedia_gpu_frame_create_cpu(
    crtmedia_pixel_format format, uint32_t width, uint32_t height, int64_t timestamp_us,
    crtmedia_gpu_frame* out_frame) {
  if (out_frame == NULL || width == 0 || height == 0) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }

  crtmedia_frame_plane planes[CRTMEDIA_FRAME_MAX_PLANES];
  uint32_t plane_count = 0;
  /* Real plane layout comes straight from the already-existing, already-
   * tested crtmedia_frame_describe_planes() (crtmedia/frame.h) -- this
   * function also rejects an unrecognized `format` (CRTMEDIA_ERROR_
   * INVALID_ARGUMENT), so that validation is not duplicated here. */
  crtmedia_result describe_result = crtmedia_frame_describe_planes(format, width, height, planes, &plane_count);
  if (describe_result != CRTMEDIA_OK) {
    return describe_result;
  }

  /* One single allocation for every plane's real backing storage --
   * simpler ownership (one free() call, matching crtmedia_gpu_frame_
   * release_cpu_storage() below) than one malloc() per plane. */
  size_t total_bytes = 0;
  uint32_t i;
  for (i = 0; i < plane_count; ++i) {
    total_bytes += (size_t)planes[i].stride * (size_t)planes[i].height;
  }

  uint8_t* storage = (uint8_t*)calloc(1, total_bytes);
  if (storage == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  size_t offset = 0;
  for (i = 0; i < plane_count; ++i) {
    planes[i].data = storage + offset;
    offset += (size_t)planes[i].stride * (size_t)planes[i].height;
  }

  memset(out_frame, 0, sizeof(*out_frame));
  out_frame->format = format;
  out_frame->width = width;
  out_frame->height = height;
  out_frame->memory_kind = CRTMEDIA_GPU_MEMORY_CPU;
  out_frame->timestamp_us = timestamp_us;
  out_frame->device_id = 0;
  out_frame->native_handle = NULL;
  out_frame->plane_count = plane_count;
  for (i = 0; i < plane_count; ++i) {
    out_frame->planes[i] = planes[i];
  }
  out_frame->release = crtmedia_gpu_frame_release_cpu_storage;
  out_frame->release_context = storage;
  return CRTMEDIA_OK;
}

void crtmedia_gpu_frame_release(crtmedia_gpu_frame* frame) {
  if (frame == NULL) {
    return;
  }
  if (frame->release != NULL) {
    frame->release(frame, frame->release_context);
  }
  memset(frame, 0, sizeof(*frame));
}
