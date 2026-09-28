#pragma once

#include <stddef.h>
#include <stdint.h>

#include "crtmedia/frame.h"

typedef enum crtmedia_v4l2_source_format {
  CRTMEDIA_V4L2_SOURCE_YUV420 = 1,
  CRTMEDIA_V4L2_SOURCE_NV12 = 2,
  CRTMEDIA_V4L2_SOURCE_YUYV = 3,
  /* MJPEG (2026-09-28, Encode and capture Tranche 2 real-hardware gate):
   * this project's own physical acceptance webcam (a USB UVC camera)
   * offers no raw YUV420/NV12/YUYV streaming format at all -- confirmed
   * via `v4l2-ctl --list-formats-ext`, which lists only 'JPEG' -- matching
   * the reality that most UVC webcams only stream MJPEG at useful
   * resolutions/frame rates. Never handled by crtmedia_v4l2_convert_to_
   * yuv420p() below (that function stays a pure, stateless, resource-free-
   * testable byte reshuffle); the real decode needs a stateful codec
   * context, so it lives only in capture_v4l2.c's own dequeue path, gated
   * behind CRTMEDIA_CAPTURE_HAVE_MJPEG (defined only when
   * CRTMEDIA_ENABLE_FFMPEG is also on, libcrtmedia/cmake/crtmedia_targets.
   * cmake) so a plain FFmpeg-less configure never gains a new dependency. */
  CRTMEDIA_V4L2_SOURCE_MJPEG = 4,
} crtmedia_v4l2_source_format;

/* Private deterministic seam for verifying native-layout conversion without
 * a camera. Production dequeue uses this exact function before QBUF. */
crtmedia_result crtmedia_v4l2_convert_to_yuv420p(
    const void* source, size_t source_size,
    crtmedia_v4l2_source_format source_format,
    uint32_t width, uint32_t height, uint32_t source_stride,
    crtmedia_frame* out_frame);
