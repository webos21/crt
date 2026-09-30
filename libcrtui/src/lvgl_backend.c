/* crtui's private LVGL software renderer: the "CRT display adapter" of
 * docs/crtui_acceptance.md Tranche 1. LVGL draws into a CPU buffer (software
 * draw unit only, no GPU backend); the result is copied out as BGRA8888 for
 * the caller to hand to crtgfx. Every widget is styled explicitly here, so the
 * look is defined by CRT code and does not depend on an LVGL theme. */

#include "render_tree.h"

#include <lvgl/lvgl.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* The fixed default look (0xRRGGBB). crtui_acceptance.md lists the same values
 * so the pixel test and any later theme work have one source of truth. */
#define CRTUI_COLOR_WINDOW_BG 0xF0F0F0
#define CRTUI_COLOR_TEXT 0x202020
#define CRTUI_COLOR_BUTTON_BG 0x2D6CDF
#define CRTUI_COLOR_BUTTON_TEXT 0xFFFFFF
#define CRTUI_COLOR_DISABLED_BG 0xB0B0B0
#define CRTUI_COLOR_TRACK 0xC8C8C8
#define CRTUI_COLOR_SLIDER_FILL 0x2D6CDF
#define CRTUI_COLOR_SLIDER_KNOB 0x1E4FA8
#define CRTUI_COLOR_PROGRESS_FILL 0x2ECC71
#define CRTUI_COLOR_BUTTON_PRESSED 0x1E4FA8
#define CRTUI_COLOR_FOCUS_RING 0xFF9800

struct crtui_lvgl_backend {
  lv_display_t* display;
  uint8_t* buffer_raw;
  uint8_t* buffer; /* 64-byte aligned view of buffer_raw */
  size_t buffer_size;
  int32_t width, height;
  uint64_t version;
  int has_version;
};

/* LVGL keeps process-wide state: initialize on the first backend, deinitialize
 * when the last one is destroyed. */
static int lvgl_users = 0;

static uint32_t crtui_tick_ms(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint32_t)((uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u);
}

static void crtui_flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
  (void)area;
  (void)pixels; /* the persistent buffer is copied out by crtui_lvgl_render() */
  lv_display_flush_ready(display);
}

static void style_plain(lv_obj_t* obj, lv_part_t part) {
  lv_obj_set_style_border_width(obj, 0, part);
  lv_obj_set_style_outline_width(obj, 0, part);
  lv_obj_set_style_shadow_width(obj, 0, part);
  lv_obj_set_style_pad_all(obj, 0, part);
  lv_obj_set_style_radius(obj, 0, part);
}

static void style_fill(lv_obj_t* obj, uint32_t rgb, lv_part_t part) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(rgb), part);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, part);
}

static lv_obj_t* create_object(const crtui_render_item* item, lv_obj_t* parent) {
  lv_obj_t* obj = NULL;
  switch (item->kind) {
    case CRTUI_WIDGET_CONTAINER:
      obj = lv_obj_create(parent);
      style_plain(obj, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
      break;
    case CRTUI_WIDGET_TEXT:
      obj = lv_label_create(parent);
      lv_label_set_text(obj, item->text != NULL ? item->text : "");
      lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_CLIP);
      lv_obj_set_style_text_color(obj, lv_color_hex(CRTUI_COLOR_TEXT), LV_PART_MAIN);
      break;
    case CRTUI_WIDGET_BUTTON: {
      obj = lv_button_create(parent);
      style_plain(obj, LV_PART_MAIN);
      lv_obj_set_style_radius(obj, 6, LV_PART_MAIN);
      style_fill(obj,
                 !item->enabled ? CRTUI_COLOR_DISABLED_BG
                                : (item->pressed ? CRTUI_COLOR_BUTTON_PRESSED : CRTUI_COLOR_BUTTON_BG),
                 LV_PART_MAIN);
      lv_obj_t* label = lv_label_create(obj);
      lv_label_set_text(label, item->text != NULL ? item->text : "");
      lv_obj_set_style_text_color(label, lv_color_hex(CRTUI_COLOR_BUTTON_TEXT), LV_PART_MAIN);
      lv_obj_center(label);
      break;
    }
    case CRTUI_WIDGET_SLIDER:
      obj = lv_slider_create(parent);
      lv_slider_set_range(obj, item->min_value, item->max_value);
      lv_slider_set_value(obj, item->value, LV_ANIM_OFF);
      style_plain(obj, LV_PART_MAIN);
      style_fill(obj, CRTUI_COLOR_TRACK, LV_PART_MAIN);
      lv_obj_set_style_radius(obj, 4, LV_PART_MAIN);
      style_plain(obj, LV_PART_INDICATOR);
      style_fill(obj, item->enabled ? CRTUI_COLOR_SLIDER_FILL : CRTUI_COLOR_DISABLED_BG, LV_PART_INDICATOR);
      lv_obj_set_style_radius(obj, 4, LV_PART_INDICATOR);
      style_plain(obj, LV_PART_KNOB);
      style_fill(obj, CRTUI_COLOR_SLIDER_KNOB, LV_PART_KNOB);
      lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_KNOB);
      break;
    case CRTUI_WIDGET_PROGRESS:
      obj = lv_bar_create(parent);
      lv_bar_set_range(obj, item->min_value, item->max_value);
      lv_bar_set_value(obj, item->value, LV_ANIM_OFF);
      style_plain(obj, LV_PART_MAIN);
      style_fill(obj, CRTUI_COLOR_TRACK, LV_PART_MAIN);
      lv_obj_set_style_radius(obj, 4, LV_PART_MAIN);
      style_plain(obj, LV_PART_INDICATOR);
      style_fill(obj, CRTUI_COLOR_PROGRESS_FILL, LV_PART_INDICATOR);
      lv_obj_set_style_radius(obj, 4, LV_PART_INDICATOR);
      break;
    default:
      break; /* surface views arrive in Tranche 5 */
  }
  if (obj != NULL && item->focused) {
    /* The focus ring: a 2 px outline just outside the widget, drawn on the
     * main part so it follows the widget's own rounding. */
    lv_obj_set_style_outline_width(obj, 2, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_outline_color(obj, lv_color_hex(CRTUI_COLOR_FOCUS_RING), LV_PART_MAIN);
    lv_obj_set_style_outline_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  }
  if (obj != NULL) {
    lv_obj_set_scrollable(obj, false);
    lv_obj_set_pos(obj, item->x, item->y);
    lv_obj_set_size(obj, item->width, item->height);
    if (!item->visible) {
      lv_obj_set_hidden(obj, true);
    }
  }
  return obj;
}

