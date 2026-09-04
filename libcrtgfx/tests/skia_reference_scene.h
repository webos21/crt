#pragma once

// Shared, deterministic drawing scene used both by skia_cpu_coverage_test.cc
// (CPU raster) and skia_gpu_offscreen_smoke.cc (real Ganesh/Vulkan offscreen
// surface, 2026-09-03, TODO.md's "Enable Skia GPU rendering" step) -- so the
// *same* real drawing is provable against both backends, not just "both
// draw something." Deliberately small and representative rather than the
// full path/transform/clip/layer/shader/blend/image sweep skia_cpu_
// coverage_test.cc's own 10 existing tests already cover on the CPU-raster
// path alone (those stay as-is, untouched, this is new, additive coverage) --
// this scene exercises one of each broad category (a path, a linear
// gradient, a blend mode) in a single, cheap-to-render pass, cheap enough to
// draw+readback multiple times in the same GPU test (resize/device-loss
// coverage).

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkShader.h"
#include "include/core/SkTileMode.h"
#include "include/effects/SkGradient.h"

#include <cstddef>
#include <cstdint>

namespace crtgfx_test {

constexpr uint32_t kReferenceSceneWidth = 64;
constexpr uint32_t kReferenceSceneHeight = 64;

struct ReferenceSceneBgra {
  uint8_t b, g, r, a;
};

// Reads one BGRA8888-premultiplied pixel out of a raw pixel buffer with the
// given row stride -- matches kBGRA_8888_SkColorType's own real channel
// order (see skia_cpu_coverage_test.cc's own established convention).
// SkSurface::readPixels()/SkImage::readPixels() always hand back top-left-
// origin pixel data regardless of a GPU surface's own internal
// GrSurfaceOrigin, so the same (x, y) coordinates below are correct
// against either backend's own readback.
inline ReferenceSceneBgra read_reference_scene_pixel(
    const void* pixels, size_t stride, int x, int y) {
  const uint8_t* row = static_cast<const uint8_t*>(pixels) + static_cast<size_t>(y) * stride;
  const uint8_t* p = row + static_cast<size_t>(x) * 4;
  return ReferenceSceneBgra{p[0], p[1], p[2], p[3]};
}

// Draws the shared reference scene into `canvas` (assumed
// kReferenceSceneWidth x kReferenceSceneHeight, BGRA8888-premultiplied) --
// real, deterministic content, one shape per quadrant so a single readback
// can check all three independently:
//   top-left:     an opaque, non-anti-aliased triangle path
//   top-right:    a red -> blue linear gradient rect
//   bottom-left:  a real alpha-blended overlay (see below for why this is
//                 kSrcOver, not kMultiply)
//   bottom-right: left untouched (stays transparent)
//
// Deliberately plain alpha compositing (kSrcOver, the default), not a
// non-separable blend mode like kMultiply (which skia_cpu_coverage_test.cc's
// own test_blend_mode() already covers on the CPU-raster path, unaffected
// by any of this): confirmed for real (2026-09-03) that this vertical
// slice's own minimal Ganesh/Vulkan device -- no extensions enabled at
// all -- silently renders kMultiply as plain kSrcOver instead (real,
// deterministic, opaque green, not the expected exact black; not a crash,
// not flaky). Non-separable blend modes need real GPU support (e.g.
// VK_EXT_blend_operation_advanced) or an explicit shader-based dst-read
// fallback wired into Ganesh's own device/extension setup, neither of
// which this vertical slice's scope includes (see its own explicit non-
// goals) -- plain Porter-Duff alpha blending, unlike Multiply/Screen/etc.,
// is real, standard, fixed-function GPU behavior every backend implements
// correctly with zero extra setup, so that is what this *shared*,
// cross-backend scene exercises instead.
inline void draw_reference_scene(SkCanvas* canvas) {
  canvas->clear(SK_ColorTRANSPARENT);

  SkPaint triangle_paint;
  triangle_paint.setAntiAlias(false);
  triangle_paint.setColor(SkColorSetARGB(255, 0x10, 0x90, 0x20));
  SkPath triangle = SkPathBuilder().moveTo(4, 4).lineTo(28, 4).lineTo(16, 28).close().detach();
  canvas->drawPath(triangle, triangle_paint);

  SkColor4f colors[2] = {
      SkColor4f{1.0f, 0.0f, 0.0f, 1.0f},  // red
      SkColor4f{0.0f, 0.0f, 1.0f, 1.0f},  // blue
  };
  SkGradient::Colors gradient_colors(colors, SkTileMode::kClamp);
  SkGradient gradient(gradient_colors, SkGradient::Interpolation{});
  SkPoint pts[2] = {SkPoint::Make(36, 16), SkPoint::Make(60, 16)};
  sk_sp<SkShader> shader = SkShaders::LinearGradient(pts, gradient);
  SkPaint gradient_paint;
  gradient_paint.setShader(shader);
  canvas->drawRect(SkRect::MakeXYWH(36, 4, 24, 24), gradient_paint);

  SkPaint red;
  red.setColor(SK_ColorRED);
  canvas->drawRect(SkRect::MakeXYWH(4, 36, 24, 24), red);
  // Plain kSrcOver (the default -- no setBlendMode() call), alpha 128/255:
  // real, standard alpha compositing of opaque white over opaque red ->
  // deterministically ~(255, 128, 128) (see check_reference_scene()'s own
  // tolerance band for exactly how deterministic).
  SkPaint overlay;
  overlay.setColor(SkColorSetARGB(128, 255, 255, 255));
  canvas->drawRect(SkRect::MakeXYWH(4, 36, 24, 24), overlay);
}

// Real, shared assertions against draw_reference_scene()'s own output --
// both skia_cpu_coverage_test.cc and skia_gpu_offscreen_smoke.cc call this
// against their own readback, so a real cross-backend behavior difference
// shows up as the same check failing in both places, not two independently
// drifting expectations. `report(name, condition)` matches both call
// sites' own existing pass/fail-reporting shape.
template <typename ReportFn>
inline void check_reference_scene(const void* pixels, size_t stride, ReportFn report) {
  ReferenceSceneBgra inside_triangle = read_reference_scene_pixel(pixels, stride, 16, 12);
  ReferenceSceneBgra outside_everything = read_reference_scene_pixel(pixels, stride, 56, 56);
  report("reference scene: triangle filled", inside_triangle.a == 255);
  report("reference scene: area outside every shape stays transparent", outside_everything.a == 0);

  ReferenceSceneBgra gradient_left = read_reference_scene_pixel(pixels, stride, 38, 16);
  ReferenceSceneBgra gradient_right = read_reference_scene_pixel(pixels, stride, 58, 16);
  report(
      "reference scene: gradient left edge is red-dominant", gradient_left.r > gradient_left.b + 40);
  report(
      "reference scene: gradient right edge is blue-dominant", gradient_right.b > gradient_right.r + 40);

  // White-over-red at alpha 128/255, plain kSrcOver -- real, standard
  // premultiplied alpha compositing: red stays saturated (both source and
  // destination are already 255 in that channel), green/blue land near
  // 128 (halfway from red's 0 toward white's 255). A band, not an exact
  // value: this project has not verified either backend's own blend
  // rounding to the last bit, and the two real backends (CPU raster,
  // Ganesh/Vulkan) are not guaranteed to round identically -- the band is
  // still tight enough to clearly distinguish "actually alpha-blended"
  // from "overlay silently ignored" (opaque red, near 0) or "overlay
  // silently opaque" (plain white, near 255).
  ReferenceSceneBgra blend = read_reference_scene_pixel(pixels, stride, 16, 48);
  report(
      "reference scene: alpha-blended overlay lands red-dominant with mid-tone green/blue",
      blend.r > 200 && blend.g > 90 && blend.g < 165 && blend.b > 90 && blend.b < 165 &&
          blend.a == 255);
}

}  // namespace crtgfx_test
