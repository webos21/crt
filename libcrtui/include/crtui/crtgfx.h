#pragma once

/* crtui <-> crtgfx input adapter (Tranche 2). Feeds crtgfx window events into a
 * crtui window, so an application's loop is:
 *
 *   crtgfx_window_pump_events(timeout);
 *   while (crtgfx_window_poll_event(gfx, &event) == CRTGFX_OK &&
 *          event.type != CRTGFX_EVENT_NONE) {
 *     crtui_window_handle_crtgfx_event(ui, window, &state, &event);
 *   }
 *   crtui_window_render(...); crtgfx_window_begin_frame()/end_frame() ...
 *
 * This header includes crtgfx/window.h (the crtgfx include directory must be on
 * the compiler's path); libcrtui itself only reads crtgfx_event, it never calls
 * into crtgfx, so the adapter adds no link dependency. */

#include "crtgfx/window.h"
#include "crtui/ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Per-window adapter state. Zero-initialize it. The adapter needs the last
 * pointer position because crtgfx scroll events carry no position. */
typedef struct crtui_crtgfx_state {
  int32_t pointer_x;
  int32_t pointer_y;
} crtui_crtgfx_state;

/* Translates one crtgfx event for `window` and applies it. UI thread only.
 *
 *   KEY_DOWN / KEY_UP          -> crtui key input (Linux evdev keycodes, as
 *                                 crtgfx defines them: Tab, Enter/KP Enter,
 *                                 Space, arrows, Escape; Shift is carried);
 *                                 any other key is ignored (text input is a
 *                                 later tranche, so TEXT is ignored too)
 *   POINTER_MOTION             -> POINTER_MOVE at the (rounded) position
 *   POINTER_BUTTON_DOWN / UP   -> POINTER_DOWN / UP for the LEFT button only;
 *                                 other buttons are ignored
 *   POINTER_SCROLL             -> WHEEL at the last pointer position; wheel_delta
 *                                 is dy rounded away from zero (a non-zero dy is
 *                                 never lost to rounding), sign exactly as crtgfx
 *                                 reports it (host-native, not normalized)
 *   RESIZE                     -> crtui_window_set_size() with the new size and
 *                                 the current DPI scale (emits RESIZED)
 *   DPI_SCALE_CHANGED          -> crtui_window_set_size() with the current size
 *                                 and the new scale (emits RESIZED)
 *   FOCUS_OUT                  -> CRTUI_INPUT_POINTER_CANCEL: an in-progress
 *                                 press or slider drag ends without activating
 *   FOCUS_IN, TEXT, EXPOSE, CLOSE_REQUESTED, FRAME_COMPLETE, NONE
 *                              -> ignored (returns CRTUI_OK); closing and pacing
 *                                 stay the application's job
 *
 * Returns CRTUI_OK for handled and ignored events, or the error the crtui call
 * produced (for example CRTUI_ERROR_INVALID_HANDLE for a destroyed window). */
CRTUI_API crtui_result crtui_window_handle_crtgfx_event(
    crtui_context* context, crtui_window window, crtui_crtgfx_state* state, const crtgfx_event* event);

#ifdef __cplusplus
}
#endif
