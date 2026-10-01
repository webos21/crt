#pragma once

/* crtui -- the CRT-owned application UI API (stage `05-ui`).
 *
 * This header is the frozen Tranche 0 contract of docs/crtui_acceptance.md.
 * No LVGL type, header or symbol appears here or in any installed header: LVGL
 * (arriving in Tranche 1) is a private implementation dependency behind this
 * API, exactly as libcurl/mbedTLS sit behind crtmedia's transport. Everything
 * below is host-neutral and resource-free: creating a context and a widget tree
 * needs no window, GPU, or display.
 *
 * ---- Threading ----------------------------------------------------------
 * A crtui_context belongs to the thread that created it (the "UI thread").
 * Every function except crtui_context_post() must be called on that thread;
 * any other thread gets CRTUI_ERROR_WRONG_THREAD and nothing is changed.
 * crtui_context_post() is the single documented cross-thread entry point: it
 * queues a function that crtui_context_pump() later runs on the UI thread, in
 * FIFO order.
 *
 * ---- Handles and lifetime -----------------------------------------------
 * A crtui_widget is an opaque 64-bit id, never a pointer. Ids carry a
 * generation, so an id that outlived its widget is *detected*, never
 * dereferenced: every call taking a dead id returns CRTUI_ERROR_INVALID_HANDLE.
 * A parent owns its children: destroying a widget destroys its whole subtree
 * immediately, and every id in that subtree is invalid from that moment on --
 * including from inside an event callback. There is no reparenting in v1.
 * crtui_context_destroy() destroys every remaining widget; posted functions
 * that never ran are discarded (not run).
 *
 * ---- Events -------------------------------------------------------------
 * Input enters through crtui_context_send_input() (UI thread). Routing:
 *   - pointer and wheel input goes to the topmost visible widget under the
 *     point (later siblings are on top; a child is clipped to every ancestor);
 *   - key input goes to the focused widget of that window, else the window;
 *   - a modal window (crtui_window_set_modal) drops all input for every other
 *     window until it is no longer modal or destroyed.
 * Delivery: the event goes to the target's callback first, then bubbles to
 * each ancestor up to and including the window; a callback returning
 * CRTUI_EVENT_HANDLED stops the bubble. FOCUS_IN, FOCUS_OUT and RESIZED go to
 * their target only. A disabled widget, or any widget under a disabled
 * ancestor, receives nothing (the input is dropped, not passed through).
 * Default actions run only if no callback handled the event. KEY_DOWN: Tab /
 * Shift+Tab move focus through the traversal order; the arrow keys move focus
 * spatially to the nearest eligible widget in that direction (no wrap; with
 * nothing focused they focus the first eligible widget), except that a focused
 * slider consumes Left/Right to step by one, clamped (VALUE_CHANGED) -- Up/Down
 * still navigate away from it; Enter / Space activate a focused button
 * (ACTIVATE). WHEEL over a slider steps it by one (positive delta = up, negative
 * = down; the sign is exactly what the caller passes, crtgfx's is host-native).
 * A pointer press on a focusable widget focuses it first; releasing the pointer
 * over the same button that took the press activates it. A press on a slider
 * sets its value from the pointer's x position and captures the pointer:
 * moves update the value even outside the slider's bounds (and are delivered to
 * the slider), until the release, which also ends the capture. A widget that
 * becomes ineligible or is destroyed during a capture ends it.
 * Re-entrancy: callbacks may call any crtui function on the UI thread,
 * including destroying their own widget. Input sent from inside a callback is
 * queued and delivered after the current event finishes (FIFO), never nested.
 * No callback is invoked for a widget after it was destroyed. The callback's
 * `user` pointer is borrowed: crtui never frees it.
 *
 * ---- Focus --------------------------------------------------------------
 * One focused widget per context. Traversal order is the tree's pre-order
 * over widgets that are focusable, enabled and visible (with every ancestor
 * enabled and visible) inside the active window; Tab wraps. Buttons and
 * sliders are focusable by default. When the focused widget is destroyed,
 * hidden or disabled, focus moves to the next eligible widget after it in
 * traversal order (wrapping), else clears; FOCUS_OUT goes only to a widget that
 * still exists.
 *
 * ---- Geometry and layout ------------------------------------------------
 * Bounds are integers in logical pixels, relative to the parent. A window's
 * content size and DPI scale (physical = logical * scale) are set by
 * crtui_window_set_size(), which emits RESIZED.
 * Containers come in four layouts. A plain Container (and a Window) is *free*:
 * children sit where crtui_widget_set_bounds() puts them. Row and Column are
 * flex boxes (main axis horizontal/vertical), and Stack overlays its children.
 * In a flex box or a Stack the parent owns each child's position; the child's
 * set_bounds/set_size width and height are its *size request*, where
 * CRTUI_SIZE_FILL means "fill the parent's padded content box on that axis". A
 * child's `grow` weight shares the space left on the main axis; its alignment
 * places or stretches it across the axis; margin surrounds it; the container's
 * padding insets the content box, `gap` separates children and `main_align`
 * distributes leftover main-axis space. Hidden children take no space. A free
 * container honors FILL too (the child then snaps to the content box on that
 * axis). Layout runs lazily before anything reads geometry (bounds queries,
 * hit-testing, input routing, rendering), so it is always current and a window
 * resize re-lays out the whole tree by itself. A ScrollView (and a List, a
 * ScrollView of buttons) is a vertical flex box whose content may exceed its
 * height: the wheel scrolls it, focusing a child scrolls it into view, and its
 * offset is clamped to the content.
 *
 * ---- Style --------------------------------------------------------------
 * A small CRT-neutral property set -- background, foreground, border color and
 * width, corner radius, opacity, font size, text alignment -- set through
 * crtui_widget_set_style(); unset properties keep the CRT default look. No
 * renderer style type is exposed. Per widget, background is the main fill (a
 * slider/progress track, a switch's off track) and foreground is the accent (text
 * color, a slider/progress fill, a switch's on track, a checkbox's mark).
 *
 * ---- External surfaces --------------------------------------------------
 * crtui_surface_view_create() reserves the API shape for producer-owned
 * surfaces (video now, a WebView in 06-web). It returns
 * CRTUI_ERROR_UNSUPPORTED until Tranche 5; the ownership rule is frozen now:
 * the view owns only position, size, clip, opacity, visibility, z-order,
 * damage and hit-testing, and never the producer's frame.
 *
 * ---- Errors -------------------------------------------------------------
 * Every function that can fail returns crtui_result. The values 0..-7 are
 * numerically identical to crtmedia_result so a consumer can treat them
 * uniformly; crtui adds three of its own. There is no silent failure. */

