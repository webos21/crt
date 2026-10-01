/* crtmedia/capture.h -- macOS backend, driven directly through real
 * AVFoundation (`AVCaptureSession`/`AVCaptureDeviceInput`/
 * `AVCaptureVideoDataOutput`) and CoreMedia/CoreVideo, matching this
 * project's own established "drive Objective-C from plain C via
 * objc_msgSend, hand-declare every real type/constant/function, never
 * #import a host SDK header" convention (libcrtgfx/src/arch/macos/
 * window_cocoa.c, libcrtgfx/src/arch/macos/gpu_metal.c, this library's own
 * src/arch/macos/audio_sink_coreaudio.c).
 *
 * Every real signature/constant/behavior below -- the exact
 * AVCaptureDevice/AVCaptureDeviceInput/AVCaptureSession/
 * AVCaptureVideoDataOutput wiring, the real `kCVPixelFormatType_
 * 420YpCbCr8BiPlanarVideoRange` ('420v') numeric value, and, most load-
 * bearing, that a delegate object satisfying AVCaptureVideoDataOutput
 * SampleBufferDelegate can be built entirely at runtime via the plain
 * Objective-C runtime C API (`objc_allocateClassPair`/`class_addMethod`/
 * `class_addIvar`/`objc_registerClassPair`, no `@interface`/`@implementation`
 * needed at all) and receives real, correctly-typed callbacks -- was
 * confirmed for real (2026-09-27) via three standalone probes on this
 * exact real Mac (its built-in camera), built and run with the system's
 * own real clang (`-framework AVFoundation -framework CoreMedia -framework
 * CoreVideo -framework Foundation`), outside this project's own toolchain:
 * (1) a plain Objective-C probe capturing real frames and inspecting
 * `kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange` plane geometry; (2) the
 * same session/device/input/output wiring driven purely through
 * `objc_msgSend` with a runtime-allocated delegate class, receiving real
 * frames with the same plane geometry; (3) the same runtime class extended
 * with an instance ivar (`class_addIvar`/`object_setInstanceVariable`/
 * `object_getInstanceVariable`), confirming per-instance state storage
 * works, which is what lets more than one crtmedia_capture instance exist
 * safely. A real, confirmed environment fact, not a design choice: a bare
 * command-line binary invoked directly (no app bundle, or an unsigned/
 * un-launched-via-LaunchServices bundle) never receives real frames on
 * this host -- TCC's camera authorization is only granted to a process
 * launched through LaunchServices (`open`) from a signed `.app` bundle
 * declaring `NSCameraUsageDescription`; a plain `crtmedia_capture_v4l2_
 * test`-style CTest executable invocation is exactly the case that does
 * not get it. crtmedia_capture_backend_open()/_start() below still
 * succeed either way (AVFoundation's own API calls do not themselves
 * fail for this) -- crtmedia_capture_backend_dequeue() simply never
 * receives a real frame, so the live acceptance test (tests/capture_
 * avfoundation_test.c) reports a precise, honest skip rather than a false
 * pass or a hard failure, mirroring capture_v4l2_test.c's own no-device
 * skip for the identical "implementation complete, physical/environment
 * gate pending" reason.
 *
 * This file DOES use this project's own libc headers (pthread_mutex_t/
 * pthread_cond_t for the frame queue below, clock_gettime()) normally,
 * matching audio_sink_coreaudio.c's own established reasoning exactly:
 * crtmedia_shared explicitly links this project's own c_shared (libc.
 * dylib), not a bare, C-runtime-free link the way libcrtgfx_skia.dylib's
 * own real, confirmed clock_gettime()/libSystem bug (HISTORY.md,
 * 2026-09-21) required a dual-clock-id workaround for -- src/player.c's
 * own plain, unguarded `clock_gettime(CLOCK_MONOTONIC, ...)` already
 * relies on that same fact today. The delegate callback below (invoked on
 * a real GCD serial queue thread AVFoundation itself manages, never one
 * this project's own pthread_create() spawned) only calls plain C
 * CoreVideo functions and this project's own pthread_mutex_t/
 * pthread_cond_t -- safe for the identical reason audio_sink_coreaudio.c's
 * own top comment already documents for its AudioQueue callback (this
 * project's macOS pthread primitives are a from-scratch, address-keyed
 * wait/wake implementation with no notion of which library created the
 * calling thread).
 *
 * No Objective-C message send happens inside the delegate callback at all
 * (only real, plain CoreVideo C functions plus this project's own libc),
 * so no NSAutoreleasePool is needed there. crtmedia_capture_backend_
 * open()'s own AVFoundation object-creation sequence (factory methods
 * such as `defaultDeviceWithMediaType:`/`deviceInputWithDevice:error:`/
 * `stringWithUTF8String:`/`numberWithInt:`/`dictionaryWithObject:forKey:`
 * return autoreleased objects, real Cocoa convention) is wrapped in one
 * explicit NSAutoreleasePool, matching window_cocoa.c's own established
 * per-call pool pattern -- the session, output and delegate objects this
 * function keeps beyond open() are created via alloc+init (a real, +1 owned
 * reference per Cocoa convention, matching every other real Cocoa object
 * this project's own macOS backends already retain this same way), so
 * draining the pool at the end of open() does not free them;
 * the device/input objects and every helper string/number/dictionary are
 * genuinely only used synchronously within open() (the session internally
 * retains its own input/output once added), so it is correct for the
 * pool to release them.
 *
 * Frame-rate negotiation is intentionally not implemented yet
 * (out_actual.frame_rate echoes the request, matching capture_v4l2.c's
 * fallback when VIDIOC_S_PARM negotiation fails). Dimensions are different:
 * the public capture contract promises negotiated dimensions, so open()
 * uses the real AVCaptureSessionPreset constants together with the real
 * kCVPixelBufferWidthKey/kCVPixelBufferHeightKey video-settings keys. Every
 * delivered frame's dimensions are still read from its CVPixelBuffer and the
 * lifecycle test verifies that those dimensions match the encoder contract. */

