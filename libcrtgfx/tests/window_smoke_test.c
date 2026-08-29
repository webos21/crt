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

  /* Multi-window check (2026-08-29, Phase 1): a second crtgfx_window_
   * create() while the first is still open must succeed and behave
   * independently -- deliberately checked here, ahead of the single-
   * window frame-cycle test below, so it still runs (and still means
   * something) even in a host/shell context where crtgfx_window_end_
   * frame()'s own real presentation fails for unrelated reasons (a real,
   * separate, already-tracked WSL-specific quirk -- see TODO.md/
   * HISTORY.md). Deliberately does not exercise begin_frame/end_frame on
   * the second window here (that path is already covered, single-
   * window, by the rest of this file) -- this block exists to prove
   * window B's own create/get_size/destroy do not disturb window A, not
   * to duplicate the frame-cycle coverage. */
  {
    crtgfx_window_desc desc_b = desc;
    crtgfx_window* window_b;
    uint32_t width_b;
    uint32_t height_b;

    desc_b.title = "crtgfx smoke (window B)";
    rc = crtgfx_window_create(&desc_b, &window_b);
    if (rc != CRTGFX_OK) {
      crtgfx_window_destroy(window);
      return fail("second window create", rc);
    }
    if (window_b == window) {
      crtgfx_window_destroy(window_b);
      crtgfx_window_destroy(window);
      return fail("second window aliases the first", 0);
    }
    rc = crtgfx_window_get_size(window_b, &width_b, &height_b);
    if (rc != CRTGFX_OK || width_b == 0 || height_b == 0) {
      crtgfx_window_destroy(window_b);
      crtgfx_window_destroy(window);
      return fail("second window get_size", rc);
    }
    /* Destroy window B first -- on a shared-connection backend (Linux),
     * this exercises the exact path where destroying a non-last window
     * must NOT tear down the connection every window on it depends on.
     * Window A's own get_size() right below (already part of this
     * file's original single-window flow) is what actually confirms it
     * survived; no separate check duplicated here. */
    crtgfx_window_destroy(window_b);
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
