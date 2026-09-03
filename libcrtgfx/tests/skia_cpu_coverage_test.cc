// Deterministic Skia CPU-raster regression coverage (TODO.md's "Broaden
// deterministic Skia CPU coverage" item). Unlike crtgfx_skia_raster_smoke
// (tests/skia_raster_smoke.cc), which goes through a real crtgfx_window to
// exercise the full window-to-pixels path, every test here builds its own
// crtgfx_framebuffer directly over a plain malloc'd buffer and hands it to
// crtgfx_skia_make_raster_surface() (src/skia_bridge.cc) -- that function
// only ever needs a crtgfx_framebuffer, not a live host window (see its own
// implementation), so this whole file runs identically headless on every
// host's CI runner, matching Phase 2's own "deterministic, no real OS
// resource needed" property for the window/event side of this API
// (tests/synthetic_event_test.c). Normal Skia headers are used throughout
// (public API), per this item's own "not a project-owned drawing facade"
// scope note.
//
// Coverage, matching TODO.md's own listed sub-items:
//  - path / transform / clip / save-restore / layer: test_path(),
//    test_transform(), test_clip_and_restore(), test_save_restore_
//    transform(), test_layer_alpha() below.
//  - image draw/scaling: test_image_draw_scale(). "Decode" specifically is
//    out of scope here, not silently skipped -- tools/build_skia.py
//    deliberately disables every real image codec (libpng/libjpeg-turbo/
//    libwebp/wuffs; see that file's own comment), so this project's Skia
//    integration has no SkCodec-based encoded-image decoding surface to
//    test at all today. What IS exercised is the real raw-pixel image
//    construction/draw/scale path (SkBitmap -> SkImages::RasterFromBitmap
//    -> SkCanvas::drawImageRect()), which is this project's actual current
//    image-handling surface.
//  - representative shaders and blend modes: test_gradient_shader(),
//    test_blend_mode().
//  - error paths for NaN/Inf and invalid surface sizes: test_nan_inf_
//    error_paths(), test_invalid_surface_sizes().
//
// Every color-content check below uses fully opaque (alpha=255) paints
// specifically so premultiplication never has to be undone to compare
// against an expected plain RGB value -- premultiplied-by-255 is a no-op.

#include "crtgfx/skia.h"

#include "include/core/SkBitmap.h"
#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkShader.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTileMode.h"
#include "include/effects/SkGradient.h"

#include "skia_reference_scene.h"

#include <stdio.h>
#include <stdlib.h>

namespace {

constexpr uint32_t kW = 64;
constexpr uint32_t kH = 64;

int g_failures = 0;

// Every check funnels through here so pass/fail lines have one consistent
// shape a human (or a PASS_REGULAR_EXPRESSION ctest check) can scan, the
// same convention crtgfx_synthetic_event's own test file established.
bool report(const char* name, bool ok) {
  if (ok) {
    printf("crtgfx_skia_cpu_coverage: %s ok\n", name);
  } else {
    printf("crtgfx_skia_cpu_coverage: %s FAILED\n", name);
    ++g_failures;
  }
  return ok;
}

struct Bgra {
  uint8_t b, g, r, a;
};

// kBGRA_8888_SkColorType (what crtgfx_skia_make_raster_surface() always
// wraps a crtgfx_framebuffer as, src/skia_bridge.cc) stores channel bytes
// in B,G,R,A order -- confirmed against crtgfx_skia_raster_smoke.cc's own
// established convention of reading the alpha byte at index 3.
Bgra read_pixel(const void* pixels, uint32_t stride, int x, int y) {
  const uint8_t* row = static_cast<const uint8_t*>(pixels) + static_cast<size_t>(y) * stride;
  const uint8_t* p = row + static_cast<size_t>(x) * 4;
  return Bgra{p[0], p[1], p[2], p[3]};
}

// One fresh, malloc'd BGRA8888-premultiplied framebuffer per test, zeroed
// (fully transparent) up front -- no crtgfx_window/host backend of any
// kind involved. Skia's raster surface writes directly into this buffer
// (no flush needed -- crtgfx_skia_raster_smoke.cc's own pixel-inspection
// code already relies on the same immediate-write behavior for a CPU
// raster surface).
class ScratchSurface {
 public:
  explicit ScratchSurface(uint32_t width = kW, uint32_t height = kH)
      : width_(width), height_(height), stride_(width * 4u) {
    pixels_ = static_cast<uint8_t*>(calloc(1, static_cast<size_t>(stride_) * height_));
    fb_.pixels = pixels_;
    fb_.width = width_;
    fb_.height = height_;
    fb_.stride = stride_;
    fb_.format = CRTGFX_PIXEL_FORMAT_BGRA8888_PREMULTIPLIED;
    fb_.generation = 0;
    if (pixels_ != nullptr) {
      surface_ = crtgfx_skia_make_raster_surface(&fb_);
    }
  }
  ~ScratchSurface() { free(pixels_); }
  ScratchSurface(const ScratchSurface&) = delete;
  ScratchSurface& operator=(const ScratchSurface&) = delete;