#include <stddef.h>
#include <stdint.h>

#include "crtui/api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum crtui_result {
  CRTUI_OK = 0,
  CRTUI_ERROR_INVALID_ARGUMENT = -1,
  CRTUI_ERROR_UNSUPPORTED = -2,
  CRTUI_WOULD_BLOCK = -3,
  CRTUI_ERROR_IO = -4,
  CRTUI_ERROR_TIMEOUT = -5,
  CRTUI_ERROR_CANCELLED = -6,
  CRTUI_ERROR_PROTOCOL = -7,
  /* crtui-specific. */
  CRTUI_ERROR_WRONG_THREAD = -8,   /* not the context's UI thread */
  CRTUI_ERROR_INVALID_HANDLE = -9, /* stale, destroyed or never-valid id */
  CRTUI_ERROR_STATE = -10          /* valid call, wrong state (e.g. no focus) */
} crtui_result;

typedef struct crtui_context crtui_context;
typedef uint64_t crtui_widget;
typedef crtui_widget crtui_window;
#define CRTUI_INVALID_WIDGET ((crtui_widget)0)

typedef enum crtui_widget_kind {
  CRTUI_WIDGET_WINDOW = 1,
  CRTUI_WIDGET_CONTAINER = 2,
  CRTUI_WIDGET_TEXT = 3,
  CRTUI_WIDGET_BUTTON = 4,
  CRTUI_WIDGET_SLIDER = 5,
  CRTUI_WIDGET_SURFACE_VIEW = 6,
  CRTUI_WIDGET_PROGRESS = 7,
  CRTUI_WIDGET_ROW = 8,
  CRTUI_WIDGET_COLUMN = 9,
  CRTUI_WIDGET_STACK = 10,
  CRTUI_WIDGET_IMAGE = 11,
  CRTUI_WIDGET_SWITCH = 12,
  CRTUI_WIDGET_CHECKBOX = 13,
  CRTUI_WIDGET_SCROLL_VIEW = 14,
  CRTUI_WIDGET_LIST = 15,
  CRTUI_WIDGET_TEXT_INPUT = 16
} crtui_widget_kind;

