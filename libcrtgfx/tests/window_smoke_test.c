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
  unsigned int frame;
  int rc;

  desc.title = "crtgfx smoke";
  desc.width = 320;
  desc.height = 200;
  desc.flags = 0;

  rc = crtgfx_window_create(&desc, &window);
  if (rc == CRTGFX_ERROR_UNSUPPORTED) {
    /* No usable host backend in this environment (e.g. headless Linux
     * CI with no Wayland compositor reachable) -- an environment
     * limitation, not a bug, matching how this project's other
     * environment-dependent tests (tests/termios_echo_roundtrip_test.c,
     * ...) skip rather than fail. Any other non-CRTGFX_OK result below is
     * a real failure once a host backend claims to be usable at all. */
    puts("crtgfx_window_smoke: ok (unsupported)");
    return 0;
  }
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
  if (crtgfx_window_begin_frame(window, &framebuffer) != CRTGFX_ERROR_INVALID_ARGUMENT) {
    crtgfx_window_destroy(window);
    return fail("double begin_frame accepted", 0);
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
  for (frame = 1; frame < 4; ++frame) {
    rc = crtgfx_window_pump_events(10);
    if (rc != CRTGFX_OK) {
      crtgfx_window_destroy(window);
      return fail("pump", rc);
    }
    rc = crtgfx_window_begin_frame(window, &framebuffer);
    if (rc != CRTGFX_OK) {
      crtgfx_window_destroy(window);
      return fail("repeat begin_frame", rc);
    }
    for (y = 0; y < framebuffer.height; ++y) {
      row = (unsigned char*)framebuffer.pixels + y * framebuffer.stride;
      for (x = 0; x < framebuffer.width; ++x) {
        row[x * 4u + 0u] = (unsigned char)((x + frame * 17u) & 0xffu);
        row[x * 4u + 1u] = (unsigned char)((y + frame * 11u) & 0xffu);
        row[x * 4u + 2u] = (unsigned char)(0x40u + frame);
        row[x * 4u + 3u] = 0xffu;
      }
    }
    rc = crtgfx_window_end_frame(window);
    if (rc != CRTGFX_OK) {
      crtgfx_window_destroy(window);
      return fail("repeat end_frame", rc);
    }
  }
  crtgfx_window_destroy(window);
  puts("crtgfx_window_smoke: ok");
  return 0;
}
