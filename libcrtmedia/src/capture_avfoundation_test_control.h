#pragma once

#include <stddef.h>
#include <stdint.h>

#include "crtmedia/frame.h"

/* Private deterministic seam for verifying macOS NV12 (CVPixelBuffer)
 * conversion without a camera -- mirrors capture_v4l2_test_control.h's
 * role for the Linux backend. Production dequeue (src/arch/macos/
 * capture_avfoundation.c) uses this exact function, called with the real,
 * locked CVPixelBuffer's own plane 0/1 pointers and strides. */
crtmedia_result crtmedia_avfoundation_convert_nv12_to_yuv420p(
    const uint8_t* y_plane, size_t y_stride, const uint8_t* uv_plane, size_t uv_stride, uint32_t width,
    uint32_t height, crtmedia_frame* out_frame);

/* Exercises the queue's transfer-of-ownership rule without requiring a
 * camera. A dequeued slot must be empty before the caller-owned frame storage
 * is released, and backend cleanup must release only frames still queued. */
int crtmedia_avfoundation_queue_ownership_self_test(void);