  bool valid() const { return surface_ != nullptr; }
  SkCanvas* canvas() { return surface_->getCanvas(); }
  Bgra pixel(int x, int y) const { return read_pixel(pixels_, stride_, x, y); }
  const void* pixels_ptr() const { return pixels_; }
  uint32_t stride_bytes() const { return stride_; }

 private:
  uint32_t width_;
  uint32_t height_;
  uint32_t stride_;
  uint8_t* pixels_ = nullptr;
  crtgfx_framebuffer fb_{};
  sk_sp<SkSurface> surface_;
};

void test_path() {
  ScratchSurface s;
  if (!report("path: surface created", s.valid())) return;
  SkCanvas* canvas = s.canvas();
  canvas->clear(SK_ColorTRANSPARENT);

  // A closed straight-edge path (moveTo/lineTo/lineTo/close) and a
  // separate curved-edge one (quadTo) -- exercises both path segment
  // kinds this project's own consumers actually draw with (text glyph
  // outlines are curve-heavy; simple UI chrome is line-heavy). Built via
  // SkPathBuilder, not SkPath's own (removed, in this Skia revision)
  // mutable moveTo/lineTo/... methods -- SkPath itself is immutable here;
  // SkPathBuilder::detach() is the real, current way to construct one.
  SkPaint paint;
  paint.setAntiAlias(false);  // exact-edge pixel checks below need no AA softening
  paint.setColor(SkColorSetARGB(255, 0x10, 0x90, 0x20));
  SkPath triangle = SkPathBuilder()
                        .moveTo(8, 8)
                        .lineTo(24, 8)
                        .lineTo(16, 24)
                        .close()
                        .detach();
  canvas->drawPath(triangle, paint);

  paint.setColor(SkColorSetARGB(255, 0x90, 0x10, 0x60));
  SkPath curve = SkPathBuilder()
                     .moveTo(32, 8)
                     .quadTo(48, 8, 48, 24)
                     .lineTo(32, 24)
                     .close()
                     .detach();
  canvas->drawPath(curve, paint);

  Bgra inside_triangle = s.pixel(16, 14);
  Bgra inside_curve = s.pixel(40, 16);
  Bgra untouched = s.pixel(56, 56);

  report("path: straight-edge triangle filled", inside_triangle.a == 255);
  report("path: curved-edge shape filled", inside_curve.a == 255);
  report("path: area outside both paths stays transparent", untouched.a == 0);
}

void test_transform() {
  ScratchSurface s;
  if (!report("transform: surface created", s.valid())) return;
  SkCanvas* canvas = s.canvas();
  canvas->clear(SK_ColorTRANSPARENT);

  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setColor(SkColorSetARGB(255, 0xe0, 0x40, 0x40));

  canvas->save();
  canvas->translate(32.0f, 8.0f);
  canvas->scale(2.0f, 2.0f);
  // Local-space (0,0)-(8,8); translate+scale above maps this to real
  // screen-space (32,8)-(48,24).
  canvas->drawRect(SkRect::MakeXYWH(0, 0, 8, 8), paint);
  canvas->restore();

  Bgra at_mapped_location = s.pixel(40, 16);       // inside the mapped (32,8)-(48,24) rect
  Bgra at_unmapped_origin = s.pixel(2, 2);          // where it would land with no transform at all
  Bgra past_mapped_edge = s.pixel(50, 16);          // just outside the mapped rect's right edge

  report("transform: rect lands at the translated+scaled location", at_mapped_location.a == 255);
  report("transform: rect does not also appear at the untransformed origin",
         at_unmapped_origin.a == 0);
  report("transform: mapped rect does not overshoot its own scaled bounds",
         past_mapped_edge.a == 0);
}

void test_clip_and_restore() {
  ScratchSurface s;
  if (!report("clip: surface created", s.valid())) return;
  SkCanvas* canvas = s.canvas();
  canvas->clear(SK_ColorTRANSPARENT);

  SkPaint red;
  red.setColor(SK_ColorRED);
  SkPaint blue;
  blue.setColor(SK_ColorBLUE);

  canvas->save();
  canvas->clipRect(SkRect::MakeXYWH(10, 10, 20, 20));
  canvas->drawRect(SkRect::MakeXYWH(0, 0, kW, kH), red);  // whole-canvas rect, clipped down
  canvas->restore();

  Bgra inside_clip = s.pixel(15, 15);
  Bgra outside_clip = s.pixel(2, 2);
  report("clip: draw confined to the clip rect", inside_clip.r == 255 && inside_clip.a == 255);
  report("clip: draw excluded outside the clip rect", outside_clip.a == 0);

  // The real point of this second draw: if restore() failed to lift the
  // clip set above, this whole-canvas draw would still be confined to
  // (10,10)-(30,30) and (2,2) would stay untouched.
  canvas->drawRect(SkRect::MakeXYWH(0, 0, kW, kH), blue);
  Bgra after_restore = s.pixel(2, 2);
  report("clip: restore() actually lifts the earlier clip",
         after_restore.b == 255 && after_restore.a == 255);
}

void test_save_restore_transform() {
  ScratchSurface s;
  if (!report("save/restore: surface created", s.valid())) return;
  SkCanvas* canvas = s.canvas();
  canvas->clear(SK_ColorTRANSPARENT);

  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setColor(SK_ColorGREEN);

  canvas->save();
  canvas->translate(40.0f, 40.0f);
  canvas->restore();  // undo the translate before anything is drawn at all

  canvas->drawRect(SkRect::MakeXYWH(0, 0, 8, 8), paint);
  Bgra at_origin = s.pixel(2, 2);
  Bgra at_would_be_translated = s.pixel(42, 42);
  report("save/restore: matrix state reverted by restore()",
         at_origin.g == 255 && at_origin.a == 255);
  report("save/restore: translate does not leak past restore()",
         at_would_be_translated.a == 0);
}

void test_layer_alpha() {
  ScratchSurface s;
  if (!report("layer: surface created", s.valid())) return;
  SkCanvas* canvas = s.canvas();

  SkPaint white;
  white.setColor(SK_ColorWHITE);
  canvas->drawRect(SkRect::MakeXYWH(0, 0, kW, kH), white);

  canvas->saveLayerAlpha(nullptr, 128);
  SkPaint black;
  black.setColor(SK_ColorBLACK);
  canvas->drawRect(SkRect::MakeXYWH(0, 0, kW, kH), black);
  canvas->restore();  // composites the half-alpha layer onto the white backdrop

  Bgra p = s.pixel(kW / 2, kH / 2);
  // Expected ~127 (half of 255) -- a band, not an exact value, since this
  // project has not verified this Skia build's own raster blend rounding
  // to the last bit; the band is still tight enough to clearly distinguish
  // "actually blended" from "layer alpha silently ignored" (which would
  // read as opaque black, near 0, or opaque white, near 255).
  report("layer: saveLayerAlpha(128) blends toward mid-gray, not opaque black",
         p.r > 90 && p.g > 90 && p.b > 90);
  report("layer: saveLayerAlpha(128) blends toward mid-gray, not opaque white",
         p.r < 165 && p.g < 165 && p.b < 165);
  report("layer: compositing onto an opaque backdrop stays opaque", p.a == 255);
}

void test_gradient_shader() {
  ScratchSurface s;
  if (!report("shader: surface created", s.valid())) return;
  SkCanvas* canvas = s.canvas();
  canvas->clear(SK_ColorTRANSPARENT);

  SkColor4f colors[2] = {
      SkColor4f{1.0f, 0.0f, 0.0f, 1.0f},  // red
      SkColor4f{0.0f, 0.0f, 1.0f, 1.0f},  // blue
  };
  SkGradient::Colors gradient_colors(colors, SkTileMode::kClamp);
  SkGradient gradient(gradient_colors, SkGradient::Interpolation{});
  SkPoint pts[2] = {SkPoint::Make(0, 0), SkPoint::Make(static_cast<float>(kW), 0)};
  sk_sp<SkShader> shader = SkShaders::LinearGradient(pts, gradient);
  if (!report("shader: SkShaders::LinearGradient constructed", shader != nullptr)) return;

  SkPaint paint;
  paint.setShader(shader);
  canvas->drawRect(SkRect::MakeXYWH(0, 0, kW, kH), paint);

  Bgra left_edge = s.pixel(2, kH / 2);
  Bgra right_edge = s.pixel(kW - 3, kH / 2);
  report("shader: left edge is red-dominant", left_edge.r > left_edge.b + 40);
  report("shader: right edge is blue-dominant", right_edge.b > right_edge.r + 40);
}

void test_blend_mode() {
  ScratchSurface s;
  if (!report("blend: surface created", s.valid())) return;
  SkCanvas* canvas = s.canvas();

  SkPaint red;
  red.setColor(SK_ColorRED);
  canvas->drawRect(SkRect::MakeXYWH(0, 0, kW, kH), red);

  SkPaint green;
  green.setColor(SK_ColorGREEN);
  green.setBlendMode(SkBlendMode::kMultiply);
  canvas->drawRect(SkRect::MakeXYWH(0, 0, kW, kH), green);

  Bgra p = s.pixel(kW / 2, kH / 2);
  // Multiply of fully-opaque red (255,0,0) over fully-opaque green
  // (0,255,0), channel by channel: R=0*255/255=0, G=255*0/255=0,
  // B=0*0/255=0 -- an exact, deterministic black. The default SkBlendMode
  // (kSrcOver) would instead leave plain opaque green (0,255,0) here,
  // so this cleanly distinguishes "kMultiply actually applied" from
  // "blend mode silently ignored".
  report("blend: kMultiply produces the expected exact black",
         p.r == 0 && p.g == 0 && p.b == 0 && p.a == 255);
}

void test_image_draw_scale() {
  ScratchSurface s(32, 32);
  if (!report("image: surface created", s.valid())) return;
  SkCanvas* canvas = s.canvas();
  canvas->clear(SK_ColorTRANSPARENT);

  // No image *codec* is linked into this project's Skia build at all
  // (tools/build_skia.py deliberately disables libpng/libjpeg-turbo/
  // libwebp/wuffs -- nothing currently needs encoded-image decoding), so
  // this exercises the real raw-pixel SkImage construction/draw/scale
  // path this project's Skia integration actually has today, not SkCodec.
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(2, 2));
  *bitmap.getAddr32(0, 0) = SkColorSetARGB(255, 255, 0, 0);    // TL red
  *bitmap.getAddr32(1, 0) = SkColorSetARGB(255, 0, 255, 0);    // TR green
  *bitmap.getAddr32(0, 1) = SkColorSetARGB(255, 0, 0, 255);    // BL blue
  *bitmap.getAddr32(1, 1) = SkColorSetARGB(255, 255, 255, 0);  // BR yellow
  sk_sp<SkImage> image = SkImages::RasterFromBitmap(bitmap);
  if (!report("image: SkImages::RasterFromBitmap constructed", image != nullptr)) return;

