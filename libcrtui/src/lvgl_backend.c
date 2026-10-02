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
#define CRTUI_COLOR_SWITCH_KNOB 0xFFFFFF
#define CRTUI_COLOR_INPUT_BG 0xFFFFFF
#define CRTUI_COLOR_INPUT_BORDER 0xA0A0A0
#define CRTUI_COLOR_INPUT_PLACEHOLDER 0x909090
#define CRTUI_COLOR_CHECK_BORDER 0x2D6CDF

struct crtui_lvgl_backend {
  lv_display_t* display;
  uint8_t* buffer_raw;
  uint8_t* buffer; /* 64-byte aligned view of buffer_raw */
  size_t buffer_size;
  int32_t width, height;
  uint64_t version;
  int has_version;
  uint32_t z_begin, z_end;
  int transparent;
  /* Image widgets: descriptors and pixel copies LVGL points into. They live
   * from one rebuild to the next (LVGL may keep referring to a source between
   * frames) and are released only after the objects using them are gone. */
  lv_image_dsc_t* images;
  uint8_t** image_data;
  size_t image_count;
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

static const lv_font_t* font_for(crtui_font font) {
  if (font == CRTUI_FONT_SMALL) return &lv_font_montserrat_12;
  if (font == CRTUI_FONT_LARGE) return &lv_font_montserrat_20;
  return &lv_font_montserrat_14;
}

/* Byte offset -> character index (the LVGL textarea cursor counts characters). */
static uint32_t char_index_for(const char* text, int32_t byte_offset) {
  uint32_t chars = 0;
  for (int32_t i = 0; text != NULL && i < byte_offset && text[i] != '\0'; ++i) {
    if (((unsigned char)text[i] & 0xC0u) != 0x80u) ++chars;
  }
  return chars;
}

static void release_images(crtui_lvgl_backend* backend) {
  for (size_t i = 0; i < backend->image_count; ++i) {
    free(backend->image_data[i]);
  }
  free(backend->image_data);
  free(backend->images);
  backend->image_data = NULL;
  backend->images = NULL;
  backend->image_count = 0;
}

/* Applies the widget's style overrides on top of the CRT default look. The
 * mapping (background = main fill, foreground = accent) is documented in
 * crtui/ui.h. */
static void apply_style(lv_obj_t* obj, const crtui_render_item* item) {
  const crtui_style* st = &item->style;
  uint32_t m = st->mask;
  if (m == 0) return;
  crtui_widget_kind kind = item->kind;
  if ((m & CRTUI_STYLE_BACKGROUND) != 0) {
    style_fill(obj, st->background_rgb, LV_PART_MAIN);
  }
  if ((m & CRTUI_STYLE_FOREGROUND) != 0) {
    lv_color_t fg = lv_color_hex(st->foreground_rgb);
    switch (kind) {
      case CRTUI_WIDGET_BUTTON: {
        lv_obj_t* label = lv_obj_get_child(obj, 0);
        if (label != NULL) lv_obj_set_style_text_color(label, fg, LV_PART_MAIN);
        break;
      }
      case CRTUI_WIDGET_SLIDER:
        style_fill(obj, st->foreground_rgb, LV_PART_INDICATOR);
        style_fill(obj, st->foreground_rgb, LV_PART_KNOB);
        break;
      case CRTUI_WIDGET_PROGRESS:
        style_fill(obj, st->foreground_rgb, LV_PART_INDICATOR);
        break;
      case CRTUI_WIDGET_SWITCH:
        style_fill(obj, st->foreground_rgb, (lv_part_t)((int)LV_PART_INDICATOR | (int)LV_STATE_CHECKED));
        break;
      case CRTUI_WIDGET_CHECKBOX:
        lv_obj_set_style_text_color(obj, fg, LV_PART_MAIN);
        style_fill(obj, st->foreground_rgb, (lv_part_t)((int)LV_PART_INDICATOR | (int)LV_STATE_CHECKED));
        break;
      default:
        lv_obj_set_style_text_color(obj, fg, LV_PART_MAIN);
        break;
    }
  }
  if ((m & CRTUI_STYLE_BORDER_COLOR) != 0) {
    lv_obj_set_style_border_color(obj, lv_color_hex(st->border_rgb), LV_PART_MAIN);
  }
  if ((m & CRTUI_STYLE_BORDER_WIDTH) != 0) {
    lv_obj_set_style_border_width(obj, st->border_width, LV_PART_MAIN);
    if (st->border_width > 0 && (m & CRTUI_STYLE_BORDER_COLOR) == 0) {
      lv_obj_set_style_border_color(obj, lv_color_hex(0x000000), LV_PART_MAIN);
    }
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  }
  if ((m & CRTUI_STYLE_RADIUS) != 0) {
    lv_obj_set_style_radius(obj, st->radius, LV_PART_MAIN);
    if (kind == CRTUI_WIDGET_SLIDER || kind == CRTUI_WIDGET_PROGRESS) {
      lv_obj_set_style_radius(obj, st->radius, LV_PART_INDICATOR);
    }
  }
  if ((m & CRTUI_STYLE_OPACITY) != 0) {
    lv_obj_set_style_opa(obj, st->opacity, LV_PART_MAIN);
  }
  if ((m & CRTUI_STYLE_FONT) != 0) {
    lv_obj_set_style_text_font(obj, font_for(st->font), LV_PART_MAIN);
    if (kind == CRTUI_WIDGET_BUTTON) {
      lv_obj_t* label = lv_obj_get_child(obj, 0);
      if (label != NULL) lv_obj_set_style_text_font(label, font_for(st->font), LV_PART_MAIN);
    }
  }
  if ((m & CRTUI_STYLE_TEXT_ALIGN) != 0) {
    lv_text_align_t align = st->text_align == CRTUI_TEXT_ALIGN_CENTER ? LV_TEXT_ALIGN_CENTER
                            : st->text_align == CRTUI_TEXT_ALIGN_END  ? LV_TEXT_ALIGN_RIGHT
                                                                       : LV_TEXT_ALIGN_LEFT;
    lv_obj_set_style_text_align(obj, align, LV_PART_MAIN);
  }
}

static lv_obj_t* create_placeholder(const crtui_render_item* item, lv_obj_t* parent) {
  lv_obj_t* obj = lv_obj_create(parent);
  if (obj == NULL) return NULL;
  style_plain(obj, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_scrollable(obj, false);
  lv_obj_set_pos(obj, item->x, item->y);
  lv_obj_set_size(obj, item->width, item->height);
  if (!item->visible) lv_obj_set_hidden(obj, true);
  return obj;
}

static lv_obj_t* create_object(crtui_lvgl_backend* backend, const crtui_render_item* item, lv_obj_t* parent) {
  lv_obj_t* obj = NULL;
  switch (item->kind) {
    case CRTUI_WIDGET_CONTAINER:
    case CRTUI_WIDGET_ROW:
    case CRTUI_WIDGET_COLUMN:
    case CRTUI_WIDGET_STACK:
    case CRTUI_WIDGET_SCROLL_VIEW:
    case CRTUI_WIDGET_LIST:
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
    case CRTUI_WIDGET_SWITCH: {
      lv_part_t indicator_on = (lv_part_t)((int)LV_PART_INDICATOR | (int)LV_STATE_CHECKED);
      obj = lv_switch_create(parent);
      style_plain(obj, LV_PART_MAIN);
      style_fill(obj, CRTUI_COLOR_TRACK, LV_PART_MAIN);
      lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
      style_plain(obj, LV_PART_INDICATOR);
      lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_INDICATOR);
      lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
      style_fill(obj, item->enabled ? CRTUI_COLOR_SLIDER_FILL : CRTUI_COLOR_DISABLED_BG, indicator_on);
      style_plain(obj, LV_PART_KNOB);
      style_fill(obj, CRTUI_COLOR_SWITCH_KNOB, LV_PART_KNOB);
      lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_KNOB);
      if (item->value != 0) lv_obj_add_state(obj, LV_STATE_CHECKED);
      break;
    }
    case CRTUI_WIDGET_CHECKBOX: {
      lv_part_t indicator_on = (lv_part_t)((int)LV_PART_INDICATOR | (int)LV_STATE_CHECKED);
      obj = lv_checkbox_create(parent);
      lv_checkbox_set_text(obj, item->text != NULL ? item->text : "");
      style_plain(obj, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_text_color(obj, lv_color_hex(CRTUI_COLOR_TEXT), LV_PART_MAIN);
      style_plain(obj, LV_PART_INDICATOR);
      style_fill(obj, CRTUI_COLOR_INPUT_BG, LV_PART_INDICATOR);
      lv_obj_set_style_border_width(obj, 2, LV_PART_INDICATOR);
      lv_obj_set_style_border_color(obj, lv_color_hex(CRTUI_COLOR_CHECK_BORDER), LV_PART_INDICATOR);
      lv_obj_set_style_radius(obj, 3, LV_PART_INDICATOR);
      style_fill(obj, CRTUI_COLOR_CHECK_BORDER, indicator_on);
      if (item->value != 0) lv_obj_add_state(obj, LV_STATE_CHECKED);
      break;
    }
    case CRTUI_WIDGET_TEXT_INPUT: {
      lv_part_t cursor_focused = (lv_part_t)((int)LV_PART_CURSOR | (int)LV_STATE_FOCUSED);
      obj = lv_textarea_create(parent);
      lv_textarea_set_one_line(obj, true);
      lv_textarea_set_text(obj, item->text != NULL ? item->text : "");
      if (item->placeholder != NULL) lv_textarea_set_placeholder_text(obj, item->placeholder);
      style_plain(obj, LV_PART_MAIN);
      style_fill(obj, CRTUI_COLOR_INPUT_BG, LV_PART_MAIN);
      lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
      lv_obj_set_style_border_color(obj, lv_color_hex(CRTUI_COLOR_INPUT_BORDER), LV_PART_MAIN);
      lv_obj_set_style_radius(obj, 4, LV_PART_MAIN);
      lv_obj_set_style_pad_left(obj, 6, LV_PART_MAIN);
      lv_obj_set_style_pad_right(obj, 6, LV_PART_MAIN);
      lv_obj_set_style_pad_top(obj, item->height > 22 ? (item->height - 22) / 2 : 0, LV_PART_MAIN);
      lv_obj_set_style_text_color(obj, lv_color_hex(CRTUI_COLOR_TEXT), LV_PART_MAIN);
      lv_obj_set_style_text_color(obj, lv_color_hex(CRTUI_COLOR_INPUT_PLACEHOLDER), LV_PART_TEXTAREA_PLACEHOLDER);
      /* A steady 2 px caret, only while the widget has the crtui focus. */
      lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_CURSOR);
      lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
      lv_obj_set_style_border_width(obj, 0, LV_PART_CURSOR);
      lv_obj_set_style_border_width(obj, 2, cursor_focused);
      lv_obj_set_style_border_color(obj, lv_color_hex(CRTUI_COLOR_TEXT), LV_PART_CURSOR);
      lv_obj_set_style_anim_duration(obj, 0, LV_PART_CURSOR);
      lv_textarea_set_cursor_pos(obj, (int32_t)char_index_for(item->text, item->caret));
      if (item->focused) lv_obj_add_state(obj, LV_STATE_FOCUSED);
      break;
    }
    case CRTUI_WIDGET_IMAGE:
      if (item->image_pixels != NULL && item->image_width > 0 && item->image_height > 0) {
        size_t bytes = (size_t)item->image_width * (size_t)item->image_height * 4u;
        lv_image_dsc_t* grown = (lv_image_dsc_t*)realloc(backend->images, (backend->image_count + 1) * sizeof(*grown));
        uint8_t** data = (uint8_t**)realloc(backend->image_data, (backend->image_count + 1) * sizeof(*data));
        if (grown != NULL) backend->images = grown;
        if (data != NULL) backend->image_data = data;
        uint8_t* copy = (uint8_t*)malloc(bytes);
        if (grown == NULL || data == NULL || copy == NULL) {
          free(copy);
          break;
        }
        memcpy(copy, item->image_pixels, bytes);
        lv_image_dsc_t* dsc = &backend->images[backend->image_count];
        memset(dsc, 0, sizeof(*dsc));
        dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
        dsc->header.cf = LV_COLOR_FORMAT_ARGB8888;
        dsc->header.w = (uint32_t)item->image_width;
        dsc->header.h = (uint32_t)item->image_height;
        dsc->header.stride = (uint32_t)item->image_width * 4u;
        dsc->data_size = (uint32_t)bytes;
        dsc->data = copy;
        backend->image_data[backend->image_count] = copy;
        ++backend->image_count;
        obj = lv_image_create(parent);
        lv_image_set_src(obj, dsc);
        /* Stretch to the widget's size with an explicit scale (256 = 100%) from
         * the top-left, instead of relying on the widget's own size tracking. */
        lv_image_set_inner_align(obj, LV_IMAGE_ALIGN_TOP_LEFT);
        lv_image_set_pivot(obj, 0, 0);
        lv_image_set_scale_x(obj, (uint32_t)((int64_t)256 * item->width / item->image_width));
        lv_image_set_scale_y(obj, (uint32_t)((int64_t)256 * item->height / item->image_height));
        style_plain(obj, LV_PART_MAIN);
      }
      break;
    default:
      /* SurfaceViews are final-compositor metadata, never LVGL objects: the
       * producer's pixels must not pass through this software framebuffer. */
      break;
  }
  if (obj != NULL) {
    apply_style(obj, item);
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
  release_images(backend);
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
  return crtui_lvgl_render_plane(
      out_backend, items, count, version, 0, UINT32_MAX, 0, pixels, stride_bytes);
}

crtui_result crtui_lvgl_render_plane(
    crtui_lvgl_backend** out_backend, const crtui_render_item* items, size_t count, uint64_t version,
    uint32_t z_begin, uint32_t z_end, int transparent, void* pixels, size_t stride_bytes) {
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
  if (!backend->has_version || backend->version != version || backend->z_begin != z_begin ||
      backend->z_end != z_end || backend->transparent != transparent) {
    lv_display_set_default(backend->display);
    lv_obj_t* screen = lv_display_get_screen_active(backend->display);
    lv_obj_clean(screen);
    release_images(backend);
    style_plain(screen, LV_PART_MAIN);
    if (!transparent && z_begin == 0 && z_end > 0) {
      style_fill(screen, CRTUI_COLOR_WINDOW_BG, LV_PART_MAIN);
    } else {
      lv_obj_set_style_bg_opa(screen, LV_OPA_TRANSP, LV_PART_MAIN);
    }
    lv_obj_set_scrollable(screen, false);
    memset(backend->buffer, 0, backend->buffer_size);
    lv_obj_t** objects = (lv_obj_t**)calloc(count, sizeof(*objects));
    if (objects == NULL) {
      return CRTUI_ERROR_IO;
    }
    objects[0] = screen;
    for (size_t i = 1; i < count; ++i) {
      lv_obj_t* parent = items[i].parent >= 0 ? objects[items[i].parent] : screen;
      int is_external_surface =
          items[i].kind == CRTUI_WIDGET_SURFACE_VIEW || items[i].kind == CRTUI_WIDGET_MEDIA_VIEW;
      int in_plane = i >= z_begin && i < z_end && !is_external_surface;
      objects[i] = parent == NULL ? NULL
                                  : (in_plane ? create_object(backend, &items[i], parent)
                                              : create_placeholder(&items[i], parent));
    }
    free(objects);
    lv_obj_invalidate(screen);
    lv_refr_now(backend->display);
    backend->version = version;
    backend->z_begin = z_begin;
    backend->z_end = z_end;
    backend->transparent = transparent;
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
  release_images(backend);
  free(backend->buffer_raw);
  free(backend);
  if (--lvgl_users == 0) {
    lv_deinit();
  }
}
