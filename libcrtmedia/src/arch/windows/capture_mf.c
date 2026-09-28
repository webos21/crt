/* crtmedia/capture.h -- Windows backend, driven directly through real
 * Media Foundation (`IMFAttributes`/`IMFActivate`/`IMFMediaSource`/
 * `IMFSourceReader`/`IMFMediaType`/`IMFSample`/`IMFMediaBuffer`), one
 * synchronous `IMFSourceReader::ReadSample()` call at a time on a
 * dedicated background thread feeding a small, fixed-capacity queue --
 * see this file's own top-of-CMakeLists comment (`libcrtmedia/
 * CMakeLists.txt`'s `CRTMEDIA_BACKEND_SOURCES` block) for why this one
 * file deliberately uses the real mingw-w64 Media Foundation headers
 * (`#define COBJMACROS`) instead of `src/arch/windows/audio_sink_wasapi.c`'s
 * own hand-declared-vtable convention: Media Foundation's real interface
 * surface (IMFAttributes alone has ~29 methods) makes hand-counting every
 * vtable slot correctly a real, serious ABI risk, not a style preference.
 *
 * `IMFSourceReader::ReadSample()` has no timeout parameter at all -- it
 * blocks the calling thread until a sample, an error, or end-of-stream
 * arrives. `crtmedia_capture_dequeue_frame()`'s own public contract needs
 * a real, bounded `timeout_ms`, so this backend spins one dedicated
 * background thread (started by `crtmedia_capture_backend_start()`) that
 * does nothing but call `ReadSample()` in a loop and push each converted
 * frame into a fixed-capacity queue (mirrors `src/arch/macos/
 * capture_avfoundation.c`'s own delegate-thread-plus-queue shape, with
 * this project's own thread instead of a GCD-managed callback thread) --
 * `crtmedia_capture_backend_dequeue()` itself never calls into Media
 * Foundation directly, only waits on that queue with a real
 * `pthread_cond_timedwait()`.
 *
 * Real, honest simplification, not yet independently verified for every
 * possible camera driver: the `IMFMediaBuffer` a `ConvertToContiguousBuffer()`
 * call returns exposes only a flat pointer and a total valid length, no
 * explicit per-plane stride (that needs the separate, optional
 * `IMF2DBuffer`/`IMF2DBuffer2` interface this file does not implement) --
 * both conversion paths below (NV12 de-interleave, I420 straight copy)
 * assume the common, tightly packed layout (`stride == width`), rejecting a
 * frame whose buffer is smaller than that exact packed size rather than
 * silently reading past the end of a padded one.
 *
 * `crtmedia_capture_backend_open()` prefers the camera's own native NV12 or
 * I420 media type over asking Media Foundation's built-in video processor
 * MFT to convert some other native type (commonly RGB24 or MJPEG on real
 * UVC webcams) to NV12 -- see that function's own top comment for the real
 * MF_E_INVALIDMEDIATYPE failure this sidesteps, confirmed on this project's
 * own real webcam hardware (2026-09-28).
 *
 * This translation unit is compiled with only libcrtmedia/include and
 * libcrtmedia/src on its own include path plus the real mingw-w64 headers
 * (libcrtmedia/CMakeLists.txt's own `set_source_files_properties()` for
 * this file), not this project's own libc headers for anything Win32 --
 * but it DOES use this project's own `<pthread.h>` normally (the frame
 * queue's own mutex/condvar), matching `audio_sink_wasapi.c`'s own
 * identical choice: `crtmedia_shared` links this project's own `c_shared`
 * regardless, so there is no header-collision concern the way there would
 * be mixing this project's own `<windows.h>`-adjacent declarations with a
 * real one -- here it is the reverse (real Win32 headers, this project's
 * own pthread), and the two namespaces do not collide at all. */

#define COBJMACROS
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <objbase.h>

