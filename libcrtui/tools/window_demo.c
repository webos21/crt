/* crtui window demo (Tranche 1): a Window > Container > Label/Button/Slider/
 * Progress scene, rendered by crtui's private LVGL software renderer into the
 * crtgfx window's software framebuffer and presented through the existing
 * crtgfx window path. Usage: crtui_window_demo [frame_limit]. With a limit it
 * prints "presented=<n> pixel_check=pass" and exits 0 only if the first frame's
 * pixels were the expected ones and every frame was presented. */

#include "crtgfx/window.h"
#include "crtui/ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKGROUND_RGB 0xF0F0F0u
#define BUTTON_RGB 0x2D6CDFu

typedef struct demo {
  crtui_context* ctx;
  crtui_window window;
  crtui_widget column, label, button, slider, progress;
  int32_t width, height;
} demo;

static int demo_build(demo* d, int32_t width, int32_t height) {
  memset(d, 0, sizeof(*d));
  d->width = width;
  d->height = height;
  if (crtui_context_create(&d->ctx) != CRTUI_OK) return -1;
  if (crtui_window_create(d->ctx, &d->window) != CRTUI_OK) return -1;
  crtui_window_set_size(d->ctx, d->window, width, height, 1.0f);
  if (crtui_container_create(d->ctx, d->window, &d->column) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(d->ctx, d->column, 20, 20, width - 40, height - 40);
  if (crtui_text_create(d->ctx, d->column, "crtui on crtgfx", &d->label) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(d->ctx, d->label, 0, 0, 240, 24);
  if (crtui_button_create(d->ctx, d->column, "Button", &d->button) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(d->ctx, d->button, 0, 40, 140, 36);
  if (crtui_slider_create(d->ctx, d->column, 0, 100, &d->slider) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(d->ctx, d->slider, 0, 100, 300, 14);
  if (crtui_progress_create(d->ctx, d->column, &d->progress) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(d->ctx, d->progress, 0, 140, 300, 16);
  return 0;
}

static uint32_t rgb_at(const crtgfx_framebuffer* fb, uint32_t x, uint32_t y) {
  const unsigned char* p = (const unsigned char*)fb->pixels + (size_t)y * fb->stride + (size_t)x * 4u;
  return ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[0];
}

int main(int argc, char** argv) {
  unsigned long frame_limit = argc > 1 ? strtoul(argv[1], NULL, 10) : 0;
  crtgfx_window_desc desc;
  crtgfx_window* window = NULL;
  desc.title = "crtui window demo";
  desc.width = 480;
  desc.height = 320;
  desc.flags = CRTGFX_WINDOW_VISIBLE;
  if (crtgfx_window_create(&desc, &window) != CRTGFX_OK) {
    fprintf(stderr, "crtui_window_demo: window create failed\n");
    return 1;
  }

  demo d;
  memset(&d, 0, sizeof(d));
  unsigned long frame = 0;
  int pixel_check = 0;
  int failed = 0;
  while (!crtgfx_window_should_close(window) && (frame_limit == 0 || frame < frame_limit)) {
    crtgfx_framebuffer fb;
    if (crtgfx_window_begin_frame(window, &fb) != CRTGFX_OK) {
      failed = 1;
      break;
    }
    if (d.ctx == NULL || d.width != (int32_t)fb.width || d.height != (int32_t)fb.height) {
      if (d.ctx != NULL) crtui_context_destroy(d.ctx);
      if (demo_build(&d, (int32_t)fb.width, (int32_t)fb.height) != 0) {
        fprintf(stderr, "crtui_window_demo: scene build failed\n");
        failed = 1;
        break;
      }
    }
    crtui_progress_set_value(d.ctx, d.progress, (int32_t)((frame * 3u) % 101u));
    crtui_slider_set_value(d.ctx, d.slider, (int32_t)((frame * 2u) % 101u));
    crtui_result r = crtui_window_render(d.ctx, d.window, fb.pixels, fb.stride, (int32_t)fb.width, (int32_t)fb.height);
    if (r != CRTUI_OK) {
      fprintf(stderr, "crtui_window_demo: render failed (%d)\n", (int)r);
      failed = 1;
      break;
    }
    if (frame == 0) {
      /* The window background and the button fill, read straight back from the
       * framebuffer that is about to be presented. */
      pixel_check = rgb_at(&fb, 3, 3) == BACKGROUND_RGB && rgb_at(&fb, 20 + 6, 20 + 40 + 6) == BUTTON_RGB;
    }
    crtgfx_window_end_frame(window);
    ++frame;
    crtgfx_window_pump_events(16);
  }
  if (d.ctx != NULL) crtui_context_destroy(d.ctx);
  crtgfx_window_destroy(window);
  fprintf(stderr, "crtui_window_demo: presented=%lu pixel_check=%s\n", frame, pixel_check ? "pass" : "fail");
  if (failed || (frame_limit != 0 && (frame != frame_limit || !pixel_check))) {
    return 1;
  }
  return 0;
}
