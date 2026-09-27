#pragma once

#include <stddef.h>
#include <stdint.h>

#include "crtmedia/frame.h"

typedef enum crtmedia_v4l2_source_format {
  CRTMEDIA_V4L2_SOURCE_YUV420 = 1,
  CRTMEDIA_V4L2_SOURCE_NV12 = 2,
  CRTMEDIA_V4L2_SOURCE_YUYV = 3,
} crtmedia_v4l2_source_format;

/* Private deterministic seam for verifying native-layout conversion without
 * a camera. Production dequeue uses this exact function before QBUF. */
crtmedia_result crtmedia_v4l2_convert_to_yuv420p(
    const void* source, size_t source_size,
    crtmedia_v4l2_source_format source_format,
    uint32_t width, uint32_t height, uint32_t source_stride,
    crtmedia_frame* out_frame);
