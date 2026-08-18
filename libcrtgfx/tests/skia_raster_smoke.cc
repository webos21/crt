#include "crtgfx/skia.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"

#include <stdio.h>

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