typedef enum crtui_key {
  CRTUI_KEY_NONE = 0,
  CRTUI_KEY_TAB,
  CRTUI_KEY_ENTER,
  CRTUI_KEY_SPACE,
  CRTUI_KEY_LEFT,
  CRTUI_KEY_RIGHT,
  CRTUI_KEY_UP,
  CRTUI_KEY_DOWN,
  CRTUI_KEY_ESCAPE,
  CRTUI_KEY_BACKSPACE,
  CRTUI_KEY_DELETE,
  CRTUI_KEY_HOME,
  CRTUI_KEY_END
} crtui_key;

#define CRTUI_MOD_SHIFT 1u

typedef enum crtui_input_type {
  CRTUI_INPUT_POINTER_DOWN = 1,
  CRTUI_INPUT_POINTER_UP,
  CRTUI_INPUT_POINTER_MOVE,
  CRTUI_INPUT_WHEEL,
  CRTUI_INPUT_KEY_DOWN,
  CRTUI_INPUT_KEY_UP,
  /* The pointer interaction was interrupted (window lost keyboard focus, touch
   * cancelled): an in-progress press or slider drag ends without activating
   * anything and without changing the slider. Carries no position. */
  CRTUI_INPUT_POINTER_CANCEL,
  /* Committed text (one UTF-8 character/grapheme in crtui_input::text). Goes to
   * the focused widget of the window; a TextInput inserts it at its caret. */
  CRTUI_INPUT_TEXT
} crtui_input_type;

typedef struct crtui_input {
  crtui_input_type type;
  crtui_window window; /* the window the input is addressed to */
  int32_t x, y;        /* pointer/wheel position, window logical pixels */
  int32_t wheel_delta;
  crtui_key key;
  uint32_t modifiers;
  char text[8]; /* CRTUI_INPUT_TEXT: NUL-terminated UTF-8 */
} crtui_input;

typedef enum crtui_event_type {
  CRTUI_EVENT_POINTER_DOWN = 1,
  CRTUI_EVENT_POINTER_UP,
  CRTUI_EVENT_POINTER_MOVE,
  CRTUI_EVENT_WHEEL,
  CRTUI_EVENT_KEY_DOWN,
  CRTUI_EVENT_KEY_UP,
  CRTUI_EVENT_FOCUS_IN,
  CRTUI_EVENT_FOCUS_OUT,
  CRTUI_EVENT_ACTIVATE,
  CRTUI_EVENT_VALUE_CHANGED,
  CRTUI_EVENT_RESIZED,
  /* A user edit changed a TextInput's text (value = new length in bytes). */
  CRTUI_EVENT_TEXT_CHANGED
} crtui_event_type;

typedef struct crtui_event {
  crtui_event_type type;
  crtui_widget target; /* where the event started (bubbling keeps this) */
  int32_t x, y;        /* pointer position; RESIZED: new width, height */
  crtui_key key;
  int32_t value;       /* VALUE_CHANGED: new value; WHEEL: delta; RESIZED: scale in percent */
} crtui_event;

#define CRTUI_EVENT_IGNORED 0
#define CRTUI_EVENT_HANDLED 1

/* `current` is the widget whose callback is running (target or an ancestor). */
typedef int (*crtui_event_fn)(crtui_widget current, const crtui_event* event, void* user);
typedef void (*crtui_post_fn)(void* user);

/* ---- Context ------------------------------------------------------------ */

/* Binds the calling thread as the UI thread. */
CRTUI_API crtui_result crtui_context_create(crtui_context** out_context);
CRTUI_API crtui_result crtui_context_destroy(crtui_context* context);

/* The only thread-safe call. Queues `fn(user)` to run on the UI thread. */
CRTUI_API crtui_result crtui_context_post(crtui_context* context, crtui_post_fn fn, void* user);

