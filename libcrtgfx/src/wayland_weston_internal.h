#pragma once

#include "crtgfx/window.h"

typedef struct crtgfx_host_window crtgfx_host_window;

typedef struct crtgfx_weston_toplevel {
  crtgfx_host_window* host;
  uint8_t* software_buffer;
  uint32_t software_buffer_size;
  uint32_t stride;
  uint32_t width;
  uint32_t height;
  uint32_t frame_pending;
  uint32_t frame_committed;
  int should_close;
} crtgfx_weston_toplevel;

struct crtgfx_window {
  crtgfx_weston_toplevel toplevel;
};

int crtgfx_weston_toplevel_create(const crtgfx_window_desc* desc, crtgfx_window** out_window);
void crtgfx_weston_toplevel_destroy(crtgfx_window* window);
int crtgfx_weston_toplevel_show(crtgfx_window* window);
int crtgfx_weston_display_dispatch(uint32_t timeout_ms);
int crtgfx_weston_toplevel_get_size(crtgfx_window* window, uint32_t* out_width, uint32_t* out_height);
int crtgfx_weston_toplevel_should_close(crtgfx_window* window);
int crtgfx_weston_toplevel_begin_frame(crtgfx_window* window, crtgfx_framebuffer* out_framebuffer);
int crtgfx_weston_toplevel_end_frame(crtgfx_window* window);

void crtgfx_weston_toplevel_note_size(crtgfx_weston_toplevel* toplevel, uint32_t width, uint32_t height);
void crtgfx_weston_toplevel_note_close(crtgfx_weston_toplevel* toplevel);

int crtgfx_host_window_create(const crtgfx_window_desc* desc, crtgfx_weston_toplevel* toplevel);
void crtgfx_host_window_destroy(crtgfx_host_window* host);
int crtgfx_host_window_show(crtgfx_host_window* host);
int crtgfx_host_window_dispatch(uint32_t timeout_ms);
int crtgfx_host_window_get_size(crtgfx_host_window* host, uint32_t* out_width, uint32_t* out_height);
int crtgfx_host_window_present_software(
    crtgfx_host_window* host, const void* pixels, uint32_t width, uint32_t height, uint32_t stride);
