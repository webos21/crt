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
  int rc;

  if (window == 0 || window->toplevel.host == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  rc = crtgfx_host_window_show(window->toplevel.host);
  /* CRTGFX_EVENT_EXPOSE: queued exactly once, on this window's first
   * successful show() -- see crtgfx/window.h's own doc comment on
   * CRTGFX_EVENT_EXPOSE for why this narrower "you are now mapped"
   * semantic replaces X11-style damage-driven Expose here. A later
   * show() call on an already-shown window (harmless on every backend --
   * see e.g. crtgfx_host_window_show()'s own Linux comment) does not
   * re-fire it. */
  if (rc == CRTGFX_OK && !window->toplevel.expose_sent) {
    crtgfx_event event = {0};
    event.type = CRTGFX_EVENT_EXPOSE;
    crtgfx_weston_toplevel_note_event(&window->toplevel, &event);
    window->toplevel.expose_sent = 1;
  }
  return rc;
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
  if (toplevel == 0) {
    return;
  }
  /* CRTGFX_EVENT_RESIZE only on a genuine change -- every backend calls
   * this from its own native resize notification (Linux xdg_toplevel::
   * configure, Windows WM_SIZE, macOS windowDidResize:), and at least
   * Windows/macOS can and do call it with a size that already matches
   * (e.g. crtgfx_host_window_get_size()'s own on-demand GetClientRect()
   * re-check) -- queuing an event every single time would misrepresent
   * "the size changed" as "get_size() was polled". */
  if (toplevel->width != width || toplevel->height != height) {
    crtgfx_event event = {0};
    event.type = CRTGFX_EVENT_RESIZE;
    event.data.resize.width = width;
    event.data.resize.height = height;
    crtgfx_weston_toplevel_note_event(toplevel, &event);
  }
  toplevel->width = width;
  toplevel->height = height;
  toplevel->frame_committed = 0;
}

void crtgfx_weston_toplevel_note_close(crtgfx_weston_toplevel* toplevel) {
  crtgfx_event event = {0};

  if (toplevel == 0) {
    return;
  }
  toplevel->should_close = 1;
  /* Additive: crtgfx_window_should_close()'s own polling contract is
   * unchanged (still just reads the flag above) -- this only gives an
   * event-driven caller the same signal through the queue instead of a
   * separate poll, see crtgfx/window.h's own CRTGFX_EVENT_CLOSE_REQUESTED
   * doc comment. */
  event.type = CRTGFX_EVENT_CLOSE_REQUESTED;
  crtgfx_weston_toplevel_note_event(toplevel, &event);
}

void crtgfx_weston_toplevel_note_focus(crtgfx_weston_toplevel* toplevel, int focused) {
  crtgfx_event event = {0};

  if (toplevel == 0) {
    return;
  }
  event.type = focused ? CRTGFX_EVENT_FOCUS_IN : CRTGFX_EVENT_FOCUS_OUT;
  crtgfx_weston_toplevel_note_event(toplevel, &event);
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