  // Nearest-neighbor, not linear: scaling this 2x2 source up to 32x32
  // should reproduce each source texel exactly with no blended edge
  // colors, which is the specific, deterministic thing being checked --
  // a real scale actually happened (2x2 -> 32x32), not just a copy.
  canvas->drawImageRect(image, SkRect::MakeXYWH(0, 0, 32, 32),
                        SkSamplingOptions(SkFilterMode::kNearest));

  Bgra top_left = s.pixel(4, 4);
  Bgra top_right = s.pixel(28, 4);
  Bgra bottom_left = s.pixel(4, 28);
  Bgra bottom_right = s.pixel(28, 28);
  report("image: scaled top-left quadrant is red",
         top_left.r == 255 && top_left.g == 0 && top_left.b == 0);
  report("image: scaled top-right quadrant is green",
         top_right.r == 0 && top_right.g == 255 && top_right.b == 0);
  report("image: scaled bottom-left quadrant is blue",
         bottom_left.r == 0 && bottom_left.g == 0 && bottom_left.b == 255);
  report("image: scaled bottom-right quadrant is yellow",
         bottom_right.r == 255 && bottom_right.g == 255 && bottom_right.b == 0);
}

void test_nan_inf_error_paths() {
  ScratchSurface s;
  if (!report("nan/inf: surface created", s.valid())) return;
  SkCanvas* canvas = s.canvas();
  canvas->clear(SK_ColorTRANSPARENT);

  const float nan_v = __builtin_nanf("");
  const float inf_v = __builtin_inff();
  SkPaint paint;
  paint.setColor(SK_ColorRED);

  // None of these are expected to draw anything meaningful. The real
  // assertion this whole function makes is implicit: the process is still
  // alive and the canvas is still usable by the time control reaches the
  // check below -- i.e. Skia's raster backend rejects malformed geometry
  // rather than crashing or corrupting its own internal state. A crash
  // here (SIGSEGV/SIGFPE) fails this ctest target the same way any other
  // crash would, which is the correct, intended failure signal -- there is
  // no C++ exception to catch, this is deliberately not wrapped in a
  // try/catch.
  canvas->drawRect(SkRect::MakeLTRB(nan_v, 0, 10, 10), paint);
  canvas->drawRect(SkRect::MakeLTRB(0, 0, inf_v, 10), paint);
  canvas->drawCircle(20, 20, nan_v, paint);
  canvas->drawCircle(20, 20, inf_v, paint);
  canvas->save();
  canvas->scale(nan_v, 1.0f);
  canvas->drawRect(SkRect::MakeXYWH(0, 0, 10, 10), paint);
  canvas->restore();

  paint.setColor(SK_ColorGREEN);
  canvas->drawRect(SkRect::MakeXYWH(40, 40, 10, 10), paint);
  Bgra p = s.pixel(45, 45);
  report("nan/inf: canvas still usable after malformed draws (no crash)",
         p.g == 255 && p.a == 255);
}

