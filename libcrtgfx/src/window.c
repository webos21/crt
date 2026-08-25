#include "crtgfx/window.h"

#include "wayland_weston_internal.h"

int crtgfx_window_create(const crtgfx_window_desc* desc, crtgfx_window** out_window) {
  crtgfx_window_desc normalized;

  if (out_window == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  *out_window = 0;

  normalized.title = "crtgfx";
  normalized.width = 640;
  normalized.height = 480;
  normalized.flags = 0;
  if (desc != 0) {
    normalized = *desc;
    if (normalized.title == 0) {
      normalized.title = "crtgfx";
    }
    if (normalized.width == 0) {
      normalized.width = 640;
    }
    if (normalized.height == 0) {
      normalized.height = 480;
    }
  }

  return crtgfx_weston_toplevel_create(&normalized, out_window);
}

void crtgfx_window_destroy(crtgfx_window* window) {
  if (window != 0) {
    crtgfx_weston_toplevel_destroy(window);
  }
}

int crtgfx_window_show(crtgfx_window* window) {
  if (window == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  return crtgfx_weston_toplevel_show(window);
}

int crtgfx_window_pump_events(uint32_t timeout_ms) {
  return crtgfx_weston_display_dispatch(timeout_ms);
}

int crtgfx_window_get_size(crtgfx_window* window, uint32_t* out_width, uint32_t* out_height) {
  if (window == 0 || out_width == 0 || out_height == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  return crtgfx_weston_toplevel_get_size(window, out_width, out_height);
}

int crtgfx_window_should_close(crtgfx_window* window) {
  if (window == 0) {
    return 1;
  }
  return crtgfx_weston_toplevel_should_close(window);
}

int crtgfx_window_begin_frame(crtgfx_window* window, crtgfx_framebuffer* out_framebuffer) {
  if (window == 0 || out_framebuffer == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  return crtgfx_weston_toplevel_begin_frame(window, out_framebuffer);
}

int crtgfx_window_end_frame(crtgfx_window* window) {
  if (window == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  return crtgfx_weston_toplevel_end_frame(window);
}

int crtgfx_window_poll_event(crtgfx_window* window, crtgfx_event* out_event) {
  if (window == 0 || out_event == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  return crtgfx_weston_toplevel_poll_event(window, out_event);
}
