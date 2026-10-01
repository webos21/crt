/* crtui window demo (Tranches 1-4): a Column lays out a label, a Row of
 * Button / Switch / Checkbox, a Slider, a Progress bar and a TextInput -- no
 * manual coordinates; the layout engine places everything and follows window
 * resizes -- rendered by the crtui software renderer into the crtgfx window
 * software framebuffer and presented through the crtgfx window path, with
 * crtgfx input events (keyboard, text, pointer, wheel, resize) fed to crtui
 * through the crtui/crtgfx.h adapter.
 *
 * Usage: crtui_window_demo [frame_limit]
 *   no limit    interactive: Tab/arrows/Enter/Space, typing into the text
 *               field, mouse clicks and drags, the wheel, and resizing all work.
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
  crtui_widget column, label, row, button, toggle, check, slider, progress, input;
  crtui_crtgfx_state input_state;
  int activations;
} demo;

static int on_event(crtui_widget current, const crtui_event* event, void* user) {
  demo* d = (demo*)user;
  (void)current;
  if (event->type == CRTUI_EVENT_ACTIVATE && event->target == d->button) { /* bubbled up to the window callback */
    ++d->activations;
  }
  return CRTUI_EVENT_IGNORED;
}

static int demo_build(demo* d, int32_t width, int32_t height) {
  crtui_style large;
  memset(d, 0, sizeof(*d));
  if (crtui_context_create(&d->ctx) != CRTUI_OK) return -1;
  if (crtui_window_create(d->ctx, &d->window) != CRTUI_OK) return -1;
  crtui_window_set_size(d->ctx, d->window, width, height, 1.0f);
  crtui_widget_set_callback(d->ctx, d->window, on_event, d);

  crtui_column_create(d->ctx, d->window, &d->column);
  crtui_widget_set_size(d->ctx, d->column, CRTUI_SIZE_FILL, CRTUI_SIZE_FILL);
  crtui_container_set_padding(d->ctx, d->column, 20, 20, 20, 20);
  crtui_container_set_gap(d->ctx, d->column, 12);

  crtui_text_create(d->ctx, d->column, "crtui on crtgfx", &d->label);
  crtui_widget_set_size(d->ctx, d->label, CRTUI_SIZE_FILL, 24);
  memset(&large, 0, sizeof(large));
  large.mask = CRTUI_STYLE_FONT;
  large.font = CRTUI_FONT_LARGE;
  crtui_widget_set_style(d->ctx, d->label, &large);

  crtui_row_create(d->ctx, d->column, &d->row);
  crtui_widget_set_size(d->ctx, d->row, CRTUI_SIZE_FILL, 36);
  crtui_container_set_gap(d->ctx, d->row, 12);
  crtui_button_create(d->ctx, d->row, "Button", &d->button);
  crtui_widget_set_size(d->ctx, d->button, 140, 36);
  crtui_switch_create(d->ctx, d->row, &d->toggle);
  crtui_widget_set_size(d->ctx, d->toggle, 50, 24);
  crtui_widget_set_alignment(d->ctx, d->toggle, CRTUI_ALIGN_START, CRTUI_ALIGN_CENTER);
  crtui_checkbox_create(d->ctx, d->row, "Option", &d->check);
  crtui_widget_set_size(d->ctx, d->check, 120, 24);
  crtui_widget_set_alignment(d->ctx, d->check, CRTUI_ALIGN_START, CRTUI_ALIGN_CENTER);

  crtui_slider_create(d->ctx, d->column, 0, 100, &d->slider);
  crtui_widget_set_size(d->ctx, d->slider, CRTUI_SIZE_FILL, 14);
  crtui_progress_create(d->ctx, d->column, &d->progress);
  crtui_widget_set_size(d->ctx, d->progress, CRTUI_SIZE_FILL, 16);
  crtui_text_input_create(d->ctx, d->column, "", &d->input);
  crtui_widget_set_size(d->ctx, d->input, CRTUI_SIZE_FILL, 28);
  crtui_text_input_set_placeholder(d->ctx, d->input, "Type here");
  return 0;
}

/* Window-space center of a widget (bounds are parent-relative). */
static void center_of(demo* d, crtui_widget w, double* x, double* y) {
  int32_t bx, by, bw, bh;
  double ox = 0, oy = 0;
  crtui_widget cursor = w;
  int first = 1;
  while (cursor != CRTUI_INVALID_WIDGET && cursor != d->window) {
    crtui_widget parent = CRTUI_INVALID_WIDGET;
    crtui_widget_get_bounds(d->ctx, cursor, &bx, &by, &bw, &bh);
    if (first) {
      ox += bw / 2;
      oy += bh / 2;
      first = 0;
    }
    ox += bx;
    oy += by;
    crtui_widget_get_parent(d->ctx, cursor, &parent);
    cursor = parent;
  }
  *x = ox;
  *y = oy;
}

static uint32_t rgb_at(const crtgfx_framebuffer* fb, uint32_t x, uint32_t y) {
  const unsigned char* p = (const unsigned char*)fb->pixels + (size_t)y * fb->stride + (size_t)x * 4u;
  return ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[0];
}

