#include "crtgfx/skia.h"

#include "include/core/SkImageInfo.h"

sk_sp<SkSurface> crtgfx_skia_make_raster_surface(const crtgfx_framebuffer* framebuffer) {
  if (framebuffer == nullptr || framebuffer->pixels == nullptr || framebuffer->width == 0 ||
      framebuffer->height == 0 || framebuffer->stride < framebuffer->width * 4u ||
      framebuffer->format != CRTGFX_PIXEL_FORMAT_BGRA8888_PREMULTIPLIED) {
    return nullptr;
  }

  SkImageInfo info = SkImageInfo::Make(
      (int)framebuffer->width, (int)framebuffer->height, kBGRA_8888_SkColorType,
      kPremul_SkAlphaType);
  return SkSurfaces::WrapPixels(info, framebuffer->pixels, framebuffer->stride);
}

sk_sp<SkTypeface> crtgfx_skia_default_typeface(SkFontMgr* font_mgr, const SkFontStyle& style) {
  if (font_mgr == nullptr) {
    return nullptr;
  }
  sk_sp<SkTypeface> typeface = font_mgr->matchFamilyStyle("Pretendard GOV", style);
  if (!typeface) {
    typeface = font_mgr->matchFamilyStyle("DejaVu Sans Mono", style);
  }
  if (!typeface) {
    typeface = font_mgr->legacyMakeTypeface(nullptr, style);
  }
  return typeface;
}