/* Runs queued posted functions (FIFO), then queued input; sets *out_ran (may be
 * NULL) to the number of posted functions run. */
CRTUI_API crtui_result crtui_context_pump(crtui_context* context, size_t* out_ran);

CRTUI_API crtui_result crtui_context_send_input(crtui_context* context, const crtui_input* input);

/* ---- Widgets ------------------------------------------------------------ */

CRTUI_API crtui_result crtui_window_create(crtui_context* context, crtui_window* out_window);
CRTUI_API crtui_result crtui_container_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget);
CRTUI_API crtui_result crtui_text_create(crtui_context* context, crtui_widget parent, const char* utf8, crtui_widget* out_widget);
CRTUI_API crtui_result crtui_button_create(crtui_context* context, crtui_widget parent, const char* utf8, crtui_widget* out_widget);
CRTUI_API crtui_result crtui_slider_create(
    crtui_context* context, crtui_widget parent, int32_t min_value, int32_t max_value, crtui_widget* out_widget);
/* A progress bar over the fixed range 0..100 (initially 0). Not focusable. */
CRTUI_API crtui_result crtui_progress_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget);
CRTUI_API crtui_result crtui_progress_set_value(crtui_context* context, crtui_widget progress, int32_t value);
CRTUI_API crtui_result crtui_progress_get_value(crtui_context* context, crtui_widget progress, int32_t* out_value);
CRTUI_API crtui_result crtui_surface_view_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget);

/* Destroys `widget` and its whole subtree. A window's parent is none. */
CRTUI_API crtui_result crtui_widget_destroy(crtui_context* context, crtui_widget widget);

CRTUI_API crtui_result crtui_widget_get_kind(crtui_context* context, crtui_widget widget, crtui_widget_kind* out_kind);
CRTUI_API crtui_result crtui_widget_get_parent(crtui_context* context, crtui_widget widget, crtui_widget* out_parent);

CRTUI_API crtui_result crtui_widget_set_bounds(
    crtui_context* context, crtui_widget widget, int32_t x, int32_t y, int32_t width, int32_t height);
CRTUI_API crtui_result crtui_widget_get_bounds(
    crtui_context* context, crtui_widget widget, int32_t* out_x, int32_t* out_y, int32_t* out_width,
    int32_t* out_height);
CRTUI_API crtui_result crtui_widget_set_visible(crtui_context* context, crtui_widget widget, int visible);
CRTUI_API crtui_result crtui_widget_set_enabled(crtui_context* context, crtui_widget widget, int enabled);
CRTUI_API crtui_result crtui_widget_set_focusable(crtui_context* context, crtui_widget widget, int focusable);

/* Text and button widgets only. get_text copies at most capacity-1 bytes plus
 * a NUL into buffer and sets *out_length (may be NULL) to the full length. */
CRTUI_API crtui_result crtui_widget_set_text(crtui_context* context, crtui_widget widget, const char* utf8);
CRTUI_API crtui_result crtui_widget_get_text(
    crtui_context* context, crtui_widget widget, char* buffer, size_t capacity, size_t* out_length);

/* Sliders only. Programmatic changes clamp to [min, max] and do NOT emit
 * VALUE_CHANGED (only user input does). */
CRTUI_API crtui_result crtui_slider_set_value(crtui_context* context, crtui_widget slider, int32_t value);
CRTUI_API crtui_result crtui_slider_get_value(crtui_context* context, crtui_widget slider, int32_t* out_value);

/* One callback per widget; NULL clears it. */
CRTUI_API crtui_result crtui_widget_set_callback(crtui_context* context, crtui_widget widget, crtui_event_fn fn, void* user);

/* ---- Windows ------------------------------------------------------------ */

CRTUI_API crtui_result crtui_window_set_size(
    crtui_context* context, crtui_window window, int32_t width, int32_t height, float dpi_scale);
CRTUI_API crtui_result crtui_window_get_size(
    crtui_context* context, crtui_window window, int32_t* out_width, int32_t* out_height, float* out_dpi_scale);
CRTUI_API crtui_result crtui_window_set_modal(crtui_context* context, crtui_window window, int modal);

