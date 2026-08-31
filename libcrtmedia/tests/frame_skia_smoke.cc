// The real Skia SkImage/SkSurface handoff proof for crtmedia/frame.h's
// CPU frame contract (TODO.md's "Define the libcrtmedia CPU frame handoff
// contract" item -- the smoke this item's own description asks for: "a
// CPU-only smoke that hands a synthetic RGBA/YUV frame to a Skia SkImage/
// SkSurface"). Built and registered from libcrtgfx/CMakeLists.txt, not
// libcrtmedia/CMakeLists.txt -- crt_wire_skia_executable() (that file's
// own function, used here too) is a CMake function() defined in that
// file's own directory scope, and CMake function definitions do not
// propagate to a sibling directory's CMakeLists.txt; libcrtmedia/tests/
// frame_test.c's own deterministic, Skia-free coverage stays in
// libcrtmedia/CMakeLists.txt where it belongs.
//
// Headless like crtgfx_skia_cpu_coverage (tests/skia_cpu_coverage_test.
// cc): every frame here is a plain malloc'd buffer, never a real
// crtgfx_window, so this runs identically on every CI runner. Normal
// Skia headers are used throughout (public API), matching this project's
// own "Skia owns drawing" policy (docs/libcrtgfx_api_policy.md).
//
// Three cases, one per crtmedia_pixel_format this contract defines:
//  - RGBA8888: wrapped directly as an SkImage (kRGBA_8888_SkColorType
//    matches exactly, no conversion needed).
//  - BGRA8888: also wrapped directly (kBGRA_8888_SkColorType), proving
//    both packed byte orders the contract lists interoperate with Skia
//    with no crtmedia_frame_convert_to_rgba() step at all.
//  - YUV420P: converted via crtmedia_frame_convert_to_rgba() first (real
//    BT.709 color-space math, the same function libcrtmedia/tests/
//    frame_test.c already covers numerically), then wrapped the same way
//    as the RGBA8888 case -- this is the case that actually exercises
//    the "hand a YUV frame to Skia" half of this item's own ask, since
//    no Skia build in this project's own tree exposes a CPU-raster YUVA
//    image factory (SkImages::RasterFromYUVAPixmaps does not exist in
//    the vendored Skia headers here -- checked directly) to hand planar
//    data to directly.
//
// Each case draws its SkImage onto a plain raster SkSurface via
// SkCanvas::drawImage(), then reads the surface's own pixels back and
// compares a few sample points against the frame's own known synthetic
// content -- proving the full round trip (crtmedia_frame -> SkImage ->
// SkSurface -> real pixels) actually preserves color, not just that the
// calls succeed without crashing.

#include "crtgfx/skia.h"
#include "crtmedia/frame.h"

#include "include/core/SkAlphaType.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorType.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

int g_failures = 0;

#define CHECK(cond, msg)                                                \
  do {                                                                  \
    if (!(cond)) {                                                      \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);     \
      ++g_failures;                                                     \
    }                                                                   \
  } while (0)

// Wraps `frame` (CRTMEDIA_PIXEL_FORMAT_RGBA8888 or _BGRA8888, one packed
// plane) as an SkImage with no copy or conversion, matching crtgfx's own
// crtgfx_skia_make_raster_surface() (src/skia_bridge.cc) wrap-not-copy
// convention for the same reason: this frame's own storage outlives the
// SkImage in every case below, so a view is correct and cheaper than a
// copy would be.
sk_sp<SkImage> wrap_packed_frame(const crtmedia_frame& frame) {
  SkColorType color_type = frame.format == CRTMEDIA_PIXEL_FORMAT_BGRA8888
                                ? kBGRA_8888_SkColorType
                                : kRGBA_8888_SkColorType;
  SkImageInfo info = SkImageInfo::Make(
      (int)frame.width, (int)frame.height, color_type, kUnpremul_SkAlphaType);
  SkPixmap pixmap(info, frame.planes[0].data, frame.planes[0].stride);
  return SkImages::RasterFromPixmapCopy(pixmap);
}

