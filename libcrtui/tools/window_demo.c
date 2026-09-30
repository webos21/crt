/* crtui window demo (Tranches 1-2): a Window > Container > Label/Button/Slider/
 * Progress scene, rendered by crtui's software renderer into the
 * crtgfx window's software framebuffer and presented through the crtgfx window
 * path, with crtgfx input events (keyboard, pointer, wheel, resize) fed to crtui
 * through the crtui/crtgfx.h adapter.
 *
 * Usage: crtui_window_demo [frame_limit]
 *   no limit    interactive: Tab/arrows/Enter/Space, mouse clicks and drags on
 *               the slider, the wheel, and window resizing all work.
 *   with limit  automated: a fixed script of synthetic events (crtgfx's own
 *               test-injection hook, the same queue real events use) drives
 *               the same path, and the run prints
 *               "presented=<n> pixel_check=pass input_check=pass" and exits 0
 *               only if the first frame's pixels were right, every frame was
 *               presented, and each scripted input had its visible effect. */

#include "crtgfx/window.h"
#include "crtui/crtgfx.h"
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
  crtui_crtgfx_state input_state;
  int activations;
  int32_t width, height;
} demo;

static int on_event(crtui_widget current, const crtui_event* event, void* user) {
  demo* d = (demo*)user;
  (void)current;
  if (event->type == CRTUI_EVENT_ACTIVATE && event->target == d->button) { /* bubbled up to the window callback */
    ++d->activations;
  } else if (event->type == CRTUI_EVENT_RESIZED) {
    /* The application's own re-layout: the widgets follow the window width. */
    crtui_widget_set_bounds(d->ctx, d->column, 20, 20, event->x - 40, event->y - 40);
    crtui_widget_set_bounds(d->ctx, d->slider, 0, 100, event->x - 80, 14);
    crtui_widget_set_bounds(d->ctx, d->progress, 0, 140, event->x - 80, 16);
  }
  return CRTUI_EVENT_IGNORED;
}

