#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct crtgfx_window crtgfx_window;

typedef enum crtgfx_result {
  CRTGFX_OK = 0,
  CRTGFX_ERROR_INVALID_ARGUMENT = -1,
  CRTGFX_ERROR_UNSUPPORTED = -2,
  CRTGFX_ERROR_HOST = -3,
} crtgfx_result;

enum {
  CRTGFX_WINDOW_VISIBLE = 1u << 0,
};

typedef struct crtgfx_window_desc {
  const char* title;
  uint32_t width;
  uint32_t height;
  uint32_t flags;
} crtgfx_window_desc;

typedef enum crtgfx_pixel_format {
  CRTGFX_PIXEL_FORMAT_BGRA8888_PREMULTIPLIED = 1,
} crtgfx_pixel_format;

typedef struct crtgfx_framebuffer {
  void* pixels;
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  crtgfx_pixel_format format;
} crtgfx_framebuffer;

/* Input events. Added 2026-08-24, Phase 2/3 of the "notepad-capability"
 * plan -- the first real input API this project has, so its own design
 * choices are recorded here rather than assumed obvious:
 *
 *  - keycode is a Linux evdev keycode (linux/input-event-codes.h's own
 *    KEY_* numbering, e.g. KEY_A=30, KEY_ESC=1) on every host, not a raw
 *    per-platform code -- chosen because it is this project's own first
 *    real host backend to receive keyboard input at all (Linux/Wayland,
 *    this session), and wl_keyboard::key's own raw value already *is* an
 *    evdev keycode (the wire protocol's own "add 8 for the xkb keycode"
 *    convention is an xkbcommon-specific offset, not an evdev one -- see
 *    src/arch/linux/window_wayland.c's own keymap-handling comment).
 *    Windows/macOS backends (not implemented yet) are expected to
 *    translate their own VK_* / kVK_* codes into this same evdev numbering
 *    via a real per-platform lookup table when they add keyboard support,
 *    matching how every cross-platform input library (SDL2, GLFW, ...)
 *    normalizes onto one canonical keycode space rather than leaking each
 *    host's own numbering into the public API.
 *  - CRTGFX_EVENT_TEXT carries real, already-composed UTF-8 text (the
 *    result of feeding a keycode through the active XKB keymap -- see
 *    xkbcommon's own xkb_state_key_get_utf8()), not a raw keysym --
 *    correct out of the box for non-Latin/dead-key/AltGr layouts, which a
 *    keycode-only API can never get right on its own. A key that produces
 *    no text (Escape, arrow keys, function keys, a bare modifier press)
 *    only ever generates KEY_DOWN/KEY_UP, never a TEXT event.
 *  - modifiers is a small, host-independent bitmask (CRTGFX_MOD_*), not
 *    the raw XKB/Win32 modifier state -- each backend translates its own
 *    native modifier representation into this bitmask once, at the point
 *    it queues the event, so nothing above this API ever needs to know
 *    which host produced it. */
typedef enum crtgfx_event_type {
  CRTGFX_EVENT_NONE = 0,
  CRTGFX_EVENT_KEY_DOWN = 1,
  CRTGFX_EVENT_KEY_UP = 2,
  CRTGFX_EVENT_TEXT = 3,
  CRTGFX_EVENT_POINTER_MOTION = 4,
  CRTGFX_EVENT_POINTER_BUTTON_DOWN = 5,
  CRTGFX_EVENT_POINTER_BUTTON_UP = 6,
} crtgfx_event_type;

enum {
  CRTGFX_MOD_SHIFT = 1u << 0,
  CRTGFX_MOD_CTRL = 1u << 1,
  CRTGFX_MOD_ALT = 1u << 2,
  CRTGFX_MOD_SUPER = 1u << 3,
};

/* button numbering matches the real evdev/X11 convention every other
 * cross-platform input library already uses: 1=left, 2=right, 3=middle. */
enum {
  CRTGFX_POINTER_BUTTON_LEFT = 1u,
  CRTGFX_POINTER_BUTTON_RIGHT = 2u,
  CRTGFX_POINTER_BUTTON_MIDDLE = 3u,
};

typedef struct crtgfx_event {
  crtgfx_event_type type;
  union {
    struct {
      uint32_t keycode;
      uint32_t modifiers;
    } key;
    struct {
      /* NUL-terminated UTF-8 for one committed character/grapheme; 8
       * bytes is enough for any real single Unicode grapheme cluster
       * xkbcommon's own xkb_state_key_get_utf8() can produce for a
       * single key press (that call itself is bounded the same way). */
      char utf8[8];
    } text;
    struct {
      double x;
      double y;
    } pointer_motion;
    struct {
      uint32_t button;
      double x;
      double y;
    } pointer_button;
  } data;
} crtgfx_event;

int crtgfx_window_create(const crtgfx_window_desc* desc, crtgfx_window** out_window);
void crtgfx_window_destroy(crtgfx_window* window);
int crtgfx_window_show(crtgfx_window* window);
int crtgfx_window_pump_events(uint32_t timeout_ms);
int crtgfx_window_get_size(crtgfx_window* window, uint32_t* out_width, uint32_t* out_height);
int crtgfx_window_should_close(crtgfx_window* window);
int crtgfx_window_begin_frame(crtgfx_window* window, crtgfx_framebuffer* out_framebuffer);
int crtgfx_window_end_frame(crtgfx_window* window);
/* Pops the oldest queued input event into *out_event. Returns CRTGFX_OK
 * with out_event->type != CRTGFX_EVENT_NONE if an event was popped,
 * CRTGFX_OK with out_event->type == CRTGFX_EVENT_NONE if the queue was
 * empty, or an error code. Call after crtgfx_window_pump_events(), which
 * is what actually receives and queues new native events -- this only
 * drains the queue crtgfx_window_pump_events() already filled. */
int crtgfx_window_poll_event(crtgfx_window* window, crtgfx_event* out_event);

#ifdef __cplusplus
}
#endif
