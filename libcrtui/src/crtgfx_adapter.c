/* crtui <-> crtgfx input adapter: see crtui/crtgfx.h. Reads crtgfx_event values;
 * never calls into crtgfx. */

#include "crtui/crtgfx.h"

#include <string.h>

/* Linux evdev keycodes (linux/input-event-codes.h), the numbering crtgfx
 * documents for every host. */
enum {
  EVDEV_KEY_ESC = 1,
  EVDEV_KEY_BACKSPACE = 14,
  EVDEV_KEY_TAB = 15,
  EVDEV_KEY_ENTER = 28,
  EVDEV_KEY_SPACE = 57,
  EVDEV_KEY_KPENTER = 96,
  EVDEV_KEY_UP = 103,
  EVDEV_KEY_HOME = 102,
  EVDEV_KEY_END = 107,
  EVDEV_KEY_DELETE = 111,
  EVDEV_KEY_LEFT = 105,
  EVDEV_KEY_RIGHT = 106,
  EVDEV_KEY_DOWN = 108
};

static crtui_key map_key(uint32_t keycode) {
  switch (keycode) {
    case EVDEV_KEY_TAB:
      return CRTUI_KEY_TAB;
    case EVDEV_KEY_ENTER:
    case EVDEV_KEY_KPENTER:
      return CRTUI_KEY_ENTER;
    case EVDEV_KEY_SPACE:
      return CRTUI_KEY_SPACE;
    case EVDEV_KEY_LEFT:
      return CRTUI_KEY_LEFT;
    case EVDEV_KEY_RIGHT:
      return CRTUI_KEY_RIGHT;
    case EVDEV_KEY_UP:
      return CRTUI_KEY_UP;
    case EVDEV_KEY_DOWN:
      return CRTUI_KEY_DOWN;
    case EVDEV_KEY_ESC:
      return CRTUI_KEY_ESCAPE;
    case EVDEV_KEY_BACKSPACE:
      return CRTUI_KEY_BACKSPACE;
    case EVDEV_KEY_DELETE:
      return CRTUI_KEY_DELETE;
    case EVDEV_KEY_HOME:
      return CRTUI_KEY_HOME;
    case EVDEV_KEY_END:
      return CRTUI_KEY_END;
    default:
      return CRTUI_KEY_NONE;
  }
}

/* Round to nearest, halves away from zero, without libm. */
static int32_t round_position(double value) {
  return value >= 0.0 ? (int32_t)(value + 0.5) : -(int32_t)(-value + 0.5);
}

/* Rounds away from zero so a small non-zero scroll is never lost. */
static int32_t round_wheel(double delta) {
  if (delta > 0.0) {
    int32_t v = round_position(delta);
    return v < 1 ? 1 : v;
  }
  if (delta < 0.0) {
    int32_t v = round_position(delta);
    return v > -1 ? -1 : v;
  }
  return 0;
}

static crtui_result send_pointer(
    crtui_context* context, crtui_window window, crtui_input_type type, int32_t x, int32_t y, int32_t wheel) {
  crtui_input input;
  memset(&input, 0, sizeof(input));
  input.type = type;
  input.window = window;
  input.x = x;
  input.y = y;
  input.wheel_delta = wheel;
  return crtui_context_send_input(context, &input);
}

crtui_result crtui_window_handle_crtgfx_event(
    crtui_context* context, crtui_window window, crtui_crtgfx_state* state, const crtgfx_event* event) {
  if (context == NULL || state == NULL || event == NULL) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  switch (event->type) {
    case CRTGFX_EVENT_KEY_DOWN:
    case CRTGFX_EVENT_KEY_UP: {
      crtui_key key = map_key(event->data.key.keycode);
      if (key == CRTUI_KEY_NONE) {
        return CRTUI_OK;
      }
      crtui_input input;
      memset(&input, 0, sizeof(input));
      input.type = event->type == CRTGFX_EVENT_KEY_DOWN ? CRTUI_INPUT_KEY_DOWN : CRTUI_INPUT_KEY_UP;
      input.window = window;
      input.key = key;
      input.modifiers = (event->data.key.modifiers & CRTGFX_MOD_SHIFT) != 0 ? CRTUI_MOD_SHIFT : 0u;
      return crtui_context_send_input(context, &input);
    }
    case CRTGFX_EVENT_TEXT: {
      crtui_input input;
      memset(&input, 0, sizeof(input));
      input.type = CRTUI_INPUT_TEXT;
      input.window = window;
      /* crtgfx guarantees NUL-termination within its 8 bytes; copy defensively. */
      size_t length = 0;
      while (length < sizeof(input.text) - 1 && event->data.text.utf8[length] != '\0') ++length;
      memcpy(input.text, event->data.text.utf8, length);
      return crtui_context_send_input(context, &input);
    }
    case CRTGFX_EVENT_POINTER_MOTION:
      state->pointer_x = round_position(event->data.pointer_motion.x);
      state->pointer_y = round_position(event->data.pointer_motion.y);
      return send_pointer(context, window, CRTUI_INPUT_POINTER_MOVE, state->pointer_x, state->pointer_y, 0);
    case CRTGFX_EVENT_POINTER_BUTTON_DOWN:
    case CRTGFX_EVENT_POINTER_BUTTON_UP:
      state->pointer_x = round_position(event->data.pointer_button.x);
      state->pointer_y = round_position(event->data.pointer_button.y);
      if (event->data.pointer_button.button != CRTGFX_POINTER_BUTTON_LEFT) {
        return CRTUI_OK;
      }
      return send_pointer(
          context, window,
          event->type == CRTGFX_EVENT_POINTER_BUTTON_DOWN ? CRTUI_INPUT_POINTER_DOWN : CRTUI_INPUT_POINTER_UP,
          state->pointer_x, state->pointer_y, 0);
    case CRTGFX_EVENT_POINTER_SCROLL: {
      int32_t delta = round_wheel(event->data.pointer_scroll.dy);
      if (delta == 0) {
        return CRTUI_OK;
      }
      return send_pointer(context, window, CRTUI_INPUT_WHEEL, state->pointer_x, state->pointer_y, delta);
    }
    case CRTGFX_EVENT_RESIZE:
    case CRTGFX_EVENT_DPI_SCALE_CHANGED: {
      int32_t width = 0;
      int32_t height = 0;
      float scale = 1.0f;
      crtui_result result = crtui_window_get_size(context, window, &width, &height, &scale);
      if (result != CRTUI_OK) {
        return result;
      }
      if (event->type == CRTGFX_EVENT_RESIZE) {
        width = (int32_t)event->data.resize.width;
        height = (int32_t)event->data.resize.height;
      } else {
        scale = (float)event->data.dpi_scale.scale;
      }
      return crtui_window_set_size(context, window, width, height, scale);
    }
    case CRTGFX_EVENT_FOCUS_OUT:
      return send_pointer(context, window, CRTUI_INPUT_POINTER_CANCEL, 0, 0, 0);
    default:
      return CRTUI_OK;
  }
}