#include "capture_internal.h"
#include "capture_avfoundation_test_control.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================== Hand-declared Objective-C runtime ABI ================== */

typedef void* id;
typedef void* SEL;
typedef void* Class;
typedef void* Ivar;
typedef unsigned char BOOL;
typedef unsigned long NSUInteger;

extern id const AVCaptureSessionPreset352x288;
extern id const AVCaptureSessionPreset640x480;
extern id const AVCaptureSessionPreset1280x720;
extern id const AVCaptureSessionPreset1920x1080;

extern id objc_msgSend(id self, SEL op, ...);
extern SEL sel_registerName(const char* name);
extern id objc_getClass(const char* name);
extern Class objc_allocateClassPair(Class superclass, const char* name, size_t extra_bytes);
extern void objc_registerClassPair(Class cls);
extern BOOL class_addMethod(Class cls, SEL name, void* imp, const char* types);
extern BOOL class_addIvar(Class cls, const char* name, size_t size, unsigned char alignment, const char* types);
extern Ivar class_getInstanceVariable(Class cls, const char* name);
extern void object_setInstanceVariable(id object, const char* name, void* value);
extern void object_getInstanceVariable(id object, const char* name, void** out_value);

/* ================== Hand-declared CoreVideo/CoreMedia ABI ================== */

typedef uint32_t OSType;
typedef int32_t CVReturn;

extern const void* const kCVPixelBufferPixelFormatTypeKey;
extern const void* const kCVPixelBufferWidthKey;
extern const void* const kCVPixelBufferHeightKey;

extern void* CMSampleBufferGetImageBuffer(void* sample_buffer);
extern CVReturn CVPixelBufferLockBaseAddress(void* pixel_buffer, uint64_t lock_flags);
extern CVReturn CVPixelBufferUnlockBaseAddress(void* pixel_buffer, uint64_t lock_flags);
extern size_t CVPixelBufferGetWidthOfPlane(void* pixel_buffer, size_t plane_index);
extern size_t CVPixelBufferGetHeightOfPlane(void* pixel_buffer, size_t plane_index);
extern size_t CVPixelBufferGetBytesPerRowOfPlane(void* pixel_buffer, size_t plane_index);
extern void* CVPixelBufferGetBaseAddressOfPlane(void* pixel_buffer, size_t plane_index);
extern OSType CVPixelBufferGetPixelFormatType(void* pixel_buffer);

/* kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ('420v') -- real, stable,
 * public CoreVideo constant, confirmed for real via this file's own top-
 * comment probe (1). CVPixelBufferLockBaseAddress's read-only flag
 * (kCVPixelBufferLock_ReadOnly) is real, stable, and documented as 1. */
#define CRT_CVPIXELFORMAT_420V 0x34323076u
#define CRT_CVPIXELBUFFER_LOCK_READONLY 1u

extern void* dispatch_queue_create(const char* label, void* attr);
extern void dispatch_sync_f(void* queue, void* context, void (*work)(void*));
extern void dispatch_release(void* object);

/* ================== Frame queue (delegate thread -> dequeue caller) ================== */

#define CRTMEDIA_AVF_QUEUE_CAPACITY 4u

typedef struct crtmedia_avfoundation_queued_frame {
  uint8_t* storage; /* owned Y+U+V packed YUV420P bytes */
  uint32_t width;
  uint32_t height;
  int64_t timestamp_us;
} crtmedia_avfoundation_queued_frame;

typedef struct crtmedia_avfoundation_capture {
  id session;
  id output;
  id delegate;
  void* dispatch_queue;
  int started;

  pthread_mutex_t lock;
  pthread_cond_t not_empty;
  crtmedia_avfoundation_queued_frame queue[CRTMEDIA_AVF_QUEUE_CAPACITY];
  uint32_t queue_head;
  uint32_t queue_count;

  int have_timestamp_origin;
  int64_t timestamp_origin_us;
  int64_t last_timestamp_us;
} crtmedia_avfoundation_capture;

static void queued_frame_reset(crtmedia_avfoundation_queued_frame* frame) {
  memset(frame, 0, sizeof(*frame));
}

static int queue_take_locked(
    crtmedia_avfoundation_capture* capture, crtmedia_avfoundation_queued_frame* out_frame) {
  crtmedia_avfoundation_queued_frame* slot;
  if (capture->queue_count == 0) {
    return 0;
  }
  slot = &capture->queue[capture->queue_head];
  *out_frame = *slot;
  queued_frame_reset(slot);
  capture->queue_head = (capture->queue_head + 1u) % CRTMEDIA_AVF_QUEUE_CAPACITY;
  --capture->queue_count;
  return 1;
}

static void queue_clear_locked(crtmedia_avfoundation_capture* capture) {
  while (capture->queue_count != 0) {
    crtmedia_avfoundation_queued_frame* slot = &capture->queue[capture->queue_head];
    free(slot->storage);
    queued_frame_reset(slot);
    capture->queue_head = (capture->queue_head + 1u) % CRTMEDIA_AVF_QUEUE_CAPACITY;
    --capture->queue_count;
  }
  capture->queue_head = 0;
}

static void dispatch_barrier_noop(void* context) {
  (void)context;
}

int crtmedia_avfoundation_queue_ownership_self_test(void) {
  crtmedia_avfoundation_capture capture;
  crtmedia_avfoundation_queued_frame taken;
  uint8_t* first;
  uint8_t* second;
  memset(&capture, 0, sizeof(capture));
  memset(&taken, 0, sizeof(taken));
  first = (uint8_t*)malloc(1);
  second = (uint8_t*)malloc(1);
  if (first == NULL || second == NULL) {
    free(first);
    free(second);
    return 0;
  }
  capture.queue_head = 3;
  capture.queue_count = 2;
  capture.queue[3].storage = first;
  capture.queue[0].storage = second;
  if (!queue_take_locked(&capture, &taken) || taken.storage != first || capture.queue[3].storage != NULL ||
      capture.queue_head != 0 || capture.queue_count != 1) {
    free(taken.storage);
    queue_clear_locked(&capture);
    return 0;
  }
  free(taken.storage);
  taken.storage = NULL;
  queue_clear_locked(&capture);
  return capture.queue_head == 0 && capture.queue_count == 0 && capture.queue[0].storage == NULL &&
         capture.queue[3].storage == NULL;
}

