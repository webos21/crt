#pragma once

#include <stddef.h>
#include <stdint.h>

#include "crtmedia/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Host-neutral camera capture contract. Device ids are opaque stable strings
 * for the duration of one enumeration/open sequence; callers must not parse
 * them as paths or host handles. */
#define CRTMEDIA_CAPTURE_DEVICE_ID_MAX 128
#define CRTMEDIA_CAPTURE_DEVICE_NAME_MAX 128

typedef struct crtmedia_capture_device_info {
  char id[CRTMEDIA_CAPTURE_DEVICE_ID_MAX];
  char name[CRTMEDIA_CAPTURE_DEVICE_NAME_MAX];
} crtmedia_capture_device_info;

typedef struct crtmedia_capture_config {
  uint32_t width;
  uint32_t height;
  uint32_t frame_rate;
  /* The public output format. The first backend accepts 0 (default) or
   * YUV420P and always returns tightly packed, CRT-owned YUV420P frames. */
  crtmedia_pixel_format format;
} crtmedia_capture_config;

typedef struct crtmedia_capture crtmedia_capture;

/* Reports the total number of usable capture devices in out_count and fills
 * at most capacity entries. devices may be NULL only when capacity is zero.
 * Enumeration is a snapshot: open can still fail if a device disappears. */
crtmedia_result crtmedia_capture_enumerate(
    crtmedia_capture_device_info* devices,
    size_t capacity,
    size_t* out_count);

/* Opens and negotiates one device. No buffers are flowing until start().
 * requested may be NULL for 640x480@30 YUV420P defaults. out_actual receives
 * the negotiated dimensions/rate and public output format. */
crtmedia_result crtmedia_capture_open(
    const char* device_id,
    const crtmedia_capture_config* requested,
    crtmedia_capture** out_capture,
    crtmedia_capture_config* out_actual);

crtmedia_result crtmedia_capture_start(crtmedia_capture* capture);

/* Waits up to timeout_ms (-1 means indefinitely). On success out_frame owns
 * its storage and remains valid independently of the device until
 * crtmedia_frame_release(). The backend has already returned its native
 * buffer to the capture queue before this function returns. Capture PTS is a
 * monotonic microsecond timeline relative to the first dequeued frame. */
crtmedia_result crtmedia_capture_dequeue_frame(
    crtmedia_capture* capture,
    int timeout_ms,
    crtmedia_frame* out_frame);

crtmedia_result crtmedia_capture_stop(crtmedia_capture* capture);
void crtmedia_capture_release(crtmedia_capture* capture);

#ifdef __cplusplus
}
#endif