#include "capture_internal.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* No INITGUID here: every MF_MT_.../MFVideoFormat_.../MF_DEVSOURCE_
 * ATTRIBUTE_.../IID_... constant this file touches stays a plain
 * `extern const GUID`
 * (DEFINE_GUID's own real, non-INITGUID expansion, guiddef.h), resolved at
 * link time against mfuuid.lib/strmiids.lib -- already linked into
 * `crtmedia`/`crtmedia_shared` project-wide for FFmpeg's own libavcodec/
 * mfenc.c (libcrtmedia/CMakeLists.txt's own comment on those two import
 * libraries), so this file needs no INITGUID/`__declspec(selectany)`
 * duplicate-definition story of its own. */

#define CRTMEDIA_MF_QUEUE_CAPACITY 4u

typedef struct crtmedia_mf_queued_frame {
  uint8_t* storage; /* owned Y+U+V packed YUV420P bytes */
  uint32_t width;
  uint32_t height;
  int64_t timestamp_us;
} crtmedia_mf_queued_frame;

typedef struct crtmedia_mf_capture {
  IMFMediaSource* source;
  IMFSourceReader* reader;
  uint32_t width;
  uint32_t height;
  int native_is_i420;

  pthread_t reader_thread;
  int reader_thread_started;
  volatile int stop_requested;
  int reader_failed;

  pthread_mutex_t lock;
  pthread_cond_t not_empty;
  crtmedia_mf_queued_frame queue[CRTMEDIA_MF_QUEUE_CAPACITY];
  uint32_t queue_head;
  uint32_t queue_count;

  int have_timestamp_origin;
  int64_t timestamp_origin_us;
  int64_t last_timestamp_us;

  int com_initialized; /* 1 if this capture's own CoInitializeEx() call was
                         * the one that actually initialized COM on this
                         * (the caller's) thread -- mirrors audio_sink_
                         * wasapi.c's own identical field/reasoning. */
  int mf_started;
} crtmedia_mf_capture;

static void owned_frame_release(crtmedia_frame* frame, void* context) {
  (void)frame;
  free(context);
}

static int multiply_size(size_t a, size_t b, size_t* out) {
  if (a != 0 && b > ((size_t)-1) / a) {
    return 0;
  }
  *out = a * b;
  return 1;
}

/* Converts one real, locked, contiguous NV12 IMFMediaBuffer (tightly
 * packed: Y plane stride == width, interleaved UV plane stride == width)
 * into an owned, tightly packed YUV420P crtmedia_frame -- the private,
 * resource-free-testable seam this file's own top comment promises,
 * mirroring crtmedia_v4l2_convert_to_yuv420p()/crtmedia_avfoundation_
 * convert_nv12_to_yuv420p()'s own established role for the other two
 * hosts. Returns CRTMEDIA_ERROR_INVALID_ARGUMENT if buffer_length is too
 * small for the exact packed NV12 size this function assumes -- never
 * reads past the end of a shorter, differently strided real buffer. */
crtmedia_result crtmedia_mf_convert_nv12_to_yuv420p(
    const uint8_t* nv12, size_t buffer_length, uint32_t width, uint32_t height, crtmedia_frame* out_frame) {
  uint8_t* storage;
  uint8_t* y_dst;
  uint8_t* u_dst;
  uint8_t* v_dst;
  const uint8_t* y_src;
  const uint8_t* uv_src;
  uint32_t chroma_width;
  uint32_t chroma_height;
  size_t y_size;
  size_t uv_size;
  size_t chroma_size;
  size_t storage_size;
  uint32_t row;
  uint32_t column;

  if (nv12 == NULL || out_frame == NULL || width == 0 || height == 0 || width == UINT32_MAX ||
      height == UINT32_MAX) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  memset(out_frame, 0, sizeof(*out_frame));
  chroma_width = (width + 1u) / 2u;
  chroma_height = (height + 1u) / 2u;
  if (!multiply_size((size_t)width, height, &y_size) ||
      !multiply_size((size_t)chroma_width, chroma_height, &chroma_size) ||
      chroma_size > (((size_t)-1) - y_size) / 2u) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (!multiply_size((size_t)chroma_width * 2u, chroma_height, &uv_size) || uv_size > ((size_t)-1) - y_size ||
      buffer_length < y_size + uv_size) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  storage_size = y_size + chroma_size * 2u;
  storage = (uint8_t*)malloc(storage_size);
  if (storage == NULL) {
    return CRTMEDIA_ERROR_IO;
  }
  y_dst = storage;
  u_dst = y_dst + y_size;
  v_dst = u_dst + chroma_size;
  y_src = nv12;
  uv_src = nv12 + y_size;

  for (row = 0; row < height; ++row) {
    memcpy(y_dst + (size_t)row * width, y_src + (size_t)row * width, width);
  }
  for (row = 0; row < chroma_height; ++row) {
    const uint8_t* uv_row = uv_src + (size_t)row * chroma_width * 2u;
    for (column = 0; column < chroma_width; ++column) {
      u_dst[(size_t)row * chroma_width + column] = uv_row[column * 2u];
      v_dst[(size_t)row * chroma_width + column] = uv_row[column * 2u + 1u];
    }
  }

  out_frame->format = CRTMEDIA_PIXEL_FORMAT_YUV420P;
  out_frame->width = width;
  out_frame->height = height;
  out_frame->color_range = CRTMEDIA_COLOR_RANGE_LIMITED;
  out_frame->color_space = height <= 576u ? CRTMEDIA_COLOR_SPACE_BT601 : CRTMEDIA_COLOR_SPACE_BT709;
  out_frame->timestamp_us = CRTMEDIA_FRAME_TIMESTAMP_NONE;
  out_frame->plane_count = 3;
  out_frame->planes[0] = (crtmedia_frame_plane){y_dst, width, width, height};
  out_frame->planes[1] = (crtmedia_frame_plane){u_dst, chroma_width, chroma_width, chroma_height};
  out_frame->planes[2] = (crtmedia_frame_plane){v_dst, chroma_width, chroma_width, chroma_height};
  out_frame->release = owned_frame_release;
  out_frame->release_context = storage;
  return CRTMEDIA_OK;
}

/* Copies one real, locked, contiguous I420 IMFMediaBuffer (tightly packed
 * planar Y, then U, then V -- crtmedia_capture_backend_open()'s own top
 * comment has the full "why" this path exists) into an owned crtmedia_frame.
 * I420's plane layout is byte-for-byte identical to this project's own
 * packed YUV420P, so this is a plain bounded memcpy, not a de-interleave. */
static crtmedia_result crtmedia_mf_copy_i420_to_yuv420p(
    const uint8_t* i420, size_t buffer_length, uint32_t width, uint32_t height, crtmedia_frame* out_frame) {
  uint8_t* storage;
  uint32_t chroma_width;
  uint32_t chroma_height;
  size_t y_size;
  size_t chroma_size;
  size_t storage_size;

  if (i420 == NULL || out_frame == NULL || width == 0 || height == 0 || width == UINT32_MAX ||
      height == UINT32_MAX) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  memset(out_frame, 0, sizeof(*out_frame));
  chroma_width = (width + 1u) / 2u;
  chroma_height = (height + 1u) / 2u;
  if (!multiply_size((size_t)width, height, &y_size) ||
      !multiply_size((size_t)chroma_width, chroma_height, &chroma_size) ||
      chroma_size > (((size_t)-1) - y_size) / 2u) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  storage_size = y_size + chroma_size * 2u;
  if (buffer_length < storage_size) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  storage = (uint8_t*)malloc(storage_size);
  if (storage == NULL) {
    return CRTMEDIA_ERROR_IO;
  }
  memcpy(storage, i420, storage_size);

  out_frame->format = CRTMEDIA_PIXEL_FORMAT_YUV420P;
  out_frame->width = width;
  out_frame->height = height;
  out_frame->color_range = CRTMEDIA_COLOR_RANGE_LIMITED;
  out_frame->color_space = height <= 576u ? CRTMEDIA_COLOR_SPACE_BT601 : CRTMEDIA_COLOR_SPACE_BT709;
  out_frame->timestamp_us = CRTMEDIA_FRAME_TIMESTAMP_NONE;
  out_frame->plane_count = 3;
  out_frame->planes[0] = (crtmedia_frame_plane){storage, width, width, height};
  out_frame->planes[1] = (crtmedia_frame_plane){storage + y_size, chroma_width, chroma_width, chroma_height};
  out_frame->planes[2] =
      (crtmedia_frame_plane){storage + y_size + chroma_size, chroma_width, chroma_width, chroma_height};
  out_frame->release = owned_frame_release;
  out_frame->release_context = storage;
  return CRTMEDIA_OK;
}

/* Background reader thread body -- one blocking ReadSample() call at a
 * time, pushing each converted frame into the fixed-capacity queue
 * (dropping the oldest under backpressure, matching capture_avfoundation.c's
 * own "always discards late frames" precedent for the identical real
 * reason: real camera capture is lossy under load, and the public dequeue_
 * frame() contract has no way to signal a drop back to a driver queue the
 * way V4L2's own QBUF/DQBUF loop does). Exits on a real ReadSample()
 * failure, MF_SOURCE_READERF_ENDOFSTREAM, or crtmedia_capture_backend_
 * stop()'s own stop_requested flag. */
static void* reader_thread_main(void* argument) {
  crtmedia_mf_capture* capture = (crtmedia_mf_capture*)argument;
  while (!capture->stop_requested) {
    DWORD actual_index = 0;
    DWORD sample_flags = 0;
    LONGLONG sample_time_100ns = 0;
    IMFSample* sample = NULL;
    HRESULT hr = IMFSourceReader_ReadSample(
        capture->reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &actual_index, &sample_flags,
        &sample_time_100ns, &sample);
    if (FAILED(hr)) {
      capture->reader_failed = 1;
      break;
    }
    if ((sample_flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
      if (sample != NULL) {
        IMFSample_Release(sample);
      }
      break;
    }
    if (sample == NULL) {
      continue; /* A real, allowed ReadSample() outcome: no error, no
                 * sample yet, streaming continues (e.g. a gap flag). */
    }

    IMFMediaBuffer* buffer = NULL;
    crtmedia_frame frame;
    int converted_ok = 0;
    if (SUCCEEDED(IMFSample_ConvertToContiguousBuffer(sample, &buffer)) && buffer != NULL) {
      BYTE* data = NULL;
      DWORD current_length = 0;
      if (SUCCEEDED(IMFMediaBuffer_Lock(buffer, &data, NULL, &current_length))) {
        crtmedia_result convert_result =
            capture->native_is_i420
                ? crtmedia_mf_copy_i420_to_yuv420p(
                      (const uint8_t*)data, (size_t)current_length, capture->width, capture->height, &frame)
                : crtmedia_mf_convert_nv12_to_yuv420p(
                      (const uint8_t*)data, (size_t)current_length, capture->width, capture->height, &frame);
        converted_ok = convert_result == CRTMEDIA_OK;
        IMFMediaBuffer_Unlock(buffer);
      }
      IMFMediaBuffer_Release(buffer);
    }
    IMFSample_Release(sample);
    if (!converted_ok) {
      continue;
    }

    int64_t native_us = (int64_t)(sample_time_100ns / 10);
    int64_t timestamp_us;
    pthread_mutex_lock(&capture->lock);
    if (!capture->have_timestamp_origin) {
      capture->timestamp_origin_us = native_us;
      capture->have_timestamp_origin = 1;
      timestamp_us = 0;
    } else {
      timestamp_us = native_us - capture->timestamp_origin_us;
      if (timestamp_us <= capture->last_timestamp_us) {
        timestamp_us = capture->last_timestamp_us + 1;
      }
    }
    capture->last_timestamp_us = timestamp_us;

    if (capture->queue_count == CRTMEDIA_MF_QUEUE_CAPACITY) {
      uint32_t oldest = capture->queue_head;
      free(capture->queue[oldest].storage);
      capture->queue_head = (capture->queue_head + 1u) % CRTMEDIA_MF_QUEUE_CAPACITY;
      --capture->queue_count;
    }
    {
      uint32_t slot = (capture->queue_head + capture->queue_count) % CRTMEDIA_MF_QUEUE_CAPACITY;
      capture->queue[slot].storage = (uint8_t*)frame.release_context;
      capture->queue[slot].width = frame.width;
      capture->queue[slot].height = frame.height;
      capture->queue[slot].timestamp_us = timestamp_us;
      ++capture->queue_count;
    }
    pthread_cond_signal(&capture->not_empty);
    pthread_mutex_unlock(&capture->lock);
  }
  return NULL;
}

/* Converts a real, CoTaskMemAlloc'd WCHAR* (GetAllocatedString's own
 * output convention) into a UTF-8 buffer of at most `capacity` bytes
 * (always NUL-terminated, truncating rather than overflowing). */
static void wide_to_utf8(const WCHAR* wide, char* out, size_t capacity) {
  if (capacity == 0) {
    return;
  }
  if (wide == NULL) {
    out[0] = '\0';
    return;
  }
  int written = WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, (int)capacity, NULL, NULL);
  if (written <= 0) {
    out[0] = '\0';
  } else {
    out[capacity - 1] = '\0';
  }
}

static void release_backend(crtmedia_mf_capture* capture) {
  if (capture == NULL) {
    return;
  }
  if (capture->reader_thread_started) {
    capture->stop_requested = 1;
    pthread_join(capture->reader_thread, NULL);
  }
  if (capture->reader != NULL) {
    IMFSourceReader_Release(capture->reader);
  }
  if (capture->source != NULL) {
    IMFMediaSource_Shutdown(capture->source);
    IMFMediaSource_Release(capture->source);
  }
  for (uint32_t index = 0; index < CRTMEDIA_MF_QUEUE_CAPACITY; ++index) {
    free(capture->queue[(capture->queue_head + index) % CRTMEDIA_MF_QUEUE_CAPACITY].storage);
  }
  pthread_cond_destroy(&capture->not_empty);
  pthread_mutex_destroy(&capture->lock);
  if (capture->mf_started) {
    MFShutdown();
  }
  if (capture->com_initialized) {
    CoUninitialize();
  }
  free(capture);
}

crtmedia_result crtmedia_capture_backend_enumerate(
    crtmedia_capture_device_info* devices, size_t capacity, size_t* out_count) {
  int com_initialized = SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED));
  if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {
    if (com_initialized) CoUninitialize();
    *out_count = 0;
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  IMFAttributes* attributes = NULL;
  size_t reported = 0;
  crtmedia_result result = CRTMEDIA_OK;
  if (SUCCEEDED(MFCreateAttributes(&attributes, 1)) &&
      SUCCEEDED(IMFAttributes_SetGUID(
          attributes, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) {
    IMFActivate** activate_list = NULL;
    UINT32 activate_count = 0;
    if (SUCCEEDED(MFEnumDeviceSources(attributes, &activate_list, &activate_count))) {
      for (UINT32 index = 0; index < activate_count; ++index) {
        WCHAR* symbolic_link = NULL;
        WCHAR* friendly_name = NULL;
        UINT32 length = 0;
        IMFAttributes_GetAllocatedString(
            (IMFAttributes*)activate_list[index], &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
            &symbolic_link, &length);
        IMFAttributes_GetAllocatedString(
            (IMFAttributes*)activate_list[index], &MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &friendly_name, &length);
        if (reported < capacity) {
          wide_to_utf8(symbolic_link, devices[reported].id, sizeof(devices[reported].id));
          wide_to_utf8(friendly_name, devices[reported].name, sizeof(devices[reported].name));
        }
        ++reported;
        if (symbolic_link != NULL) CoTaskMemFree(symbolic_link);
        if (friendly_name != NULL) CoTaskMemFree(friendly_name);
        IMFActivate_Release(activate_list[index]);
      }
      CoTaskMemFree(activate_list);
    } else {
      result = CRTMEDIA_ERROR_UNSUPPORTED;
    }
  } else {
    result = CRTMEDIA_ERROR_UNSUPPORTED;
  }
  if (attributes != NULL) {
    IMFAttributes_Release((IMFAttributes*)attributes);
  }
  *out_count = reported;
  MFShutdown();
  if (com_initialized) CoUninitialize();
  return result;
}

crtmedia_result crtmedia_capture_backend_open(
    const char* device_id, const crtmedia_capture_config* requested, void** out_backend,
    crtmedia_capture_config* out_actual) {
  if (out_backend == NULL || out_actual == NULL ||
      (requested != NULL && requested->format != 0 && requested->format != CRTMEDIA_PIXEL_FORMAT_YUV420P)) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_backend = NULL;

  crtmedia_mf_capture* capture = (crtmedia_mf_capture*)calloc(1, sizeof(*capture));
  if (capture == NULL) {
    return CRTMEDIA_ERROR_IO;
  }
  pthread_mutex_init(&capture->lock, NULL);
  pthread_cond_init(&capture->not_empty, NULL);
  capture->last_timestamp_us = -1;
  capture->com_initialized = SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED));
  if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {
    release_backend(capture);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  capture->mf_started = 1;

  crtmedia_result result = CRTMEDIA_ERROR_UNSUPPORTED;
  IMFAttributes* enum_attributes = NULL;
  IMFActivate** activate_list = NULL;
  UINT32 activate_count = 0;
  IMFActivate* matched = NULL;
  if (SUCCEEDED(MFCreateAttributes(&enum_attributes, 1)) &&
      SUCCEEDED(IMFAttributes_SetGUID(
          enum_attributes, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID)) &&
      SUCCEEDED(MFEnumDeviceSources(enum_attributes, &activate_list, &activate_count))) {
    for (UINT32 index = 0; index < activate_count; ++index) {
      if (matched == NULL) {
        WCHAR* symbolic_link = NULL;
        UINT32 length = 0;
        if (SUCCEEDED(IMFAttributes_GetAllocatedString(
                (IMFAttributes*)activate_list[index], &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
                &symbolic_link, &length))) {
          char candidate_id[CRTMEDIA_CAPTURE_DEVICE_ID_MAX];
          wide_to_utf8(symbolic_link, candidate_id, sizeof(candidate_id));
          if (strcmp(candidate_id, device_id) == 0) {
            matched = activate_list[index];
            IMFActivate_AddRef(matched);
          }
          CoTaskMemFree(symbolic_link);
        }
      }
      IMFActivate_Release(activate_list[index]);
    }
    CoTaskMemFree(activate_list);
  }
  if (enum_attributes != NULL) {
    IMFAttributes_Release(enum_attributes);
  }
  if (matched == NULL) {
    release_backend(capture);
    return CRTMEDIA_ERROR_IO;
  }

  HRESULT hr = IMFActivate_ActivateObject(matched, &IID_IMFMediaSource, (void**)&capture->source);
  IMFActivate_Release(matched);
  if (FAILED(hr) || capture->source == NULL) {
    release_backend(capture);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  /* MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING backs the fallback path below
   * (a native type this file does not otherwise recognize, converted to
   * NV12 by Media Foundation's own built-in video processor MFT); harmless
   * to request unconditionally even on the common path, which never needs
   * it (crtmedia_capture_backend_open()'s own comment further down). */
  IMFAttributes* reader_attributes = NULL;
  MFCreateAttributes(&reader_attributes, 1);
  if (reader_attributes != NULL) {
    IMFAttributes_SetUINT32(reader_attributes, &MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
  }
  HRESULT reader_hr = MFCreateSourceReaderFromMediaSource(capture->source, reader_attributes, &capture->reader);
  if (reader_attributes != NULL) {
    IMFAttributes_Release(reader_attributes);
  }
  if (FAILED(reader_hr)) {
    release_backend(capture);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  /* Prefer one of the device's own already-native uncompressed YUV types
   * (NV12, then I420) over asking the built-in video processor MFT to
   * convert from whatever else the device natively offers (commonly RGB24
   * or MJPEG on real UVC webcams): confirmed for real on this machine's
   * own camera (a Logitech UVC device whose only native types are RGB24
   * and I420, no NV12) that requesting SetCurrentMediaType(NV12) directly
   * -- even with MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING set -- fails
   * with MF_E_INVALIDMEDIATYPE (0xC00D5212); selecting the camera's own
   * native I420 type verbatim (no conversion requested at all) succeeds.
   * I420 is exactly this project's own packed YUV420P plane layout (Y,
   * then U, then V, no interleaving), so the reader thread below can copy
   * it out directly instead of running crtmedia_mf_convert_nv12_to_yuv420p's
   * NV12 de-interleave. Falls back to the original "ask the video
   * processor to convert the first native type to NV12" path only if
   * neither NV12 nor I420 is natively offered -- kept for whatever other
   * camera might expose only e.g. YUY2/MJPEG, not yet independently
   * verified on real hardware exposing that case. */
  UINT32 native_width = 640;
  UINT32 native_height = 480;
  IMFMediaType* selected_native_type = NULL;
  int selected_is_i420 = 0;
  {
    IMFMediaType* nv12_candidate = NULL;
    UINT32 nv12_width = 0, nv12_height = 0;
    IMFMediaType* i420_candidate = NULL;
    UINT32 i420_width = 0, i420_height = 0;
    for (DWORD probe_index = 0; nv12_candidate == NULL; ++probe_index) {
      IMFMediaType* probe_type = NULL;
      if (FAILED(IMFSourceReader_GetNativeMediaType(
              capture->reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, probe_index, &probe_type)) ||
          probe_type == NULL) {
        break;
      }
      GUID probe_subtype = {0};
      UINT64 probe_size = 0;
      IMFMediaType_GetGUID(probe_type, &MF_MT_SUBTYPE, &probe_subtype);
      IMFMediaType_GetUINT64(probe_type, &MF_MT_FRAME_SIZE, &probe_size);
      if (IsEqualGUID(&probe_subtype, &MFVideoFormat_NV12) && nv12_candidate == NULL) {
        nv12_candidate = probe_type;
        nv12_width = (UINT32)(probe_size >> 32);
        nv12_height = (UINT32)(probe_size & 0xFFFFFFFFu);
        continue; /* top preference found; loop condition stops next pass */
      }
      if (IsEqualGUID(&probe_subtype, &MFVideoFormat_I420) && i420_candidate == NULL) {
        i420_candidate = probe_type;
        i420_width = (UINT32)(probe_size >> 32);
        i420_height = (UINT32)(probe_size & 0xFFFFFFFFu);
        continue;
      }
      IMFMediaType_Release(probe_type);
    }
    if (nv12_candidate != NULL) {
      selected_native_type = nv12_candidate;
      native_width = nv12_width;
      native_height = nv12_height;
      if (i420_candidate != NULL) IMFMediaType_Release(i420_candidate);
    } else if (i420_candidate != NULL) {
      selected_native_type = i420_candidate;
      selected_is_i420 = 1;
      native_width = i420_width;
      native_height = i420_height;
    }
  }

  IMFMediaType* desired_type = selected_native_type;
  if (desired_type == NULL) {
    /* Neither native type found -- fall back to requesting NV12 from the
     * first native type's own frame size via the video processor. */
    IMFMediaType* native_type = NULL;
    if (SUCCEEDED(IMFSourceReader_GetNativeMediaType(
            capture->reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &native_type)) &&
        native_type != NULL) {
      UINT64 packed_size = 0;
      if (SUCCEEDED(IMFMediaType_GetUINT64(native_type, &MF_MT_FRAME_SIZE, &packed_size))) {
        native_width = (UINT32)(packed_size >> 32);
        native_height = (UINT32)(packed_size & 0xFFFFFFFFu);
      }
      IMFMediaType_Release(native_type);
    }
    if (FAILED(MFCreateMediaType(&desired_type)) ||
        FAILED(IMFMediaType_SetGUID(desired_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video)) ||
        FAILED(IMFMediaType_SetGUID(desired_type, &MF_MT_SUBTYPE, &MFVideoFormat_NV12))) {
      if (desired_type != NULL) IMFMediaType_Release(desired_type);
      release_backend(capture);
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
  }
  if (FAILED(IMFSourceReader_SetCurrentMediaType(
          capture->reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, desired_type))) {
    IMFMediaType_Release(desired_type);
    release_backend(capture);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  IMFMediaType_Release(desired_type);

  capture->width = native_width;
  capture->height = native_height;
  capture->native_is_i420 = selected_is_i420;
  out_actual->width = native_width;
  out_actual->height = native_height;
  out_actual->frame_rate = requested != NULL && requested->frame_rate != 0 ? requested->frame_rate : 30u;
  out_actual->format = CRTMEDIA_PIXEL_FORMAT_YUV420P;
  *out_backend = capture;
  (void)result;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_capture_backend_start(void* backend) {
  crtmedia_mf_capture* capture = (crtmedia_mf_capture*)backend;
  if (capture->reader_thread_started) {
    return CRTMEDIA_OK;
  }
  capture->have_timestamp_origin = 0;
  capture->last_timestamp_us = -1;
  capture->stop_requested = 0;
  capture->reader_failed = 0;
  if (pthread_create(&capture->reader_thread, NULL, reader_thread_main, capture) != 0) {
    return CRTMEDIA_ERROR_IO;
  }
  capture->reader_thread_started = 1;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_capture_backend_dequeue(void* backend, int timeout_ms, crtmedia_frame* out_frame) {
  crtmedia_mf_capture* capture = (crtmedia_mf_capture*)backend;
  crtmedia_mf_queued_frame queued;
  int wait_result = 0;
  if (!capture->reader_thread_started) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  memset(out_frame, 0, sizeof(*out_frame));
  pthread_mutex_lock(&capture->lock);
  while (capture->queue_count == 0 && wait_result == 0) {
    if (timeout_ms < 0) {
      wait_result = pthread_cond_wait(&capture->not_empty, &capture->lock);
    } else {
      struct timespec deadline;
      clock_gettime(CLOCK_REALTIME, &deadline);
      deadline.tv_sec += timeout_ms / 1000;
      deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
      if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_nsec -= 1000000000L;
        deadline.tv_sec += 1;
      }
      wait_result = pthread_cond_timedwait(&capture->not_empty, &capture->lock, &deadline);
      break; /* one bounded wait attempt, matching capture_v4l2.c's own
              * poll(timeout_ms)/capture_avfoundation.c's own identical
              * shape. */
    }
  }
  if (capture->queue_count == 0) {
    pthread_mutex_unlock(&capture->lock);
    return CRTMEDIA_WOULD_BLOCK;
  }
  queued = capture->queue[capture->queue_head];
  capture->queue_head = (capture->queue_head + 1u) % CRTMEDIA_MF_QUEUE_CAPACITY;
  --capture->queue_count;
  pthread_mutex_unlock(&capture->lock);

  out_frame->format = CRTMEDIA_PIXEL_FORMAT_YUV420P;
  out_frame->width = queued.width;
  out_frame->height = queued.height;
  out_frame->color_range = CRTMEDIA_COLOR_RANGE_LIMITED;
  out_frame->color_space = queued.height <= 576u ? CRTMEDIA_COLOR_SPACE_BT601 : CRTMEDIA_COLOR_SPACE_BT709;
  out_frame->timestamp_us = queued.timestamp_us;
  {
    uint32_t chroma_width = (queued.width + 1u) / 2u;
    uint32_t chroma_height = (queued.height + 1u) / 2u;
    size_t y_size = (size_t)queued.width * queued.height;
    size_t chroma_size = (size_t)chroma_width * chroma_height;
    out_frame->plane_count = 3;
    out_frame->planes[0] = (crtmedia_frame_plane){queued.storage, queued.width, queued.width, queued.height};
    out_frame->planes[1] =
        (crtmedia_frame_plane){queued.storage + y_size, chroma_width, chroma_width, chroma_height};
    out_frame->planes[2] =
        (crtmedia_frame_plane){queued.storage + y_size + chroma_size, chroma_width, chroma_width, chroma_height};
  }
  out_frame->release = owned_frame_release;
  out_frame->release_context = queued.storage;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_capture_backend_stop(void* backend) {
  crtmedia_mf_capture* capture = (crtmedia_mf_capture*)backend;
  if (!capture->reader_thread_started) {
    return CRTMEDIA_OK;
  }
  capture->stop_requested = 1;
  pthread_join(capture->reader_thread, NULL);
  capture->reader_thread_started = 0;
  return CRTMEDIA_OK;
}

void crtmedia_capture_backend_release(void* backend) {
  release_backend((crtmedia_mf_capture*)backend);
}
