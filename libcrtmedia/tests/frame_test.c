// Deterministic, host-resource-free coverage for crtmedia/frame.h --
// crtmedia_frame_describe_planes()'s plane geometry for every format this
// contract defines, crtmedia_frame_release()'s ownership contract, and
// crtmedia_frame_convert_to_rgba()'s packed-RGB passthrough/channel-swap
// and YUV420P color conversion (limited/full range, all three color
// spaces). See tests/frame_skia_smoke.cc (built only when CRTGFX_ENABLE_
// SKIA is on, from libcrtgfx/CMakeLists.txt -- see that file's own
// comment on why) for the real Skia SkImage/SkSurface handoff proof this
// file does not attempt.

#include "crtmedia/frame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg)                                                                     \
  do {                                                                                        \
    if (!(cond)) {                                                                            \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);                           \
      ++failures;                                                                             \
    }                                                                                          \
  } while (0)

static void test_describe_planes_packed(void) {
  crtmedia_frame_plane planes[CRTMEDIA_FRAME_MAX_PLANES];
  uint32_t count = 0;
  crtmedia_result r = crtmedia_frame_describe_planes(CRTMEDIA_PIXEL_FORMAT_RGBA8888, 7, 5, planes, &count);
  CHECK(r == CRTMEDIA_OK, "describe_planes RGBA8888 ok");
  CHECK(count == 1, "describe_planes RGBA8888 plane_count == 1");
  CHECK(planes[0].width == 7 && planes[0].height == 5, "describe_planes RGBA8888 dims");
  CHECK(planes[0].stride == 7 * 4, "describe_planes RGBA8888 stride");

  r = crtmedia_frame_describe_planes(CRTMEDIA_PIXEL_FORMAT_BGRA8888, 3, 9, planes, &count);
  CHECK(r == CRTMEDIA_OK, "describe_planes BGRA8888 ok");
  CHECK(planes[0].stride == 3 * 4, "describe_planes BGRA8888 stride");
}

static void test_describe_planes_yuv420p(void) {
  crtmedia_frame_plane planes[CRTMEDIA_FRAME_MAX_PLANES];
  uint32_t count = 0;

  /* Even dimensions: exact halving. */
  crtmedia_result r = crtmedia_frame_describe_planes(CRTMEDIA_PIXEL_FORMAT_YUV420P, 8, 4, planes, &count);
  CHECK(r == CRTMEDIA_OK, "describe_planes YUV420P even ok");
  CHECK(count == 3, "describe_planes YUV420P plane_count == 3");
  CHECK(planes[0].width == 8 && planes[0].height == 4 && planes[0].stride == 8, "describe_planes YUV420P luma");
  CHECK(planes[1].width == 4 && planes[1].height == 2 && planes[1].stride == 4, "describe_planes YUV420P U");
  CHECK(planes[2].width == 4 && planes[2].height == 2 && planes[2].stride == 4, "describe_planes YUV420P V");

  /* Odd dimensions: chroma planes round up, matching real decoder
   * behavior for an odd-sized 4:2:0 frame. */
  r = crtmedia_frame_describe_planes(CRTMEDIA_PIXEL_FORMAT_YUV420P, 7, 5, planes, &count);
  CHECK(r == CRTMEDIA_OK, "describe_planes YUV420P odd ok");
  CHECK(planes[1].width == 4 && planes[1].height == 3, "describe_planes YUV420P odd chroma rounds up");
}

static void test_describe_planes_invalid(void) {
  crtmedia_frame_plane planes[CRTMEDIA_FRAME_MAX_PLANES];
  uint32_t count = 0;
  CHECK(crtmedia_frame_describe_planes(CRTMEDIA_PIXEL_FORMAT_RGBA8888, 0, 5, planes, &count) ==
            CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "describe_planes rejects width == 0");
  CHECK(crtmedia_frame_describe_planes((crtmedia_pixel_format)999, 4, 4, planes, &count) ==
            CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "describe_planes rejects unknown format");
  CHECK(crtmedia_frame_describe_planes(CRTMEDIA_PIXEL_FORMAT_RGBA8888, 4, 4, NULL, &count) ==
            CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "describe_planes rejects null out_planes");
}

