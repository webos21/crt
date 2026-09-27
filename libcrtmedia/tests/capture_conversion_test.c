#include "capture_v4l2_test_control.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                                         \
  do {                                                                    \
    if (!(condition)) {                                                   \
      fprintf(stderr, "crtmedia_capture_conversion_test: %s\n", message); \
      return 1;                                                           \
    }                                                                     \
  } while (0)

static int check_plane(const crtmedia_frame_plane* plane,
                       const unsigned char* expected, size_t size) {
  return plane->data != NULL && memcmp(plane->data, expected, size) == 0;
}

int main(void) {
  const unsigned char expected_y[8] = {10, 11, 12, 13, 20, 21, 22, 23};
  const unsigned char expected_u[2] = {31, 32};
  const unsigned char expected_v[2] = {41, 42};
  crtmedia_frame frame;

  const unsigned char yuv420[12] = {
      10, 11, 12, 13, 20, 21, 22, 23, 31, 32, 41, 42};
  CHECK(crtmedia_v4l2_convert_to_yuv420p(
            yuv420, sizeof(yuv420), CRTMEDIA_V4L2_SOURCE_YUV420,
            4, 2, 4, &frame) == CRTMEDIA_OK, "convert YUV420");
  CHECK(check_plane(&frame.planes[0], expected_y, sizeof(expected_y)), "YUV420 Y");
  CHECK(check_plane(&frame.planes[1], expected_u, sizeof(expected_u)), "YUV420 U");
  CHECK(check_plane(&frame.planes[2], expected_v, sizeof(expected_v)), "YUV420 V");
  crtmedia_frame_release(&frame);

  const unsigned char nv12[12] = {
      10, 11, 12, 13, 20, 21, 22, 23, 31, 41, 32, 42};
  CHECK(crtmedia_v4l2_convert_to_yuv420p(
            nv12, sizeof(nv12), CRTMEDIA_V4L2_SOURCE_NV12,
            4, 2, 4, &frame) == CRTMEDIA_OK, "convert NV12");
  CHECK(check_plane(&frame.planes[0], expected_y, sizeof(expected_y)), "NV12 Y");
  CHECK(check_plane(&frame.planes[1], expected_u, sizeof(expected_u)), "NV12 U");
  CHECK(check_plane(&frame.planes[2], expected_v, sizeof(expected_v)), "NV12 V");
  crtmedia_frame_release(&frame);

  /* Row 0 has U/V 30/40 and 32/42; row 1 has 32/42 and 32/42. Vertical
   * averaging therefore yields the same expected 31/41, 32/42 planes. */
  const unsigned char yuyv[16] = {
      10, 30, 11, 40, 12, 32, 13, 42,
      20, 32, 21, 42, 22, 32, 23, 42};
  CHECK(crtmedia_v4l2_convert_to_yuv420p(
            yuyv, sizeof(yuyv), CRTMEDIA_V4L2_SOURCE_YUYV,
            4, 2, 8, &frame) == CRTMEDIA_OK, "convert YUYV");
  CHECK(check_plane(&frame.planes[0], expected_y, sizeof(expected_y)), "YUYV Y");
  CHECK(check_plane(&frame.planes[1], expected_u, sizeof(expected_u)), "YUYV U");
  CHECK(check_plane(&frame.planes[2], expected_v, sizeof(expected_v)), "YUYV V");
  CHECK(frame.release != NULL && frame.release_context != NULL, "owned output");
  crtmedia_frame_release(&frame);

  printf("crtmedia_capture_conversion_test: ok formats=3\n");
  return 0;
}