static int64_t now_us(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return (int64_t)ts.tv_sec * 1000000 + (int64_t)ts.tv_nsec / 1000;
}

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

/* Private deterministic seam (capture_avfoundation_test_control.h): the
 * exact conversion crtmedia_capture_backend_dequeue() below performs on a
 * real, locked CVPixelBuffer's own Y/UV plane pointers, factored out so a
 * resource-free test can verify it byte-exactly without a camera --
 * mirrors capture_v4l2_test_control.h's own crtmedia_v4l2_convert_to_
 * yuv420p() role for the Linux backend. */
crtmedia_result crtmedia_avfoundation_convert_nv12_to_yuv420p(
    const uint8_t* y_plane, size_t y_stride, const uint8_t* uv_plane, size_t uv_stride, uint32_t width,
    uint32_t height, crtmedia_frame* out_frame) {
  uint8_t* storage;
  uint8_t* y_dst;
  uint8_t* u_dst;
  uint8_t* v_dst;
  uint32_t chroma_width;
  uint32_t chroma_height;
  size_t y_size;
  size_t chroma_size;
  size_t storage_size;
  uint32_t row;
  uint32_t column;

  if (y_plane == NULL || uv_plane == NULL || out_frame == NULL || width == 0 || height == 0 ||
      width == UINT32_MAX || height == UINT32_MAX || y_stride < width || uv_stride < ((width + 1u) / 2u) * 2u) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  memset(out_frame, 0, sizeof(*out_frame));
  chroma_width = (width + 1u) / 2u;
  chroma_height = (height + 1u) / 2u;
  if (!multiply_size((size_t)width, height, &y_size) || !multiply_size((size_t)chroma_width, chroma_height, &chroma_size) ||
      chroma_size > (((size_t)-1) - y_size) / 2u) {
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

  for (row = 0; row < height; ++row) {
    memcpy(y_dst + (size_t)row * width, y_plane + (size_t)row * y_stride, width);
  }
  for (row = 0; row < chroma_height; ++row) {
    const uint8_t* uv_row = uv_plane + (size_t)row * uv_stride;
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

/* Delegate IMP for captureOutput:didOutputSampleBuffer:fromConnection: --
 * confirmed for real (this file's own top-comment probes) to be invoked
 * with these exact five arguments (self, _cmd, the AVCaptureOutput, the
 * real CMSampleBufferRef, the AVCaptureConnection) by AVFoundation on a
 * real GCD queue thread. Converts directly from the locked CVPixelBuffer's
 * own plane pointers (no intermediate raw copy) and pushes into the fixed-
 * capacity queue, dropping the oldest queued frame under backpressure --
 * matching AVCaptureVideoDataOutput's own "always discards late frames"
 * spirit (real camera capture is lossy under load; the public dequeue_
 * frame() contract has no way to signal a drop back to the driver the way
 * V4L2's own QBUF/DQBUF loop does, so the queue itself absorbs it). */
static void capture_output_callback(id self, SEL cmd, id output, void* sample_buffer, id connection) {
  crtmedia_avfoundation_capture* capture = NULL;
  void* pixel_buffer;
  crtmedia_frame frame;
  int64_t native_us;
  int64_t timestamp_us;
  (void)self;
  (void)cmd;
  (void)output;
  (void)connection;
  object_getInstanceVariable(self, "_capture", (void**)&capture);
  if (capture == NULL) {
    return;
  }
  pixel_buffer = CMSampleBufferGetImageBuffer(sample_buffer);
  if (pixel_buffer == NULL || CVPixelBufferGetPixelFormatType(pixel_buffer) != CRT_CVPIXELFORMAT_420V) {
    return;
  }
  CVPixelBufferLockBaseAddress(pixel_buffer, CRT_CVPIXELBUFFER_LOCK_READONLY);
  {
    uint32_t width = (uint32_t)CVPixelBufferGetWidthOfPlane(pixel_buffer, 0);
    uint32_t height = (uint32_t)CVPixelBufferGetHeightOfPlane(pixel_buffer, 0);
    size_t y_stride = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 0);
    size_t uv_stride = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 1);
    const uint8_t* y_plane = (const uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 0);
    const uint8_t* uv_plane = (const uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 1);
    crtmedia_result converted =
        y_plane != NULL && uv_plane != NULL
            ? crtmedia_avfoundation_convert_nv12_to_yuv420p(y_plane, y_stride, uv_plane, uv_stride, width, height, &frame)
            : CRTMEDIA_ERROR_UNSUPPORTED;
    CVPixelBufferUnlockBaseAddress(pixel_buffer, CRT_CVPIXELBUFFER_LOCK_READONLY);
    if (converted != CRTMEDIA_OK) {
      return;
    }
  }

  native_us = now_us();
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

  if (capture->queue_count == CRTMEDIA_AVF_QUEUE_CAPACITY) {
    /* Drop the oldest queued frame -- real, lossy backpressure, matching
     * this function's own top comment. */
    uint32_t oldest = capture->queue_head;
    free(capture->queue[oldest].storage);
    queued_frame_reset(&capture->queue[oldest]);
    capture->queue_head = (capture->queue_head + 1u) % CRTMEDIA_AVF_QUEUE_CAPACITY;
    --capture->queue_count;
  }
  {
    uint32_t slot = (capture->queue_head + capture->queue_count) % CRTMEDIA_AVF_QUEUE_CAPACITY;
    capture->queue[slot].storage = (uint8_t*)frame.release_context;
    capture->queue[slot].width = frame.width;
    capture->queue[slot].height = frame.height;
    capture->queue[slot].timestamp_us = timestamp_us;
    ++capture->queue_count;
  }
  pthread_cond_signal(&capture->not_empty);
  pthread_mutex_unlock(&capture->lock);
}

static pthread_once_t g_delegate_class_once = PTHREAD_ONCE_INIT;
static Class g_delegate_class = NULL;

static void register_delegate_class(void) {
  Class superclass = objc_getClass("NSObject");
  Class cls = objc_allocateClassPair(superclass, "CRTMediaAVFoundationCaptureDelegate", 0);
  if (cls == NULL) {
    g_delegate_class = objc_getClass("CRTMediaAVFoundationCaptureDelegate");
    return;
  }
  /* Type encoding confirmed for real by this file's own top-comment probe
   * (2): "v@:@^{opaqueCMSampleBuffer=}@" -- void return, self/_cmd, an
   * object (the output), an opaque CMSampleBufferRef pointer, an object
   * (the connection). */
  class_addMethod(
      cls, sel_registerName("captureOutput:didOutputSampleBuffer:fromConnection:"),
      (void*)capture_output_callback, "v@:@^{opaqueCMSampleBuffer=}@");
  class_addIvar(cls, "_capture", sizeof(void*), (unsigned char)__builtin_ctzl(sizeof(void*)), "^v");
  objc_registerClassPair(cls);
  g_delegate_class = cls;
}

crtmedia_result crtmedia_capture_backend_enumerate(
    crtmedia_capture_device_info* devices, size_t capacity, size_t* out_count) {
  id pool = ((id (*)(id, SEL))objc_msgSend)(
      ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSAutoreleasePool"), sel_registerName("alloc")),
      sel_registerName("init"));
  id media_type_video = ((id (*)(id, SEL, const char*))objc_msgSend)(
      (id)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), "vide");
  id devices_array = ((id (*)(id, SEL, id))objc_msgSend)(
      (id)objc_getClass("AVCaptureDevice"), sel_registerName("devicesWithMediaType:"), media_type_video);
  NSUInteger count = ((NSUInteger (*)(id, SEL))objc_msgSend)(devices_array, sel_registerName("count"));
  NSUInteger index;
  size_t reported = 0;
  for (index = 0; index < count; ++index) {
    id device = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(devices_array, sel_registerName("objectAtIndex:"), index);
    id unique_id = ((id (*)(id, SEL))objc_msgSend)(device, sel_registerName("uniqueID"));
    id name = ((id (*)(id, SEL))objc_msgSend)(device, sel_registerName("localizedName"));
    const char* unique_id_str = ((const char* (*)(id, SEL))objc_msgSend)(unique_id, sel_registerName("UTF8String"));
    const char* name_str = ((const char* (*)(id, SEL))objc_msgSend)(name, sel_registerName("UTF8String"));
    if (reported < capacity) {
      snprintf(devices[reported].id, sizeof(devices[reported].id), "%s", unique_id_str != NULL ? unique_id_str : "");
      snprintf(devices[reported].name, sizeof(devices[reported].name), "%s", name_str != NULL ? name_str : "");
    }
    ++reported;
  }
  *out_count = reported;
  ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("release"));
  return CRTMEDIA_OK;
}

