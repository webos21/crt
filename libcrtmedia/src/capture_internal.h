#pragma once

#include <stddef.h>

#include "crtmedia/capture.h"

#if defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_MACOS) || \
    defined(CRT_TARGET_OS_WINDOWS)
crtmedia_result crtmedia_capture_backend_enumerate(
    crtmedia_capture_device_info* devices, size_t capacity, size_t* out_count);
crtmedia_result crtmedia_capture_backend_open(
    const char* device_id, const crtmedia_capture_config* requested,
    void** out_backend, crtmedia_capture_config* out_actual);
crtmedia_result crtmedia_capture_backend_start(void* backend);
crtmedia_result crtmedia_capture_backend_dequeue(
    void* backend, int timeout_ms, crtmedia_frame* out_frame);
crtmedia_result crtmedia_capture_backend_stop(void* backend);
void crtmedia_capture_backend_release(void* backend);
#endif