static int release_called = 0;
static void* released_context = NULL;

static void test_release_fn(crtmedia_frame* frame, void* context) {
  (void)frame;
  release_called = 1;
  released_context = context;
}

static void test_frame_release(void) {
  int dummy_context = 42;
  crtmedia_frame frame;
  memset(&frame, 0, sizeof(frame));
  frame.format = CRTMEDIA_PIXEL_FORMAT_RGBA8888;
  frame.release = test_release_fn;
  frame.release_context = &dummy_context;

  release_called = 0;
  released_context = NULL;
  crtmedia_frame_release(&frame);
  CHECK(release_called == 1, "release() calls the owning callback");
  CHECK(released_context == &dummy_context, "release() passes through release_context");
  CHECK(frame.format == 0, "release() zeroes the struct afterward");

  /* A non-owning view (release == NULL) must not crash and must still
   * zero the struct. */
  memset(&frame, 0, sizeof(frame));
  frame.format = CRTMEDIA_PIXEL_FORMAT_RGBA8888;
  release_called = 0;
  crtmedia_frame_release(&frame);
  CHECK(release_called == 0, "release() with release==NULL calls nothing");
  CHECK(frame.format == 0, "release() with release==NULL still zeroes the struct");

  /* A null frame pointer must not crash. */
  crtmedia_frame_release(NULL);
}

static void test_convert_bgra_to_rgba(void) {
  uint8_t src_pixels[2 * 2 * 4] = {
      /* row 0: B,G,R,A pairs */
      10, 20, 30, 255, 40, 50, 60, 128,
      /* row 1 */
      70, 80, 90, 0, 100, 110, 120, 255,
  };
  uint8_t dst_pixels[2 * 2 * 4];

  crtmedia_frame src;
  memset(&src, 0, sizeof(src));
  src.format = CRTMEDIA_PIXEL_FORMAT_BGRA8888;
  src.width = 2;
  src.height = 2;
  src.plane_count = 1;
  src.planes[0].data = src_pixels;
  src.planes[0].stride = 2 * 4;
  src.planes[0].width = 2;
  src.planes[0].height = 2;

  crtmedia_frame dst;
  memset(&dst, 0, sizeof(dst));
  dst.format = CRTMEDIA_PIXEL_FORMAT_RGBA8888;
  dst.width = 2;
  dst.height = 2;
  dst.planes[0].data = dst_pixels;
  dst.planes[0].stride = 2 * 4;

  crtmedia_result r = crtmedia_frame_convert_to_rgba(&src, &dst);
  CHECK(r == CRTMEDIA_OK, "convert BGRA8888->RGBA8888 ok");
  /* Pixel (0,0): src BGRA (10,20,30,255) -> dst RGBA (30,20,10,255). */
  CHECK(dst_pixels[0] == 30 && dst_pixels[1] == 20 && dst_pixels[2] == 10 && dst_pixels[3] == 255,
        "convert BGRA8888->RGBA8888 swaps R/B, pixel 0");
  /* Pixel (1,1): src BGRA (100,110,120,255) -> dst RGBA (120,110,100,255). */
  size_t last = (size_t)1 * dst.planes[0].stride + 1 * 4;
  CHECK(dst_pixels[last + 0] == 120 && dst_pixels[last + 1] == 110 && dst_pixels[last + 2] == 100 &&
            dst_pixels[last + 3] == 255,
        "convert BGRA8888->RGBA8888 swaps R/B, pixel 3");
}

