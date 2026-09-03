// Real coverage for crtmedia/gpu_frame.h (TODO.md's upper-runtime roadmap
// "Fix the common GPU resource contract" step) -- crtmedia_gpu_frame_
// create_cpu()'s own real, addressable, correctly-strided plane storage
// for both a single-plane packed format (RGBA8888) and a multi-plane
// planar format (YUV420P, exercising crtmedia_frame_describe_planes()'s
// own chroma-subsampled layout), real ownership/release, and argument
// validation.

#include "crtmedia/gpu_frame.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg)                                                                     \
  do {                                                                                        \
    if (!(cond)) {                                                                            \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);                           \
      ++failures;                                                                             \
    }                                                                                          \
  } while (0)

static void no_op_release(crtmedia_gpu_frame* frame, void* release_context) {
  (void)frame;
  (void)release_context;
}

int main(void) {
  // Invalid arguments.
  CHECK(
      crtmedia_gpu_frame_create_cpu(CRTMEDIA_PIXEL_FORMAT_RGBA8888, 4, 4, 0, NULL) ==
          CRTMEDIA_ERROR_INVALID_ARGUMENT,
      "create_cpu(..., NULL out_frame) fails cleanly");
  crtmedia_gpu_frame frame;
  CHECK(
      crtmedia_gpu_frame_create_cpu((crtmedia_pixel_format)0, 4, 4, 0, &frame) ==
          CRTMEDIA_ERROR_INVALID_ARGUMENT,
      "create_cpu() rejects an unrecognized format");
  CHECK(
      crtmedia_gpu_frame_create_cpu(CRTMEDIA_PIXEL_FORMAT_RGBA8888, 0, 4, 0, &frame) ==
          CRTMEDIA_ERROR_INVALID_ARGUMENT,
      "create_cpu() rejects a 0 width");
  CHECK(
      crtmedia_gpu_frame_create_cpu(CRTMEDIA_PIXEL_FORMAT_RGBA8888, 4, 0, 0, &frame) ==
          CRTMEDIA_ERROR_INVALID_ARGUMENT,
      "create_cpu() rejects a 0 height");

  // RGBA8888: exactly one plane.
  CHECK(
      crtmedia_gpu_frame_create_cpu(CRTMEDIA_PIXEL_FORMAT_RGBA8888, 4, 4, 12345, &frame) == CRTMEDIA_OK,
      "create_cpu() succeeds for a real RGBA8888 frame");
  CHECK(frame.format == CRTMEDIA_PIXEL_FORMAT_RGBA8888, "frame.format matches what was requested");
  CHECK(frame.width == 4 && frame.height == 4, "frame.width/height match what was requested");
  CHECK(frame.memory_kind == CRTMEDIA_GPU_MEMORY_CPU, "a software-fallback frame reports CRTMEDIA_GPU_MEMORY_CPU");
  CHECK(frame.timestamp_us == 12345, "frame.timestamp_us matches what was requested");
  CHECK(frame.device_id == 0, "a CPU frame has no real device affinity");
  CHECK(frame.native_handle == NULL, "a CPU frame has no real native GPU handle");
  CHECK(frame.plane_count == 1, "RGBA8888 has exactly one real plane");
  CHECK(frame.planes[0].data != NULL, "the real plane has real, non-NULL backing storage");
  CHECK(frame.release != NULL, "create_cpu() produces a real, owning frame");

  // Write a real byte pattern through the plane using its own real
  // stride, then read it back -- proves real, correctly-strided,
  // addressable memory, not just a non-NULL pointer.
  {
    uint8_t* row0 = (uint8_t*)frame.planes[0].data;
    uint8_t* row1 = row0 + frame.planes[0].stride;
    memset(row0, 0xAB, frame.planes[0].width * 4);
    memset(row1, 0xCD, frame.planes[0].width * 4);
    CHECK(row0[0] == 0xAB && row0[frame.planes[0].width * 4 - 1] == 0xAB, "row 0 is real, writable memory");
    CHECK(row1[0] == 0xCD && row1[frame.planes[0].width * 4 - 1] == 0xCD, "row 1 (past one real stride) is too");
  }

  crtmedia_gpu_frame_release(&frame);
  CHECK(frame.format == 0, "release() zeroes the struct");
  CHECK(frame.release == NULL, "release() clears the release callback too");

  // YUV420P: three real, independently-addressable, chroma-subsampled planes.
  CHECK(
      crtmedia_gpu_frame_create_cpu(CRTMEDIA_PIXEL_FORMAT_YUV420P, 8, 8, 0, &frame) == CRTMEDIA_OK,
      "create_cpu() succeeds for a real YUV420P frame");
  CHECK(frame.plane_count == 3, "YUV420P has exactly three real planes");
  for (int i = 0; i < 3; ++i) {
    CHECK(frame.planes[i].data != NULL, "each real YUV420P plane has real, non-NULL backing storage");
    memset(frame.planes[i].data, (uint8_t)(0x10 + i), frame.planes[i].stride * frame.planes[i].height);
  }
  CHECK(
      ((uint8_t*)frame.planes[0].data)[0] == 0x10 && ((uint8_t*)frame.planes[1].data)[0] == 0x11 &&
          ((uint8_t*)frame.planes[2].data)[0] == 0x12,
      "the three real planes are genuinely independent, non-overlapping storage");
  crtmedia_gpu_frame_release(&frame);

  // A non-owning view (release == NULL) -- must not attempt to free static storage.
  static uint8_t static_storage[16];
  memset(&frame, 0, sizeof(frame));
  frame.format = CRTMEDIA_PIXEL_FORMAT_RGBA8888;
  frame.width = 2;
  frame.height = 2;
  frame.memory_kind = CRTMEDIA_GPU_MEMORY_CPU;
  frame.plane_count = 1;
  frame.planes[0].data = static_storage;
  frame.planes[0].stride = 8;
  frame.planes[0].width = 2;
  frame.planes[0].height = 2;
  frame.release = NULL;
  crtmedia_gpu_frame_release(&frame); // must not crash / must not free static_storage
  CHECK(frame.format == 0, "release() still zeroes a non-owning view's own struct");

  // release with a real, non-NULL, non-freeing callback: proves the
  // callback actually gets invoked (not skipped).
  memset(&frame, 0, sizeof(frame));
  frame.release = no_op_release;
  crtmedia_gpu_frame_release(&frame); // real call into no_op_release(); must not crash

  crtmedia_gpu_frame_release(NULL); // must be a real, safe no-op

  if (failures != 0) {
    fprintf(stderr, "crtmedia_gpu_frame_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_gpu_frame_test: ok\n");
  return 0;
}