static crtui_result backend_resize(crtui_lvgl_backend* backend, int32_t width, int32_t height) {
  if (backend->display != NULL && backend->width == width && backend->height == height) {
    return CRTUI_OK;
  }
  if (backend->display != NULL) {
    lv_display_delete(backend->display);
    backend->display = NULL;
  }
  free(backend->buffer_raw);
  backend->buffer_raw = NULL;
  backend->buffer = NULL;
  size_t size = (size_t)width * (size_t)height * 4u;
  backend->buffer_raw = (uint8_t*)calloc(1, size + 64u);
  if (backend->buffer_raw == NULL) {
    return CRTUI_ERROR_IO;
  }
  backend->buffer = (uint8_t*)(((uintptr_t)backend->buffer_raw + 63u) & ~(uintptr_t)63u);
  backend->buffer_size = size;
  backend->display = lv_display_create(width, height);
  if (backend->display == NULL) {
    return CRTUI_ERROR_IO;
  }
  lv_display_set_color_format(backend->display, LV_COLOR_FORMAT_ARGB8888);
  lv_display_set_buffers(backend->display, backend->buffer, NULL, (uint32_t)size, LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(backend->display, crtui_flush);
  backend->width = width;
  backend->height = height;
  backend->has_version = 0;
  return CRTUI_OK;
}

crtui_result crtui_lvgl_render(
    crtui_lvgl_backend** out_backend, const crtui_render_item* items, size_t count, uint64_t version, void* pixels,
    size_t stride_bytes) {
  if (out_backend == NULL || items == NULL || count == 0 || pixels == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  crtui_lvgl_backend* backend = *out_backend;
  if (backend == NULL) {
    backend = (crtui_lvgl_backend*)calloc(1, sizeof(*backend));
    if (backend == NULL) {
      return CRTUI_ERROR_IO;
    }
    if (lvgl_users++ == 0) {
      lv_init();
      lv_tick_set_cb(crtui_tick_ms);
    }
    *out_backend = backend;
  }
  crtui_result result = backend_resize(backend, items[0].width, items[0].height);
  if (result != CRTUI_OK) {
    return result;
  }
  if (!backend->has_version || backend->version != version) {
    lv_display_set_default(backend->display);
    lv_obj_t* screen = lv_display_get_screen_active(backend->display);
    lv_obj_clean(screen);
    style_plain(screen, LV_PART_MAIN);
    style_fill(screen, CRTUI_COLOR_WINDOW_BG, LV_PART_MAIN);
    lv_obj_set_scrollable(screen, false);
    lv_obj_t** objects = (lv_obj_t**)calloc(count, sizeof(*objects));
    if (objects == NULL) {
      return CRTUI_ERROR_IO;
    }
    objects[0] = screen;
    for (size_t i = 1; i < count; ++i) {
      lv_obj_t* parent = items[i].parent >= 0 ? objects[items[i].parent] : screen;
      objects[i] = parent != NULL ? create_object(&items[i], parent) : NULL;
    }
    free(objects);
    lv_obj_invalidate(screen);
    lv_refr_now(backend->display);
    backend->version = version;
    backend->has_version = 1;
  }
  if (stride_bytes == (size_t)backend->width * 4u) {
    memcpy(pixels, backend->buffer, backend->buffer_size);
  } else {
    for (int32_t row = 0; row < backend->height; ++row) {
      memcpy((uint8_t*)pixels + (size_t)row * stride_bytes, backend->buffer + (size_t)row * backend->width * 4u,
             (size_t)backend->width * 4u);
    }
  }
  return CRTUI_OK;
}

void crtui_lvgl_backend_destroy(crtui_lvgl_backend* backend) {
  if (backend == NULL) {
    return;
  }
  if (backend->display != NULL) {
    lv_display_delete(backend->display);
  }
  free(backend->buffer_raw);
  free(backend);
  if (--lvgl_users == 0) {
    lv_deinit();
  }
}
