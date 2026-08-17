#include "crtgfx/window.h"

#include <stdio.h>

static void draw_demo_frame(crtgfx_window* window, unsigned int tick) {
  crtgfx_framebuffer framebuffer;
  unsigned char* row;
  uint32_t x;
  uint32_t y;

  if (crtgfx_window_begin_frame(window, &framebuffer) != CRTGFX_OK) {
    return;
  }
  for (y = 0; y < framebuffer.height; ++y) {
    row = (unsigned char*)framebuffer.pixels + y * framebuffer.stride;
    for (x = 0; x < framebuffer.width; ++x) {
      row[x * 4u + 0u] = (unsigned char)((x + tick) & 0xffu);
      row[x * 4u + 1u] = (unsigned char)((y + tick * 2u) & 0xffu);
      row[x * 4u + 2u] = (unsigned char)(((x ^ y) + tick * 3u) & 0xffu);
      row[x * 4u + 3u] = 0xffu;
    }
  }
  (void)crtgfx_window_end_frame(window);
}

int main(void) {
  crtgfx_window_desc desc;
  crtgfx_window* window;
  unsigned int tick;
  int rc;

  desc.title = "crtgfx Windows Wayland surface";
  desc.width = 800;
  desc.height = 480;
  desc.flags = CRTGFX_WINDOW_VISIBLE;

  rc = crtgfx_window_create(&desc, &window);
  if (rc != CRTGFX_OK) {
    fprintf(stderr, "crtgfx_window_demo: create failed (%d)\n", rc);
    return 1;
  }

  tick = 0;
  while (!crtgfx_window_should_close(window)) {
    draw_demo_frame(window, tick++);
    crtgfx_window_pump_events(16);
  }

  crtgfx_window_destroy(window);
  return 0;
}