static void inject(crtgfx_window* window, crtgfx_event_type type, uint32_t keycode, double x, double y, double dy,
                   uint32_t button, const char* text) {
  crtgfx_event e;
  memset(&e, 0, sizeof(e));
  e.type = type;
  switch (type) {
    case CRTGFX_EVENT_KEY_DOWN:
    case CRTGFX_EVENT_KEY_UP:
      e.data.key.keycode = keycode;
      break;
    case CRTGFX_EVENT_TEXT:
      strncpy(e.data.text.utf8, text, sizeof(e.data.text.utf8) - 1);
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

static void inject_click(crtgfx_window* window, double x, double y) {
  inject(window, CRTGFX_EVENT_POINTER_MOTION, 0, x, y, 0, 0, NULL);
  inject(window, CRTGFX_EVENT_POINTER_BUTTON_DOWN, 0, x, y, 0, CRTGFX_POINTER_BUTTON_LEFT, NULL);
  inject(window, CRTGFX_EVENT_POINTER_BUTTON_UP, 0, x, y, 0, CRTGFX_POINTER_BUTTON_LEFT, NULL);
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
  int switch_after_click = 0;
  char typed[16] = "";
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
    {
      /* The framebuffer may change before the RESIZE event is polled; the layout
       * engine re-lays out the whole tree by itself. */
      int32_t w, h;
      float scale;
      crtui_window_get_size(d.ctx, d.window, &w, &h, &scale);
      if (w != (int32_t)fb.width || h != (int32_t)fb.height) {
        crtui_window_set_size(d.ctx, d.window, (int32_t)fb.width, (int32_t)fb.height, scale);
      }
    }

    if (frame_limit != 0) {
      /* Scripted input; click targets are computed from the laid-out bounds. */
      double x, y;
      if (frame == 2) {
        inject(window, CRTGFX_EVENT_KEY_DOWN, 15 /* Tab */, 0, 0, 0, 0, NULL);
        inject(window, CRTGFX_EVENT_KEY_UP, 15, 0, 0, 0, 0, NULL);
      } else if (frame == 4) {
        crtui_widget f = CRTUI_INVALID_WIDGET;
        crtui_context_get_focus(d.ctx, &f);
        focus_after_tab = f == d.button;
        inject(window, CRTGFX_EVENT_KEY_DOWN, 28 /* Enter */, 0, 0, 0, 0, NULL);
        inject(window, CRTGFX_EVENT_KEY_UP, 28, 0, 0, 0, 0, NULL);
      } else if (frame == 6) {
        center_of(&d, d.toggle, &x, &y);
        inject_click(window, x, y);
      } else if (frame == 8) {
        int checked = 0;
        crtui_toggle_get_checked(d.ctx, d.toggle, &checked);
        switch_after_click = checked;
        center_of(&d, d.slider, &x, &y);
        inject_click(window, x, y);
      } else if (frame == 10) {
        crtui_slider_get_value(d.ctx, d.slider, &slider_after_click);
        inject(window, CRTGFX_EVENT_POINTER_SCROLL, 0, 0, 0, 10.0, 0, NULL);
      } else if (frame == 12) {
        crtui_slider_get_value(d.ctx, d.slider, &slider_after_wheel);
        center_of(&d, d.input, &x, &y);
        inject_click(window, x, y);
        inject(window, CRTGFX_EVENT_TEXT, 0, 0, 0, 0, 0, "h");
        inject(window, CRTGFX_EVENT_TEXT, 0, 0, 0, 0, 0, "i");
      } else if (frame == 14) {
        size_t length = 0;
        crtui_widget_get_text(d.ctx, d.input, typed, sizeof(typed), &length);
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
       * about to be presented. */
      double bx, by;
      center_of(&d, d.button, &bx, &by);
      pixel_check = rgb_at(&fb, 3, 3) == BACKGROUND_RGB &&
                    rgb_at(&fb, (uint32_t)bx - 60, (uint32_t)by - 14) == BUTTON_RGB;
    }
    crtgfx_window_end_frame(window);
    ++frame;
    crtgfx_window_pump_events(16);
  }
  int input_check = 0;
  if (frame_limit != 0) {
    /* Tab focused the button and Enter activated it; a click flipped the switch; a
     * click on the slider set it near the pointer and the wheel stepped it by one;
     * a click on the text field focused it and TEXT events typed "hi". */
    input_check = focus_after_tab && d.activations >= 1 && switch_after_click == 1 && slider_after_click > 10 &&
                  slider_after_wheel == slider_after_click + 1 && strcmp(typed, "hi") == 0;
    if (!input_check) {
      fprintf(stderr,
              "crtui_window_demo: focus_after_tab=%d activations=%d switch=%d slider_after_click=%d "
              "slider_after_wheel=%d typed=\"%s\"\n",
              focus_after_tab, d.activations, switch_after_click, (int)slider_after_click, (int)slider_after_wheel,
              typed);
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