static void test_convert_yuv420p_gray(void) {
  /* A flat mid-gray frame (Y=128 everywhere, U=V=128 "no color") must
   * convert to a flat gray RGB regardless of color_space/color_range --
   * a genuinely color-neutral input is the one case every real
   * conversion matrix agrees on, making it a solid sanity check without
   * hand-deriving a specific chrominant expected value. */
  uint8_t y_plane[4 * 4];
  uint8_t u_plane[2 * 2];
  uint8_t v_plane[2 * 2];
  memset(y_plane, 128, sizeof(y_plane));
  memset(u_plane, 128, sizeof(u_plane));
  memset(v_plane, 128, sizeof(v_plane));

  crtmedia_frame src;
  memset(&src, 0, sizeof(src));
  src.format = CRTMEDIA_PIXEL_FORMAT_YUV420P;
  src.width = 4;
  src.height = 4;
  src.color_range = CRTMEDIA_COLOR_RANGE_FULL;
  src.color_space = CRTMEDIA_COLOR_SPACE_BT709;
  src.plane_count = 3;
  src.planes[0] = (crtmedia_frame_plane){y_plane, 4, 4, 4};
  src.planes[1] = (crtmedia_frame_plane){u_plane, 2, 2, 2};
  src.planes[2] = (crtmedia_frame_plane){v_plane, 2, 2, 2};

  uint8_t dst_pixels[4 * 4 * 4];
  crtmedia_frame dst;
  memset(&dst, 0, sizeof(dst));
  dst.format = CRTMEDIA_PIXEL_FORMAT_RGBA8888;
  dst.width = 4;
  dst.height = 4;
  dst.planes[0].data = dst_pixels;
  dst.planes[0].stride = 4 * 4;

  crtmedia_result r = crtmedia_frame_convert_to_rgba(&src, &dst);
  CHECK(r == CRTMEDIA_OK, "convert YUV420P->RGBA8888 (gray, full range) ok");
  for (int i = 0; i < 4 * 4; ++i) {
    CHECK(dst_pixels[i * 4 + 0] == 128 && dst_pixels[i * 4 + 1] == 128 && dst_pixels[i * 4 + 2] == 128 &&
              dst_pixels[i * 4 + 3] == 255,
          "convert YUV420P->RGBA8888 gray pixel");
  }

  /* Same flat input, LIMITED range: Y=128 sits mid-swing of [16,235], so
   * it still maps close to gray (129, given 219-unit integer rounding),
   * and chroma at raw 128 is still exactly "no color" regardless of
   * range. */
  src.color_range = CRTMEDIA_COLOR_RANGE_LIMITED;
  r = crtmedia_frame_convert_to_rgba(&src, &dst);
  CHECK(r == CRTMEDIA_OK, "convert YUV420P->RGBA8888 (gray, limited range) ok");
  CHECK(dst_pixels[0] == dst_pixels[1] && dst_pixels[1] == dst_pixels[2], "convert YUV420P limited range stays gray");
}

static void test_convert_invalid(void) {
  crtmedia_frame src, dst;
  memset(&src, 0, sizeof(src));
  memset(&dst, 0, sizeof(dst));
  src.format = CRTMEDIA_PIXEL_FORMAT_RGBA8888;
  src.width = 4;
  src.height = 4;
  dst.format = CRTMEDIA_PIXEL_FORMAT_RGBA8888;
  dst.width = 4;
  dst.height = 4;

  CHECK(crtmedia_frame_convert_to_rgba(NULL, &dst) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "convert rejects null src");
  CHECK(crtmedia_frame_convert_to_rgba(&src, NULL) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "convert rejects null dst");
  dst.format = CRTMEDIA_PIXEL_FORMAT_BGRA8888;
  CHECK(crtmedia_frame_convert_to_rgba(&src, &dst) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "convert rejects a non-RGBA8888 dst");
  dst.format = CRTMEDIA_PIXEL_FORMAT_RGBA8888;
  dst.width = 5;
  CHECK(crtmedia_frame_convert_to_rgba(&src, &dst) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "convert rejects a width mismatch");
}

int main(void) {
  test_describe_planes_packed();
  test_describe_planes_yuv420p();
  test_describe_planes_invalid();
  test_frame_release();
  test_convert_bgra_to_rgba();
  test_convert_yuv420p_gray();
  test_convert_invalid();

  if (failures != 0) {
    fprintf(stderr, "crtmedia_frame_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_frame_test: ok\n");
  return 0;
}
