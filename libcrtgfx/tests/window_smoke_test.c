#include "crtgfx/window.h"

#include <stdio.h>

static int fail(const char* message, int code) {
  fprintf(stderr, "crtgfx_window_smoke: %s (%d)\n", message, code);
  return 1;
}

int main(void) {
  crtgfx_window_desc desc;
  crtgfx_window* window;
  crtgfx_framebuffer framebuffer;
  unsigned char* row;
  uint32_t width;
  uint32_t height;
  uint32_t x;
  uint32_t y;
  int rc;

  desc.title = "crtgfx smoke";
  desc.width = 320;
  desc.height = 200;
  desc.flags = 0;

  rc = crtgfx_window_create(&desc, &window);
#if defined(CRT_TARGET_OS_WINDOWS)
  if (rc != CRTGFX_OK) {
    return fail("create", rc);
  }
  rc = crtgfx_window_get_size(window, &width, &height);
  if (rc != CRTGFX_OK) {
    crtgfx_window_destroy(window);
    return fail("get_size", rc);
  }
  if (width == 0 || height == 0) {
    crtgfx_window_destroy(window);
    return fail("empty client size", 0);
  }
  rc = crtgfx_window_begin_frame(window, &framebuffer);
  if (rc != CRTGFX_OK) {
    crtgfx_window_destroy(window);
    return fail("begin_frame", rc);
  }
  if (framebuffer.pixels == 0 || framebuffer.width == 0 || framebuffer.height == 0 ||
      framebuffer.stride < framebuffer.width * 4u ||
      framebuffer.format != CRTGFX_PIXEL_FORMAT_BGRA8888_PREMULTIPLIED) {
    crtgfx_window_destroy(window);
    return fail("invalid framebuffer", 0);
  }
  for (y = 0; y < framebuffer.height; ++y) {
    row = (unsigned char*)framebuffer.pixels + y * framebuffer.stride;
    for (x = 0; x < framebuffer.width; ++x) {
      row[x * 4u + 0u] = (unsigned char)(x & 0xffu);
      row[x * 4u + 1u] = (unsigned char)(y & 0xffu);
      row[x * 4u + 2u] = 0x40u;
      row[x * 4u + 3u] = 0xffu;
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
  puts("crtgfx_window_smoke: ok");
  return 0;
#else
  if (rc != CRTGFX_ERROR_UNSUPPORTED) {
    return fail("expected unsupported backend", rc);
  }
  puts("crtgfx_window_smoke: ok (unsupported)");
  return 0;
#endif
}