static void release_backend(crtmedia_avfoundation_capture* capture) {
  if (capture == NULL) {
    return;
  }
  if (capture->session != NULL) {
    if (capture->started) {
      ((void (*)(id, SEL))objc_msgSend)(capture->session, sel_registerName("stopRunning"));
      capture->started = 0;
    }
  }
  if (capture->output != NULL) {
    ((void (*)(id, SEL, id, void*))objc_msgSend)(
        capture->output, sel_registerName("setSampleBufferDelegate:queue:"), NULL, NULL);
  }
  if (capture->dispatch_queue != NULL) {
    dispatch_sync_f(capture->dispatch_queue, NULL, dispatch_barrier_noop);
  }
  if (capture->delegate != NULL) {
    object_setInstanceVariable(capture->delegate, "_capture", NULL);
  }
  pthread_mutex_lock(&capture->lock);
  queue_clear_locked(capture);
  pthread_mutex_unlock(&capture->lock);
  if (capture->session != NULL) {
    ((void (*)(id, SEL))objc_msgSend)(capture->session, sel_registerName("release"));
  }
  if (capture->output != NULL) {
    ((void (*)(id, SEL))objc_msgSend)(capture->output, sel_registerName("release"));
  }
  if (capture->delegate != NULL) {
    ((void (*)(id, SEL))objc_msgSend)(capture->delegate, sel_registerName("release"));
  }
  if (capture->dispatch_queue != NULL) {
    dispatch_release(capture->dispatch_queue);
  }
  pthread_cond_destroy(&capture->not_empty);
  pthread_mutex_destroy(&capture->lock);
  free(capture);
}

