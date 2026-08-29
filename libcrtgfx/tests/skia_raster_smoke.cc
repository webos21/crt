#include "crtgfx/skia.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkTypeface.h"
#include "include/ports/SkFontMgr_directory.h"

#include <stdio.h>

#ifndef CRT_SKIA_FONTS_DIR
#error "CRT_SKIA_FONTS_DIR must be defined (see libcrtgfx/CMakeLists.txt)"
#endif

static int fail(const char* message, int code = 0) {
  printf("crtgfx_skia_raster_smoke: %s (%d)\n", message, code);
  return 1;
}

extern "C" int main() {
  crtgfx_window_desc desc;
  crtgfx_window* window;
  crtgfx_framebuffer framebuffer;
  sk_sp<SkSurface> surface;
  SkCanvas* canvas;
  SkPaint paint;
  unsigned char* pixels;
  unsigned int changed;
  int rc;

  desc.title = "crtgfx skia raster smoke";
  desc.width = 320;
  desc.height = 200;
  desc.flags = 0;

  rc = crtgfx_window_create(&desc, &window);
  if (rc == CRTGFX_ERROR_UNSUPPORTED) {
    puts("crtgfx_skia_raster_smoke: ok (unsupported)");
    return 0;
  }
  if (rc != CRTGFX_OK) {
    return fail("create", rc);
  }

  rc = crtgfx_window_begin_frame(window, &framebuffer);
  if (rc != CRTGFX_OK) {
    crtgfx_window_destroy(window);
    return fail("begin_frame", rc);
  }

  surface = crtgfx_skia_make_raster_surface(&framebuffer);
  if (!surface) {
    crtgfx_window_destroy(window);
    return fail("make_raster_surface");
  }

  canvas = surface->getCanvas();
  if (canvas == nullptr) {
    crtgfx_window_destroy(window);
    return fail("canvas");
  }

  canvas->clear(SK_ColorTRANSPARENT);
  paint.setColor(SkColorSetARGB(255, 0x20, 0x80, 0xf0));
  canvas->drawRect(SkRect::MakeXYWH(16.0f, 12.0f, 120.0f, 72.0f), paint);
  paint.setColor(SkColorSetARGB(255, 0xf0, 0x60, 0x30));
  canvas->drawCircle(192.0f, 96.0f, 48.0f, paint);

  pixels = static_cast<unsigned char*>(framebuffer.pixels);
  changed = 0;
  for (uint32_t y = 0; y < framebuffer.height; ++y) {
    const unsigned char* row = pixels + y * framebuffer.stride;
    for (uint32_t x = 0; x < framebuffer.width; ++x) {
      if (row[x * 4u + 3u] != 0) {
        changed = 1;
        break;
      }
    }
    if (changed) {
      break;
    }
  }
  if (!changed) {
    crtgfx_window_destroy(window);
    return fail("draw produced no pixels");
  }

  // Real end-to-end text rendering: crtgfx -> Skia -> this project's own
  // FreeType port (porting/recipes/freetype.json, shared-pass on Linux/
  // Windows as of 2026-08-24), not a stub/empty font manager. Uses the
  // same real, documented SkFontMgr_New_Custom_Directory() API porting/
  // tests/freetype_glyph_test.c's own notes already point to, scanning
  // the bundled libcrtgfx/assets/fonts/ directory (Pretendard GOV,
  // DejaVuSansMono.ttf -- see that directory's README.md).
  {
    sk_sp<SkFontMgr> font_mgr = SkFontMgr_New_Custom_Directory(CRT_SKIA_FONTS_DIR);
    if (!font_mgr) {
      crtgfx_window_destroy(window);
      return fail("SkFontMgr_New_Custom_Directory");
    }
    // crtgfx_skia_default_typeface() (crtgfx/skia.h): resolves "Pretendard
    // GOV" by exact family-name match first, falling back to "DejaVu Sans
    // Mono" and finally legacyMakeTypeface(nullptr, ...) -- see that
    // function's own doc comment for why a plain nullptr lookup alone can
    // no longer be trusted to mean "the project default" now that the
    // fonts directory holds more than one real family.
    sk_sp<SkTypeface> typeface = crtgfx_skia_default_typeface(font_mgr.get(), SkFontStyle());
    if (!typeface) {
      crtgfx_window_destroy(window);
      return fail("crtgfx_skia_default_typeface found no typeface");
    }

    SkFont font(typeface, 28.0f);
    paint.setColor(SkColorSetARGB(255, 0xff, 0xff, 0xff));
    // Drawn well below the rect (y: 12-84) and circle (y: 48-144) above,
    // so the pixel-changed check below is unambiguous: it can only be
    // seeing real glyph ink, not the shapes already drawn.
    canvas->drawString("CRT", 16.0f, 176.0f, font, paint);

    unsigned int text_changed = 0;
    for (uint32_t y = 150; y < framebuffer.height && !text_changed; ++y) {
      const unsigned char* row = pixels + y * framebuffer.stride;
      for (uint32_t x = 0; x < framebuffer.width; ++x) {
        if (row[x * 4u + 3u] != 0) {
          text_changed = 1;
          break;
        }
      }
    }
    if (!text_changed) {
      crtgfx_window_destroy(window);
      return fail("drawString produced no pixels");
    }
  }

  rc = crtgfx_window_end_frame(window);
  if (rc != CRTGFX_OK) {
    crtgfx_window_destroy(window);
    return fail("end_frame", rc);
  }
  rc = crtgfx_window_pump_events(10);
  if (rc != CRTGFX_OK) {
    crtgfx_window_destroy(window);
    return fail("pump", rc);
  }

  crtgfx_window_destroy(window);
  puts("crtgfx_skia_raster_smoke: ok");
  return 0;
}
