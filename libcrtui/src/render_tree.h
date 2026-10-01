#pragma once

/* Private seam between crtui's CRT-owned model (core.c) and its LVGL renderer
 * (lvgl_backend.c). core.c flattens one window's widget tree, in pre-order,
 * into an array of these items; the backend turns that array into LVGL objects
 * and renders it. No LVGL type crosses this header, and core.c includes no
 * LVGL header. Never installed. */

#include <stddef.h>
#include <stdint.h>

#include "crtui/ui.h"

typedef struct crtui_render_item {
  crtui_widget_kind kind;
  int32_t parent; /* index of the parent item, -1 for the window (item 0) */
  int32_t x, y, width, height; /* relative to the parent; the window item carries its size */
  int visible;
  int enabled; /* effective: false if this widget or any ancestor is disabled */
  const char* text; /* borrowed, may be NULL */
  int32_t value, min_value, max_value;
  int focused; /* this widget holds the context focus */
  int pressed; /* a pointer press is in progress on this widget */
  crtui_style style;         /* the widget's style overrides (mask says which) */
  const char* placeholder;   /* TextInput, borrowed, may be NULL */
  int32_t caret;             /* TextInput: caret as a byte offset */
  const uint8_t* image_pixels; /* Image: BGRA8888 straight alpha, tightly packed, borrowed */
  int32_t image_width, image_height;
} crtui_render_item;

typedef struct crtui_lvgl_backend crtui_lvgl_backend;

/* Renders `items` (item 0 is the window) into `pixels` as BGRA8888 (bytes
 * B,G,R,A: the same layout as crtgfx_pixel_format BGRA8888_PREMULTIPLIED; the
 * window is opaque, so premultiplication is a no-op). `*backend` is created on
 * first use and is rebuilt from `items` whenever `version` changes. */
crtui_result crtui_lvgl_render(
    crtui_lvgl_backend** backend, const crtui_render_item* items, size_t count, uint64_t version, void* pixels,
    size_t stride_bytes);

/* Same renderer, restricted to the pre-order interval [z_begin, z_end).
 * Items outside the interval remain as transparent geometry-only parents so
 * LVGL preserves descendant coordinates and ancestor clipping. */
crtui_result crtui_lvgl_render_plane(
    crtui_lvgl_backend** backend, const crtui_render_item* items, size_t count, uint64_t version,
    uint32_t z_begin, uint32_t z_end, int transparent, void* pixels, size_t stride_bytes);

void crtui_lvgl_backend_destroy(crtui_lvgl_backend* backend);