crtmedia_result crtmedia_capture_backend_open(
    const char* device_id, const crtmedia_capture_config* requested, void** out_backend,
    crtmedia_capture_config* out_actual) {
  static const struct {
    uint32_t width;
    uint32_t height;
  } presets[] = {
      {352, 288},
      {640, 480},
      {1280, 720},
      {1920, 1080},
  };
  uint32_t requested_width = requested != NULL && requested->width != 0 ? requested->width : 640u;
  uint32_t requested_height = requested != NULL && requested->height != 0 ? requested->height : 480u;
  uint32_t requested_rate = requested != NULL && requested->frame_rate != 0 ? requested->frame_rate : 30u;
  size_t preset_index = 1; /* 640x480 default */
  size_t candidate;
  crtmedia_avfoundation_capture* capture;
  id pool;
  id device;
  id input;
  id error = NULL;
  id session;
  id output;
  id delegate;
  id preset_string;
  id pixel_format_value;
  id width_value;
  id height_value;
  id video_settings;
  BOOL can_add;

  if (out_backend == NULL || out_actual == NULL ||
      (requested != NULL && requested->format != 0 && requested->format != CRTMEDIA_PIXEL_FORMAT_YUV420P)) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_backend = NULL;

  for (candidate = 0; candidate < sizeof(presets) / sizeof(presets[0]); ++candidate) {
    if (presets[candidate].width >= requested_width && presets[candidate].height >= requested_height) {
      preset_index = candidate;
      break;
    }
  }

  pthread_once(&g_delegate_class_once, register_delegate_class);
  if (g_delegate_class == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  capture = (crtmedia_avfoundation_capture*)calloc(1, sizeof(*capture));
  if (capture == NULL) {
    return CRTMEDIA_ERROR_IO;
  }
  pthread_mutex_init(&capture->lock, NULL);
  pthread_cond_init(&capture->not_empty, NULL);
  capture->last_timestamp_us = -1;

  pool = ((id (*)(id, SEL))objc_msgSend)(
      ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSAutoreleasePool"), sel_registerName("alloc")),
      sel_registerName("init"));

  device = ((id (*)(id, SEL, id))objc_msgSend)(
      (id)objc_getClass("AVCaptureDevice"), sel_registerName("deviceWithUniqueID:"),
      ((id (*)(id, SEL, const char*))objc_msgSend)(
          (id)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), device_id));
  if (device == NULL) {
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("release"));
    release_backend(capture);
    return CRTMEDIA_ERROR_IO;
  }

  input = ((id (*)(id, SEL, id, id*))objc_msgSend)(
      (id)objc_getClass("AVCaptureDeviceInput"), sel_registerName("deviceInputWithDevice:error:"), device, &error);
  if (input == NULL) {
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("release"));
    release_backend(capture);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }

  session = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("AVCaptureSession"), sel_registerName("alloc"));
  session = ((id (*)(id, SEL))objc_msgSend)(session, sel_registerName("init"));
  switch (preset_index) {
    case 0:
      preset_string = AVCaptureSessionPreset352x288;
      break;
    case 2:
      preset_string = AVCaptureSessionPreset1280x720;
      break;
    case 3:
      preset_string = AVCaptureSessionPreset1920x1080;
      break;
    default:
      preset_string = AVCaptureSessionPreset640x480;
      break;
  }
  if (!((BOOL (*)(id, SEL, id))objc_msgSend)(session, sel_registerName("canSetSessionPreset:"), preset_string)) {
    ((void (*)(id, SEL))objc_msgSend)(session, sel_registerName("release"));
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("release"));
    release_backend(capture);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  ((void (*)(id, SEL, id))objc_msgSend)(session, sel_registerName("setSessionPreset:"), preset_string);
  can_add = ((BOOL (*)(id, SEL, id))objc_msgSend)(session, sel_registerName("canAddInput:"), input);
  if (!can_add) {
    ((void (*)(id, SEL))objc_msgSend)(session, sel_registerName("release"));
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("release"));
    release_backend(capture);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  ((void (*)(id, SEL, id))objc_msgSend)(session, sel_registerName("addInput:"), input);

  output = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("AVCaptureVideoDataOutput"), sel_registerName("alloc"));
  output = ((id (*)(id, SEL))objc_msgSend)(output, sel_registerName("init"));
  pixel_format_value = ((id (*)(id, SEL, int))objc_msgSend)(
      (id)objc_getClass("NSNumber"), sel_registerName("numberWithInt:"), (int)CRT_CVPIXELFORMAT_420V);
  width_value = ((id (*)(id, SEL, unsigned int))objc_msgSend)(
      (id)objc_getClass("NSNumber"), sel_registerName("numberWithUnsignedInt:"), presets[preset_index].width);
  height_value = ((id (*)(id, SEL, unsigned int))objc_msgSend)(
      (id)objc_getClass("NSNumber"), sel_registerName("numberWithUnsignedInt:"), presets[preset_index].height);
  video_settings = ((id (*)(id, SEL))objc_msgSend)(
      (id)objc_getClass("NSMutableDictionary"), sel_registerName("dictionary"));
  ((void (*)(id, SEL, id, id))objc_msgSend)(
      video_settings, sel_registerName("setObject:forKey:"), pixel_format_value,
      (id)kCVPixelBufferPixelFormatTypeKey);
  ((void (*)(id, SEL, id, id))objc_msgSend)(
      video_settings, sel_registerName("setObject:forKey:"), width_value, (id)kCVPixelBufferWidthKey);
  ((void (*)(id, SEL, id, id))objc_msgSend)(
      video_settings, sel_registerName("setObject:forKey:"), height_value, (id)kCVPixelBufferHeightKey);
  ((void (*)(id, SEL, id))objc_msgSend)(output, sel_registerName("setVideoSettings:"), video_settings);
  ((void (*)(id, SEL, BOOL))objc_msgSend)(output, sel_registerName("setAlwaysDiscardsLateVideoFrames:"), (BOOL)1);

  delegate = ((id (*)(id, SEL))objc_msgSend)((id)g_delegate_class, sel_registerName("alloc"));
  delegate = ((id (*)(id, SEL))objc_msgSend)(delegate, sel_registerName("init"));
  object_setInstanceVariable(delegate, "_capture", capture);

  capture->dispatch_queue = dispatch_queue_create("crtmedia.capture.avfoundation", NULL);
  ((void (*)(id, SEL, id, void*))objc_msgSend)(
      output, sel_registerName("setSampleBufferDelegate:queue:"), delegate, capture->dispatch_queue);

  capture->session = session;
  capture->output = output;
  capture->delegate = delegate;

  can_add = ((BOOL (*)(id, SEL, id))objc_msgSend)(session, sel_registerName("canAddOutput:"), output);
  if (!can_add) {
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("release"));
    release_backend(capture);
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  ((void (*)(id, SEL, id))objc_msgSend)(session, sel_registerName("addOutput:"), output);

  ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("release"));

  out_actual->width = presets[preset_index].width;
  out_actual->height = presets[preset_index].height;
  out_actual->frame_rate = requested_rate; /* best-effort echo -- see this file's own top comment */
  out_actual->format = CRTMEDIA_PIXEL_FORMAT_YUV420P;
  *out_backend = capture;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_capture_backend_start(void* backend) {
  crtmedia_avfoundation_capture* capture = (crtmedia_avfoundation_capture*)backend;
  if (capture->started) {
    return CRTMEDIA_OK;
  }
  pthread_mutex_lock(&capture->lock);
  queue_clear_locked(capture);
  capture->have_timestamp_origin = 0;
  capture->last_timestamp_us = -1;
  pthread_mutex_unlock(&capture->lock);
  ((void (*)(id, SEL))objc_msgSend)(capture->session, sel_registerName("startRunning"));
  capture->started = 1;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_capture_backend_dequeue(void* backend, int timeout_ms, crtmedia_frame* out_frame) {
  crtmedia_avfoundation_capture* capture = (crtmedia_avfoundation_capture*)backend;
  crtmedia_avfoundation_queued_frame queued;
  int wait_result = 0;
  if (!capture->started) {
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
      break; /* one bounded wait attempt, matching V4L2's own poll(timeout_ms) shape */
    }
  }
  if (capture->queue_count == 0) {
    pthread_mutex_unlock(&capture->lock);
    return wait_result == 0 ? CRTMEDIA_WOULD_BLOCK : CRTMEDIA_WOULD_BLOCK;
  }
  (void)queue_take_locked(capture, &queued);
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
  crtmedia_avfoundation_capture* capture = (crtmedia_avfoundation_capture*)backend;
  if (!capture->started) {
    return CRTMEDIA_OK;
  }
  ((void (*)(id, SEL))objc_msgSend)(capture->session, sel_registerName("stopRunning"));
  capture->started = 0;
  if (capture->dispatch_queue != NULL) {
    dispatch_sync_f(capture->dispatch_queue, NULL, dispatch_barrier_noop);
  }
  pthread_mutex_lock(&capture->lock);
  queue_clear_locked(capture);
  pthread_mutex_unlock(&capture->lock);
  return CRTMEDIA_OK;
}

void crtmedia_capture_backend_release(void* backend) {
  release_backend((crtmedia_avfoundation_capture*)backend);
}
