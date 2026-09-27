/* Resource-free coverage for "Encode and capture" Tranche 4's macOS
 * conversion seam (capture_avfoundation_test_control.h) -- mirrors
 * capture_conversion_test.c's own role for the Linux V4L2 backend, byte-
 * exact against a deterministic synthetic 4x2 NV12 image. Also exercises a
 * padded stride (source rows wider than the real image, both for Y and for
 * the interleaved UV plane) -- a real CVPixelBuffer's own bytesPerRow is
 * commonly larger than width*bytesPerPixel (row alignment/padding), unlike
 * a tightly packed test buffer, so this is not a redundant case. */
#include "capture_avfoundation_test_control.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                                              \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "crtmedia_capture_avfoundation_conversion_test: %s\n", message); \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int check_plane(const crtmedia_frame_plane* plane, const unsigned char* expected, size_t size) {
  return plane->data != NULL && memcmp(plane->data, expected, size) == 0;
}

int main(void) {
  const unsigned char expected_y[8] = {10, 11, 12, 13, 20, 21, 22, 23};
  const unsigned char expected_u[2] = {31, 32};
  const unsigned char expected_v[2] = {41, 42};
  crtmedia_frame frame;

  /* Tightly packed: y_stride == width (4), uv_stride == chroma_width*2 (4). */
  const unsigned char y_tight[8] = {10, 11, 12, 13, 20, 21, 22, 23};
  const unsigned char uv_tight[4] = {31, 41, 32, 42};
  CHECK(crtmedia_avfoundation_convert_nv12_to_yuv420p(y_tight, 4, uv_tight, 4, 4, 2, &frame) == CRTMEDIA_OK,
        "convert tightly packed NV12");
  CHECK(check_plane(&frame.planes[0], expected_y, sizeof(expected_y)), "tight Y");
  CHECK(check_plane(&frame.planes[1], expected_u, sizeof(expected_u)), "tight U");
  CHECK(check_plane(&frame.planes[2], expected_v, sizeof(expected_v)), "tight V");
  CHECK(frame.format == CRTMEDIA_PIXEL_FORMAT_YUV420P && frame.width == 4 && frame.height == 2, "tight geometry");
  CHECK(frame.plane_count == 3 && frame.release != NULL && frame.release_context != NULL, "tight ownership");
  crtmedia_frame_release(&frame);
  CHECK(frame.release == NULL && frame.format == 0, "release zeroes the frame");

  /* Padded rows: y_stride=6 (2 bytes of real per-row padding), uv_stride=8
   * (4 bytes of padding) -- the real CVPixelBufferGetBytesPerRowOfPlane()
   * case this function exists to handle correctly. */
  const unsigned char y_padded[12] = {10, 11, 12, 13, 0xEE, 0xEE, 20, 21, 22, 23, 0xEE, 0xEE};
  const unsigned char uv_padded[8] = {31, 41, 32, 42, 0xEE, 0xEE, 0xEE, 0xEE};
  CHECK(crtmedia_avfoundation_convert_nv12_to_yuv420p(y_padded, 6, uv_padded, 8, 4, 2, &frame) == CRTMEDIA_OK,
        "convert padded-stride NV12");
  CHECK(check_plane(&frame.planes[0], expected_y, sizeof(expected_y)), "padded Y");
  CHECK(check_plane(&frame.planes[1], expected_u, sizeof(expected_u)), "padded U");
  CHECK(check_plane(&frame.planes[2], expected_v, sizeof(expected_v)), "padded V");
  crtmedia_frame_release(&frame);

  /* Real, rejected misuse: a stride narrower than the real image content
   * must fail rather than read out of bounds. */
  CHECK(crtmedia_avfoundation_convert_nv12_to_yuv420p(y_tight, 2, uv_tight, 4, 4, 2, &frame) ==
            CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "reject a Y stride narrower than width");
  CHECK(crtmedia_avfoundation_convert_nv12_to_yuv420p(NULL, 4, uv_tight, 4, 4, 2, &frame) ==
            CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "reject a null Y plane");
  CHECK(crtmedia_avfoundation_convert_nv12_to_yuv420p(y_tight, 4, uv_tight, 4, 0, 2, &frame) ==
            CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "reject a zero width");

  printf("crtmedia_capture_avfoundation_conversion_test: ok\n");
  return 0;
}