void test_invalid_surface_sizes() {
  uint8_t dummy[64] = {0};
  crtgfx_framebuffer fb{};

  report("invalid surface: null framebuffer pointer rejected",
         crtgfx_skia_make_raster_surface(nullptr) == nullptr);

  fb.pixels = dummy;
  fb.width = 0;
  fb.height = 4;
  fb.stride = 16;
  fb.format = CRTGFX_PIXEL_FORMAT_BGRA8888_PREMULTIPLIED;
  report("invalid surface: zero width rejected", crtgfx_skia_make_raster_surface(&fb) == nullptr);

  fb.width = 4;
  fb.height = 0;
  report("invalid surface: zero height rejected", crtgfx_skia_make_raster_surface(&fb) == nullptr);

  fb.height = 4;
  fb.stride = 8;  // < width(4)*4 == 16
  report("invalid surface: too-small stride rejected",
         crtgfx_skia_make_raster_surface(&fb) == nullptr);

  fb.stride = 16;
  fb.pixels = nullptr;
  report("invalid surface: null pixels rejected", crtgfx_skia_make_raster_surface(&fb) == nullptr);

  fb.pixels = dummy;
  fb.format = static_cast<crtgfx_pixel_format>(0);  // no format has value 0 today
  report("invalid surface: unrecognized pixel format rejected",
         crtgfx_skia_make_raster_surface(&fb) == nullptr);

  fb.format = CRTGFX_PIXEL_FORMAT_BGRA8888_PREMULTIPLIED;
  report("invalid surface: otherwise-valid framebuffer is accepted (sanity check)",
         crtgfx_skia_make_raster_surface(&fb) != nullptr);
}

