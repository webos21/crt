#include "wayland_weston_internal.h"

#include <stdlib.h>

#define CRTGFX_BYTES_PER_PIXEL 4u

static int crtgfx_weston_resize_software_buffer(crtgfx_weston_toplevel* toplevel) {
  uint32_t stride;
  uint32_t needed;
  uint8_t* buffer;

  if (toplevel == 0 || toplevel->width == 0 || toplevel->height == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  if (toplevel->width > UINT32_MAX / CRTGFX_BYTES_PER_PIXEL) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  stride = toplevel->width * CRTGFX_BYTES_PER_PIXEL;
  if (toplevel->height > UINT32_MAX / stride) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  needed = stride * toplevel->height;
  if (needed == toplevel->software_buffer_size && toplevel->software_buffer != 0) {
    toplevel->stride = stride;
    return CRTGFX_OK;
  }

  buffer = (uint8_t*)realloc(toplevel->software_buffer, needed);
  if (buffer == 0) {
    return CRTGFX_ERROR_HOST;
  }
  toplevel->software_buffer = buffer;
  toplevel->software_buffer_size = needed;
  toplevel->stride = stride;
  return CRTGFX_OK;
}

int crtgfx_weston_toplevel_create(const crtgfx_window_desc* desc, crtgfx_window** out_window) {
  crtgfx_window* window;
  int rc;

  if (desc == 0 || out_window == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }

  window = (crtgfx_window*)calloc(1, sizeof(*window));
  if (window == 0) {
    return CRTGFX_ERROR_HOST;
  }

  window->toplevel.width = desc->width;
  window->toplevel.height = desc->height;

  rc = crtgfx_host_window_create(desc, &window->toplevel);
  if (rc != CRTGFX_OK) {
    free(window);
    return rc;
  }

  *out_window = window;
  return CRTGFX_OK;
}

void crtgfx_weston_toplevel_destroy(crtgfx_window* window) {
  if (window->toplevel.host != 0) {
    crtgfx_host_window_destroy(window->toplevel.host);
    window->toplevel.host = 0;
  }
  free(window->toplevel.software_buffer);
  free(window);
}

int crtgfx_weston_toplevel_show(crtgfx_window* window) {
  if (window == 0 || window->toplevel.host == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  return crtgfx_host_window_show(window->toplevel.host);
}

int crtgfx_weston_display_dispatch(uint32_t timeout_ms) {
  return crtgfx_host_window_dispatch(timeout_ms);
}

int crtgfx_weston_toplevel_get_size(crtgfx_window* window, uint32_t* out_width, uint32_t* out_height) {
  int rc;

  if (window == 0 || out_width == 0 || out_height == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  if (window->toplevel.host != 0) {
    rc = crtgfx_host_window_get_size(window->toplevel.host, &window->toplevel.width,
                                     &window->toplevel.height);
    if (rc != CRTGFX_OK) {
      return rc;
    }
  }
  *out_width = window->toplevel.width;
  *out_height = window->toplevel.height;
  return CRTGFX_OK;
}

int crtgfx_weston_toplevel_should_close(crtgfx_window* window) {
  if (window == 0) {
    return 1;
  }
  return window->toplevel.should_close;
}

int crtgfx_weston_toplevel_begin_frame(crtgfx_window* window, crtgfx_framebuffer* out_framebuffer) {
  int rc;

  if (window == 0 || out_framebuffer == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  if (window->toplevel.frame_pending) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  rc = crtgfx_weston_toplevel_get_size(window, &window->toplevel.width, &window->toplevel.height);
  if (rc != CRTGFX_OK) {
    return rc;
  }
  rc = crtgfx_weston_resize_software_buffer(&window->toplevel);
  if (rc != CRTGFX_OK) {
    return rc;
  }

  out_framebuffer->pixels = window->toplevel.software_buffer;
  out_framebuffer->width = window->toplevel.width;
  out_framebuffer->height = window->toplevel.height;
  out_framebuffer->stride = window->toplevel.stride;
  out_framebuffer->format = CRTGFX_PIXEL_FORMAT_BGRA8888_PREMULTIPLIED;
  window->toplevel.frame_pending = 1;
  return CRTGFX_OK;
}

int crtgfx_weston_toplevel_end_frame(crtgfx_window* window) {
  if (window == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  if (!window->toplevel.frame_pending || window->toplevel.software_buffer == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  window->toplevel.frame_pending = 0;
  window->toplevel.frame_committed = 1;
  return crtgfx_host_window_present_software(window->toplevel.host, window->toplevel.software_buffer,
                                             window->toplevel.width, window->toplevel.height,
                                             window->toplevel.stride);
}

void crtgfx_weston_toplevel_note_size(crtgfx_weston_toplevel* toplevel, uint32_t width, uint32_t height) {
  if (toplevel != 0) {
    toplevel->width = width;
    toplevel->height = height;
    toplevel->frame_committed = 0;
  }
}

void crtgfx_weston_toplevel_note_close(crtgfx_weston_toplevel* toplevel) {
  if (toplevel != 0) {
    toplevel->should_close = 1;
  }
}

void crtgfx_weston_toplevel_note_event(crtgfx_weston_toplevel* toplevel, const crtgfx_event* event) {
  uint32_t tail;

  if (toplevel == 0 || event == 0) {
    return;
  }
  if (toplevel->event_queue_count >= CRTGFX_EVENT_QUEUE_CAPACITY) {
    /* Full: drop the event rather than overwrite the oldest still-unread
     * one -- an event lost because the caller fell behind should still
     * be the *newest* one lost, not a silent gap in the middle of
     * whatever the caller already started reading. */
    return;
  }
  tail = (toplevel->event_queue_head + toplevel->event_queue_count) % CRTGFX_EVENT_QUEUE_CAPACITY;
  toplevel->event_queue[tail] = *event;
  toplevel->event_queue_count += 1;
}

int crtgfx_weston_toplevel_poll_event(crtgfx_window* window, crtgfx_event* out_event) {
  crtgfx_weston_toplevel* toplevel;

  if (window == 0 || out_event == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  toplevel = &window->toplevel;
  if (toplevel->event_queue_count == 0) {
    out_event->type = CRTGFX_EVENT_NONE;
    return CRTGFX_OK;
  }
  *out_event = toplevel->event_queue[toplevel->event_queue_head];
  toplevel->event_queue_head = (toplevel->event_queue_head + 1) % CRTGFX_EVENT_QUEUE_CAPACITY;
  toplevel->event_queue_count -= 1;
  return CRTGFX_OK;
}
