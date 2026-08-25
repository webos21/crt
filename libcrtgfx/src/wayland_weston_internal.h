#pragma once

#include "crtgfx/window.h"

typedef struct crtgfx_host_window crtgfx_host_window;

/* Fixed-size ring buffer, not a growable queue: input events are drained
 * every frame by any real event loop (crtgfx_window_pump_events()
 * followed by crtgfx_window_poll_event() until empty), so unbounded
 * growth would only ever mean a caller that stopped polling -- a real
 * bug on the caller's own side a fixed capacity surfaces (oldest events
 * silently dropped once full) rather than hides behind an ever-growing
 * allocation. 64 matches this project's own CRTGFX_WL_MAX_MSG-style
 * "generous but bounded" sizing convention elsewhere in this backend;
 * a single key press can enqueue at most 2 events (KEY_DOWN + TEXT), so
 * this comfortably covers a burst of fast typing/mouse movement between
 * two pump_events() calls. */
#define CRTGFX_EVENT_QUEUE_CAPACITY 64u

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
  crtgfx_event event_queue[CRTGFX_EVENT_QUEUE_CAPACITY];
  uint32_t event_queue_head;
  uint32_t event_queue_count;
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
int crtgfx_weston_toplevel_poll_event(crtgfx_window* window, crtgfx_event* out_event);

void crtgfx_weston_toplevel_note_size(crtgfx_weston_toplevel* toplevel, uint32_t width, uint32_t height);
void crtgfx_weston_toplevel_note_close(crtgfx_weston_toplevel* toplevel);
/* Backend-facing push side of the event queue: any arch backend calls
 * this once per real native input event it receives (from its own
 * wl_keyboard/WM_KEYDOWN/NSEvent handler, ...), already translated into
 * this project's own host-independent crtgfx_event shape -- see crtgfx/
 * window.h's own design comment on crtgfx_event for why that translation
 * belongs in the backend, not here. Silently drops the event if the
 * queue is already full (see CRTGFX_EVENT_QUEUE_CAPACITY's own comment).
 * crtgfx_weston_toplevel_poll_event() (declared just above) is the pop
 * side, wrapped by the public crtgfx_window_poll_event() in window.c
 * the same way every other crtgfx_window_* function wraps its own
 * crtgfx_weston_toplevel_* counterpart. */
void crtgfx_weston_toplevel_note_event(crtgfx_weston_toplevel* toplevel, const crtgfx_event* event);

int crtgfx_host_window_create(const crtgfx_window_desc* desc, crtgfx_weston_toplevel* toplevel);
void crtgfx_host_window_destroy(crtgfx_host_window* host);
int crtgfx_host_window_show(crtgfx_host_window* host);
int crtgfx_host_window_dispatch(uint32_t timeout_ms);
int crtgfx_host_window_get_size(crtgfx_host_window* host, uint32_t* out_width, uint32_t* out_height);
int crtgfx_host_window_present_software(
    crtgfx_host_window* host, const void* pixels, uint32_t width, uint32_t height, uint32_t stride);
