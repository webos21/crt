#include "wayland_weston_internal.h"

struct crtgfx_host_window {
  int unused;
};

int crtgfx_host_window_create(const crtgfx_window_desc* desc, crtgfx_weston_toplevel* toplevel) {
  (void)desc;
  (void)toplevel;
  return CRTGFX_ERROR_UNSUPPORTED;
}

void crtgfx_host_window_destroy(crtgfx_host_window* host) {
  (void)host;
}

int crtgfx_host_window_show(crtgfx_host_window* host) {
  (void)host;
  return CRTGFX_ERROR_UNSUPPORTED;
}

int crtgfx_host_window_dispatch(uint32_t timeout_ms) {
  (void)timeout_ms;
  return CRTGFX_ERROR_UNSUPPORTED;
}

int crtgfx_host_window_get_size(crtgfx_host_window* host, uint32_t* out_width, uint32_t* out_height) {
  (void)host;
  (void)out_width;
  (void)out_height;
  return CRTGFX_ERROR_UNSUPPORTED;
}

int crtgfx_host_window_present_software(
    crtgfx_host_window* host, const void* pixels, uint32_t width, uint32_t height, uint32_t stride) {
  (void)host;
  (void)pixels;
  (void)width;
  (void)height;
  (void)stride;
  return CRTGFX_ERROR_UNSUPPORTED;
}