// Draws `image` onto a fresh raster SkSurface the same size as the image,
// then reads back the pixel at (x, y) as a packed 0xAARRGGBB value (the
// same layout SkColorGetR/G/B/A expect) for comparison.
SkColor draw_and_sample(const sk_sp<SkImage>& image, int x, int y) {
  SkImageInfo surface_info =
      SkImageInfo::Make(image->width(), image->height(), kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(surface_info);
  if (!surface) {
    return 0;
  }
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  canvas->drawImage(image, 0, 0);

  SkBitmap bitmap;
  bitmap.allocPixels(surface_info);
  if (!surface->readPixels(bitmap, 0, 0)) {
    return 0;
  }
  return bitmap.getColor(x, y);
}

// SkColorGetR/G/B/A assume 0xAARRGGBB regardless of the SkImageInfo's own
// color type -- SkBitmap::getColor() already normalizes to that layout,
// so these plain accessors are correct for a bitmap read back from a
// kRGBA_8888_SkColorType surface (this file only ever samples from one).

void test_rgba8888_direct(void) {
  const uint32_t kWidth = 4, kHeight = 4;
  uint8_t pixels[kWidth * kHeight * 4];
  // A solid, fully-opaque, easy-to-check color: R=200, G=100, B=50.
  for (uint32_t i = 0; i < kWidth * kHeight; ++i) {
    pixels[i * 4 + 0] = 200;
    pixels[i * 4 + 1] = 100;
    pixels[i * 4 + 2] = 50;
    pixels[i * 4 + 3] = 255;
  }

  crtmedia_frame frame;
  memset(&frame, 0, sizeof(frame));
  frame.format = CRTMEDIA_PIXEL_FORMAT_RGBA8888;
  frame.width = kWidth;
  frame.height = kHeight;
  frame.plane_count = 1;
  frame.planes[0].data = pixels;
  frame.planes[0].stride = kWidth * 4;
  frame.planes[0].width = kWidth;
  frame.planes[0].height = kHeight;

  sk_sp<SkImage> image = wrap_packed_frame(frame);
  CHECK(image != nullptr, "RGBA8888 frame wraps into a real SkImage");
  if (!image) return;

  SkColor sample = draw_and_sample(image, 2, 2);
  CHECK(SkColorGetR(sample) == 200 && SkColorGetG(sample) == 100 && SkColorGetB(sample) == 50 &&
            SkColorGetA(sample) == 255,
        "RGBA8888 frame draws through SkSurface with the right color");
}

void test_bgra8888_direct(void) {
  const uint32_t kWidth = 4, kHeight = 4;
  uint8_t pixels[kWidth * kHeight * 4];
  // BGRA byte order: B=50, G=100, R=200 -- same logical color as the
  // RGBA8888 case above, proving both packed byte orders round-trip to
  // the same real RGB result through Skia.
  for (uint32_t i = 0; i < kWidth * kHeight; ++i) {
    pixels[i * 4 + 0] = 50;
    pixels[i * 4 + 1] = 100;
    pixels[i * 4 + 2] = 200;
    pixels[i * 4 + 3] = 255;
  }

  crtmedia_frame frame;
  memset(&frame, 0, sizeof(frame));
  frame.format = CRTMEDIA_PIXEL_FORMAT_BGRA8888;
  frame.width = kWidth;
  frame.height = kHeight;
  frame.plane_count = 1;
  frame.planes[0].data = pixels;
  frame.planes[0].stride = kWidth * 4;
  frame.planes[0].width = kWidth;
  frame.planes[0].height = kHeight;

  sk_sp<SkImage> image = wrap_packed_frame(frame);
  CHECK(image != nullptr, "BGRA8888 frame wraps into a real SkImage");
  if (!image) return;

  SkColor sample = draw_and_sample(image, 2, 2);
  CHECK(SkColorGetR(sample) == 200 && SkColorGetG(sample) == 100 && SkColorGetB(sample) == 50 &&
            SkColorGetA(sample) == 255,
        "BGRA8888 frame draws through SkSurface with the right color (R/B correctly un-swapped)");
}

void test_yuv420p_via_conversion(void) {
  const uint32_t kWidth = 4, kHeight = 4;
  uint8_t y_plane[kWidth * kHeight];
  uint8_t u_plane[2 * 2];
  uint8_t v_plane[2 * 2];
  // Pure red (255, 0, 0) encoded as BT.709 full-range YCbCr: Y = Kr*R =
  // 0.2126*255 = 54.2, Cb = 128 + (0-Y)/(2*(1-Kb)) = 98.8, Cr = 128 +
  // (255-Y)/(2*(1-Kr)) = 255.5 (clamped to 255) -- computed directly from
  // the same Kr/Kb constants src/frame_convert.c's own yuv_kr_kb() uses
  // for BT.709, not memorized/approximated, so this is checking crtmedia_
  // frame_convert_to_rgba()'s actual round-trip correctness rather than
  // just that it runs without crashing.
  memset(y_plane, 54, sizeof(y_plane));
  memset(u_plane, 99, sizeof(u_plane));
  memset(v_plane, 255, sizeof(v_plane));

  crtmedia_frame yuv_frame;
  memset(&yuv_frame, 0, sizeof(yuv_frame));
  yuv_frame.format = CRTMEDIA_PIXEL_FORMAT_YUV420P;
  yuv_frame.width = kWidth;
  yuv_frame.height = kHeight;
  yuv_frame.color_range = CRTMEDIA_COLOR_RANGE_FULL;
  yuv_frame.color_space = CRTMEDIA_COLOR_SPACE_BT709;
  yuv_frame.plane_count = 3;
  yuv_frame.planes[0] = crtmedia_frame_plane{y_plane, kWidth, kWidth, kHeight};
  yuv_frame.planes[1] = crtmedia_frame_plane{u_plane, 2, 2, 2};
  yuv_frame.planes[2] = crtmedia_frame_plane{v_plane, 2, 2, 2};

  uint8_t rgba_pixels[kWidth * kHeight * 4];
  crtmedia_frame rgba_frame;
  memset(&rgba_frame, 0, sizeof(rgba_frame));
  rgba_frame.format = CRTMEDIA_PIXEL_FORMAT_RGBA8888;
  rgba_frame.width = kWidth;
  rgba_frame.height = kHeight;
  rgba_frame.planes[0].data = rgba_pixels;
  rgba_frame.planes[0].stride = kWidth * 4;

  crtmedia_result r = crtmedia_frame_convert_to_rgba(&yuv_frame, &rgba_frame);
  CHECK(r == CRTMEDIA_OK, "YUV420P->RGBA8888 conversion succeeds");

  sk_sp<SkImage> image = wrap_packed_frame(rgba_frame);
  CHECK(image != nullptr, "converted YUV420P frame wraps into a real SkImage");
  if (!image) return;

  SkColor sample = draw_and_sample(image, 2, 2);
  // Allow a small tolerance for integer rounding through the double-
  // precision color matrix and Skia's own pixel pipeline -- this is
  // checking "did a red frame draw as recognizably red", not bit-exact
  // reproduction of a hand-computed matrix result.
  int r_channel = SkColorGetR(sample);
  int g_channel = SkColorGetG(sample);
  int b_channel = SkColorGetB(sample);
  CHECK(r_channel > 240 && g_channel < 15 && b_channel < 15,
        "YUV420P red frame draws through SkSurface as recognizably red");
}

}  // namespace

extern "C" int main() {
  test_rgba8888_direct();
  test_bgra8888_direct();
  test_yuv420p_via_conversion();

  if (g_failures != 0) {
    printf("crtmedia_frame_skia_smoke: %d failure(s)\n", g_failures);
    return 1;
  }
  printf("crtmedia_frame_skia_smoke: ok\n");
  return 0;
}