/* ---- Layout, style and the v1 widget set (Tranche 4) ---------------------- */

#define CRTUI_SIZE_FILL ((int32_t)-1)

typedef enum crtui_align {
  CRTUI_ALIGN_START = 0,
  CRTUI_ALIGN_CENTER = 1,
  CRTUI_ALIGN_END = 2,
  CRTUI_ALIGN_STRETCH = 3
} crtui_align;

typedef enum crtui_main_align {
  CRTUI_MAIN_START = 0,
  CRTUI_MAIN_CENTER = 1,
  CRTUI_MAIN_END = 2,
  CRTUI_MAIN_SPACE_BETWEEN = 3
} crtui_main_align;

typedef enum crtui_font {
  CRTUI_FONT_DEFAULT = 0, /* 14 px */
  CRTUI_FONT_SMALL = 1,   /* 12 px */
  CRTUI_FONT_LARGE = 2    /* 20 px */
} crtui_font;

typedef enum crtui_text_align {
  CRTUI_TEXT_ALIGN_START = 0,
  CRTUI_TEXT_ALIGN_CENTER = 1,
  CRTUI_TEXT_ALIGN_END = 2
} crtui_text_align;

enum {
  CRTUI_STYLE_BACKGROUND = 1u << 0,
  CRTUI_STYLE_FOREGROUND = 1u << 1,
  CRTUI_STYLE_BORDER_COLOR = 1u << 2,
  CRTUI_STYLE_BORDER_WIDTH = 1u << 3,
  CRTUI_STYLE_RADIUS = 1u << 4,
  CRTUI_STYLE_OPACITY = 1u << 5,
  CRTUI_STYLE_FONT = 1u << 6,
  CRTUI_STYLE_TEXT_ALIGN = 1u << 7
};

/* Colors are 0xRRGGBB; opacity is 0 (transparent) to 255 (opaque). `mask` says
 * which fields are set: set_style() applies only those and leaves the rest of the
 * widget's style alone; get_style() reports the widget's current overrides. */
typedef struct crtui_style {
  uint32_t mask;
  uint32_t background_rgb;
  uint32_t foreground_rgb;
  uint32_t border_rgb;
  int32_t border_width;
  int32_t radius;
  uint8_t opacity;
  crtui_font font;
  crtui_text_align text_align;
} crtui_style;

CRTUI_API crtui_result crtui_row_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget);
CRTUI_API crtui_result crtui_column_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget);
CRTUI_API crtui_result crtui_stack_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget);

/* Size request (CRTUI_SIZE_FILL allowed) without touching the position. */
CRTUI_API crtui_result crtui_widget_set_size(crtui_context* context, crtui_widget widget, int32_t width, int32_t height);
CRTUI_API crtui_result crtui_widget_set_margin(
    crtui_context* context, crtui_widget widget, int32_t left, int32_t top, int32_t right, int32_t bottom);
/* Containers only (Window, Container, Row, Column, Stack, ScrollView, List). */
CRTUI_API crtui_result crtui_container_set_padding(
    crtui_context* context, crtui_widget container, int32_t left, int32_t top, int32_t right, int32_t bottom);
CRTUI_API crtui_result crtui_container_set_gap(crtui_context* context, crtui_widget container, int32_t gap);
CRTUI_API crtui_result crtui_container_set_main_align(
    crtui_context* context, crtui_widget container, crtui_main_align main_align);
/* How the parent places this widget: in a Row `vertical` is the cross axis, in a
 * Column `horizontal` is; a Stack uses both. A free container ignores it. */
CRTUI_API crtui_result crtui_widget_set_alignment(
    crtui_context* context, crtui_widget widget, crtui_align horizontal, crtui_align vertical);
/* Flex weight along the parent's main axis (0 = none). */
CRTUI_API crtui_result crtui_widget_set_grow(crtui_context* context, crtui_widget widget, int32_t grow);

CRTUI_API crtui_result crtui_widget_set_style(crtui_context* context, crtui_widget widget, const crtui_style* style);
CRTUI_API crtui_result crtui_widget_get_style(crtui_context* context, crtui_widget widget, crtui_style* out_style);
CRTUI_API crtui_result crtui_widget_clear_style(crtui_context* context, crtui_widget widget);