static int demo_build(demo* d, int32_t width, int32_t height) {
  memset(d, 0, sizeof(*d));
  d->width = width;
  d->height = height;
  if (crtui_context_create(&d->ctx) != CRTUI_OK) return -1;
  if (crtui_window_create(d->ctx, &d->window) != CRTUI_OK) return -1;
  if (crtui_container_create(d->ctx, d->window, &d->column) != CRTUI_OK) return -1;
  if (crtui_text_create(d->ctx, d->column, "crtui on crtgfx", &d->label) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(d->ctx, d->label, 0, 0, 240, 24);
  if (crtui_button_create(d->ctx, d->column, "Button", &d->button) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(d->ctx, d->button, 0, 40, 140, 36);
  if (crtui_slider_create(d->ctx, d->column, 0, 100, &d->slider) != CRTUI_OK) return -1;
  if (crtui_progress_create(d->ctx, d->column, &d->progress) != CRTUI_OK) return -1;
  crtui_widget_set_callback(d->ctx, d->window, on_event, d);
  /* Sizing the window emits RESIZED, whose handler lays the widgets out. */
  return crtui_window_set_size(d->ctx, d->window, width, height, 1.0f) == CRTUI_OK ? 0 : -1;
}

static uint32_t rgb_at(const crtgfx_framebuffer* fb, uint32_t x, uint32_t y) {
  const unsigned char* p = (const unsigned char*)fb->pixels + (size_t)y * fb->stride + (size_t)x * 4u;
  return ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[0];
}

static void inject(crtgfx_window* window, crtgfx_event_type type, uint32_t keycode, double x, double y, double dy,
                   uint32_t button) {
  crtgfx_event e;
  memset(&e, 0, sizeof(e));
  e.type = type;
  switch (type) {
    case CRTGFX_EVENT_KEY_DOWN:
    case CRTGFX_EVENT_KEY_UP:
      e.data.key.keycode = keycode;
      break;
    case CRTGFX_EVENT_POINTER_MOTION:
      e.data.pointer_motion.x = x;
      e.data.pointer_motion.y = y;
      break;
    case CRTGFX_EVENT_POINTER_BUTTON_DOWN:
    case CRTGFX_EVENT_POINTER_BUTTON_UP:
      e.data.pointer_button.button = button;
      e.data.pointer_button.x = x;
      e.data.pointer_button.y = y;
      break;
    case CRTGFX_EVENT_POINTER_SCROLL:
      e.data.pointer_scroll.dy = dy;
      break;
    default:
      break;
  }
  crtgfx_window_inject_event(window, &e);
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
  /* Automated-mode bookkeeping. */
  int32_t slider_after_click = -1;
  int32_t slider_after_wheel = -1;
  int focus_after_tab = 0;
  while (!crtgfx_window_should_close(window) && (frame_limit == 0 || frame < frame_limit)) {
    crtgfx_framebuffer fb;
    if (crtgfx_window_begin_frame(window, &fb) != CRTGFX_OK) {
      failed = 1;
      break;
    }
    if (d.ctx == NULL && demo_build(&d, (int32_t)fb.width, (int32_t)fb.height) != 0) {
      fprintf(stderr, "crtui_window_demo: scene build failed\n");
      failed = 1;
      break;
    }
    if (d.width != (int32_t)fb.width || d.height != (int32_t)fb.height) {
      /* The framebuffer changed before the RESIZE event was polled. */
      d.width = (int32_t)fb.width;
      d.height = (int32_t)fb.height;
      crtui_window_set_size(d.ctx, d.window, d.width, d.height, 1.0f);
    }

    if (frame_limit != 0) {
      /* Scripted input. Coordinates follow the layout above: the column starts at
       * (20,20); the button is at (20,60) 140x36, the slider at (20,120) wide. */
      if (frame == 2) {
        inject(window, CRTGFX_EVENT_KEY_DOWN, 15 /* Tab */, 0, 0, 0, 0);
        inject(window, CRTGFX_EVENT_KEY_UP, 15, 0, 0, 0, 0);
      } else if (frame == 4) {
        crtui_widget f = CRTUI_INVALID_WIDGET;
        crtui_context_get_focus(d.ctx, &f);
        focus_after_tab = f == d.button;
        inject(window, CRTGFX_EVENT_KEY_DOWN, 28 /* Enter */, 0, 0, 0, 0);
        inject(window, CRTGFX_EVENT_KEY_UP, 28, 0, 0, 0, 0);
      } else if (frame == 6) {
        inject(window, CRTGFX_EVENT_POINTER_MOTION, 30, 70, 0, 0, 0);
        inject(window, CRTGFX_EVENT_POINTER_BUTTON_DOWN, 0, 30, 70, 0, CRTGFX_POINTER_BUTTON_LEFT);
        inject(window, CRTGFX_EVENT_POINTER_BUTTON_UP, 0, 30, 70, 0, CRTGFX_POINTER_BUTTON_LEFT);
      } else if (frame == 8) {
        inject(window, CRTGFX_EVENT_POINTER_MOTION, 200, 127, 0, 0, 0);
        inject(window, CRTGFX_EVENT_POINTER_BUTTON_DOWN, 0, 200, 127, 0, CRTGFX_POINTER_BUTTON_LEFT);
        inject(window, CRTGFX_EVENT_POINTER_BUTTON_UP, 0, 200, 127, 0, CRTGFX_POINTER_BUTTON_LEFT);
      } else if (frame == 10) {
        crtui_slider_get_value(d.ctx, d.slider, &slider_after_click);
        inject(window, CRTGFX_EVENT_POINTER_SCROLL, 0, 0, 0, 10.0, 0);
      } else if (frame == 12) {
        crtui_slider_get_value(d.ctx, d.slider, &slider_after_wheel);
      }
    }

    /* Real and injected events take the same path: crtgfx queue -> adapter. */
    crtgfx_event event;
    while (crtgfx_window_poll_event(window, &event) == CRTGFX_OK && event.type != CRTGFX_EVENT_NONE) {
      crtui_window_handle_crtgfx_event(d.ctx, d.window, &d.input_state, &event);
    }

    crtui_progress_set_value(d.ctx, d.progress, (int32_t)((frame * 3u) % 101u));
    crtui_result r = crtui_window_render(d.ctx, d.window, fb.pixels, fb.stride, (int32_t)fb.width, (int32_t)fb.height);
    if (r != CRTUI_OK) {
      fprintf(stderr, "crtui_window_demo: render failed (%d)\n", (int)r);
      failed = 1;
      break;
    }
    if (frame == 0) {
      /* Window background and button fill, read back from the framebuffer that is
       * about to be presented (button: column (20,20) + (0,40), so (26,66)). */
      pixel_check = rgb_at(&fb, 3, 3) == BACKGROUND_RGB && rgb_at(&fb, 26, 66) == BUTTON_RGB;
    }
    crtgfx_window_end_frame(window);
    ++frame;
    crtgfx_window_pump_events(16);
  }
  int input_check = 0;
  if (frame_limit != 0) {
    /* Tab focused the button; Enter activated it; the click on the slider set it near
     * the pointer (x=200 of a slider from 20 wide (width-80)); the wheel stepped it by one. */
    input_check = focus_after_tab && d.activations >= 1 && slider_after_click > 10 &&
                  slider_after_wheel == slider_after_click + 1;
    if (!input_check) {
      fprintf(stderr, "crtui_window_demo: focus_after_tab=%d activations=%d slider_after_click=%d slider_after_wheel=%d\n",
              focus_after_tab, d.activations, (int)slider_after_click, (int)slider_after_wheel);
    }
  }
  if (d.ctx != NULL) crtui_context_destroy(d.ctx);
  crtgfx_window_destroy(window);
  if (frame_limit != 0) {
    fprintf(stderr, "crtui_window_demo: presented=%lu pixel_check=%s input_check=%s\n", frame,
            pixel_check ? "pass" : "fail", input_check ? "pass" : "fail");
  } else {
    fprintf(stderr, "crtui_window_demo: presented=%lu pixel_check=%s\n", frame, pixel_check ? "pass" : "fail");
  }
  if (failed || (frame_limit != 0 && (frame != frame_limit || !pixel_check || !input_check))) {
    return 1;
  }
  return 0;
}