// The shared reference scene (tests/skia_reference_scene.h) drawn and
// checked on the CPU-raster path -- new, additive coverage (2026-09-03,
// TODO.md's "Enable Skia GPU rendering" step), not a replacement for any
// test above. skia_gpu_offscreen_smoke.cc draws and checks the exact same
// scene against a real Ganesh/Vulkan-backed surface, so a real cross-
// backend rendering difference shows up as this same shared assertion
// failing in both places, not two independently drifting expectations.
void test_reference_scene() {
  ScratchSurface s(crtgfx_test::kReferenceSceneWidth, crtgfx_test::kReferenceSceneHeight);
  if (!report("reference scene: surface created", s.valid())) return;
  crtgfx_test::draw_reference_scene(s.canvas());
  crtgfx_test::check_reference_scene(
      s.pixels_ptr(), s.stride_bytes(),
      [](const char* name, bool ok) { report(name, ok); });
}

}  // namespace

extern "C" int main() {
  test_path();
  test_transform();
  test_clip_and_restore();
  test_save_restore_transform();
  test_layer_alpha();
  test_gradient_shader();
  test_blend_mode();
  test_image_draw_scale();
  test_nan_inf_error_paths();
  test_invalid_surface_sizes();
  test_reference_scene();

  if (g_failures == 0) {
    puts("crtgfx_skia_cpu_coverage: ok");
    return 0;
  }
  printf("crtgfx_skia_cpu_coverage: %d check(s) failed\n", g_failures);
  return 1;
}