/* Image: the pixels are copied (BGRA8888, straight alpha) and stretched to the
 * widget's size. */
CRTUI_API crtui_result crtui_image_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget);
CRTUI_API crtui_result crtui_image_set_pixels(
    crtui_context* context, crtui_widget image, const void* bgra, int32_t width, int32_t height, size_t stride_bytes);

/* Switch and Checkbox share the checked state. Toggled by a click, Enter or
 * Space; a user toggle emits VALUE_CHANGED (0/1), set_checked() does not. */
CRTUI_API crtui_result crtui_switch_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget);
CRTUI_API crtui_result crtui_checkbox_create(
    crtui_context* context, crtui_widget parent, const char* utf8, crtui_widget* out_widget);
CRTUI_API crtui_result crtui_toggle_set_checked(crtui_context* context, crtui_widget toggle, int checked);
CRTUI_API crtui_result crtui_toggle_get_checked(crtui_context* context, crtui_widget toggle, int* out_checked);

/* ScrollView / List: vertical scrolling. The offset is in pixels, clamped to
 * [0, content_height - height]. A List item is a full-width Button row. */
CRTUI_API crtui_result crtui_scroll_view_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget);
CRTUI_API crtui_result crtui_list_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget);
CRTUI_API crtui_result crtui_list_add_item(
    crtui_context* context, crtui_widget list, const char* utf8, crtui_widget* out_item);
CRTUI_API crtui_result crtui_scroll_view_set_offset(crtui_context* context, crtui_widget scroll_view, int32_t offset);
CRTUI_API crtui_result crtui_scroll_view_get_offset(
    crtui_context* context, crtui_widget scroll_view, int32_t* out_offset, int32_t* out_content_height);

/* Single-line text entry. set_text/get_text read and write its content (the
 * caret moves to the end on set_text). The caret is a byte offset on a UTF-8
 * character boundary. Typing (CRTUI_INPUT_TEXT), Backspace, Delete, Left/Right,
 * Home/End edit it and emit TEXT_CHANGED; Enter emits ACTIVATE ("submit"). */
CRTUI_API crtui_result crtui_text_input_create(
    crtui_context* context, crtui_widget parent, const char* initial_utf8, crtui_widget* out_widget);
CRTUI_API crtui_result crtui_text_input_set_placeholder(crtui_context* context, crtui_widget text_input, const char* utf8);
CRTUI_API crtui_result crtui_text_input_set_caret(crtui_context* context, crtui_widget text_input, size_t byte_offset);
CRTUI_API crtui_result crtui_text_input_get_caret(crtui_context* context, crtui_widget text_input, size_t* out_byte_offset);

/* ---- Rendering (Tranche 1) ---------------------------------------------- */

/* Renders the window's current tree into `pixels` as BGRA8888 (bytes B,G,R,A --
 * the crtgfx_pixel_format BGRA8888_PREMULTIPLIED layout; the window is opaque).
 * `width`/`height` must equal the window's size (crtui_window_set_size(), DPI
 * scale 1 for now) and stride_bytes must be at least width * 4. Every pixel of
 * the buffer is overwritten. UI thread only. Returns CRTUI_ERROR_UNSUPPORTED if
 * this build has no renderer (crtui is built without LVGL); the headless model
 * works either way. The image is drawn by a private LVGL software renderer:
 * crtui never exposes LVGL, and crtgfx presentation is the caller's step. */
CRTUI_API crtui_result crtui_window_render(
    crtui_context* context, crtui_window window, void* pixels, size_t stride_bytes, int32_t width, int32_t height);

/* ---- Focus and hit-testing ---------------------------------------------- */

CRTUI_API crtui_result crtui_widget_focus(crtui_context* context, crtui_widget widget);
/* CRTUI_ERROR_STATE (and *out_widget = CRTUI_INVALID_WIDGET) if nothing is focused. */
CRTUI_API crtui_result crtui_context_get_focus(crtui_context* context, crtui_widget* out_widget);
CRTUI_API crtui_result crtui_context_hit_test(
    crtui_context* context, crtui_window window, int32_t x, int32_t y, crtui_widget* out_widget);

#ifdef __cplusplus
}
#endif
