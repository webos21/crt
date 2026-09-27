#include "crtmedia/capture.h"

#include "capture_internal.h"

#include <stdlib.h>

struct crtmedia_capture {
  void* backend;
};

crtmedia_result crtmedia_capture_enumerate(
    crtmedia_capture_device_info* devices,
    size_t capacity,
    size_t* out_count) {
  if (out_count == NULL || (capacity != 0 && devices == NULL)) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
#if defined(CRT_TARGET_OS_LINUX)
  return crtmedia_capture_backend_enumerate(devices, capacity, out_count);
#else
  (void)devices;
  (void)capacity;
  *out_count = 0;
  return CRTMEDIA_ERROR_UNSUPPORTED;
#endif
}

crtmedia_result crtmedia_capture_open(
    const char* device_id,
    const crtmedia_capture_config* requested,
    crtmedia_capture** out_capture,
    crtmedia_capture_config* out_actual) {
  crtmedia_capture* capture;
  crtmedia_result result;
  void* backend = NULL;
  if (device_id == NULL || device_id[0] == '\0' || out_capture == NULL ||
      out_actual == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_capture = NULL;
#if defined(CRT_TARGET_OS_LINUX)
  result = crtmedia_capture_backend_open(
      device_id, requested, &backend, out_actual);
  if (result != CRTMEDIA_OK) {
    return result;
  }
  capture = (crtmedia_capture*)calloc(1, sizeof(*capture));
  if (capture == NULL) {
    crtmedia_capture_backend_release(backend);
    return CRTMEDIA_ERROR_IO;
  }
  capture->backend = backend;
  *out_capture = capture;
  return CRTMEDIA_OK;
#else
  (void)requested;
  (void)capture;
  (void)result;
  (void)backend;
  return CRTMEDIA_ERROR_UNSUPPORTED;
#endif
}

crtmedia_result crtmedia_capture_start(crtmedia_capture* capture) {
  if (capture == NULL || capture->backend == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
#if defined(CRT_TARGET_OS_LINUX)
  return crtmedia_capture_backend_start(capture->backend);
#else
  return CRTMEDIA_ERROR_UNSUPPORTED;
#endif
}

crtmedia_result crtmedia_capture_dequeue_frame(
    crtmedia_capture* capture, int timeout_ms, crtmedia_frame* out_frame) {
  if (capture == NULL || capture->backend == NULL || out_frame == NULL ||
      timeout_ms < -1) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
#if defined(CRT_TARGET_OS_LINUX)
  return crtmedia_capture_backend_dequeue(capture->backend, timeout_ms, out_frame);
#else
  return CRTMEDIA_ERROR_UNSUPPORTED;
#endif
}

crtmedia_result crtmedia_capture_stop(crtmedia_capture* capture) {
  if (capture == NULL || capture->backend == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
#if defined(CRT_TARGET_OS_LINUX)
  return crtmedia_capture_backend_stop(capture->backend);
#else
  return CRTMEDIA_ERROR_UNSUPPORTED;
#endif
}

void crtmedia_capture_release(crtmedia_capture* capture) {
  if (capture == NULL) {
    return;
  }
#if defined(CRT_TARGET_OS_LINUX)
  if (capture->backend != NULL) {
    crtmedia_capture_backend_release(capture->backend);
  }
#endif
  free(capture);
}
