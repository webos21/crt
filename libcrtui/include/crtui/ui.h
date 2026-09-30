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
 * Default actions run only if no callback handled a KEY_DOWN: Tab / Shift+Tab
 * move focus, Enter / Space activate a focused button (ACTIVATE), and the
 * arrow keys step a focused slider by one, clamped (VALUE_CHANGED). A pointer
 * press on a focusable widget focuses it first; releasing the pointer over the
 * same button that took the press activates it.
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
 * ---- Geometry -----------------------------------------------------------
 * Bounds are integers in logical pixels, relative to the parent. A window's
 * content size and DPI scale (physical = logical * scale) are set by
 * crtui_window_set_size(), which emits RESIZED. Automatic layout is a later
 * tranche; until then bounds are authoritative and only ever set by the
 * application.
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
  CRTUI_WIDGET_PROGRESS = 7
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
  CRTUI_KEY_ESCAPE
} crtui_key;

#define CRTUI_MOD_SHIFT 1u

typedef enum crtui_input_type {
  CRTUI_INPUT_POINTER_DOWN = 1,
  CRTUI_INPUT_POINTER_UP,
  CRTUI_INPUT_POINTER_MOVE,
  CRTUI_INPUT_WHEEL,
  CRTUI_INPUT_KEY_DOWN,
  CRTUI_INPUT_KEY_UP
} crtui_input_type;

typedef struct crtui_input {
  crtui_input_type type;
  crtui_window window; /* the window the input is addressed to */
  int32_t x, y;        /* pointer/wheel position, window logical pixels */
  int32_t wheel_delta;
  crtui_key key;
  uint32_t modifiers;
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
  CRTUI_EVENT_RESIZED
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
crtui_result crtui_context_create(crtui_context** out_context);
crtui_result crtui_context_destroy(crtui_context* context);

/* The only thread-safe call. Queues `fn(user)` to run on the UI thread. */
crtui_result crtui_context_post(crtui_context* context, crtui_post_fn fn, void* user);

/* Runs queued posted functions (FIFO), then queued input; sets *out_ran (may be
 * NULL) to the number of posted functions run. */
crtui_result crtui_context_pump(crtui_context* context, size_t* out_ran);

crtui_result crtui_context_send_input(crtui_context* context, const crtui_input* input);

/* ---- Widgets ------------------------------------------------------------ */

crtui_result crtui_window_create(crtui_context* context, crtui_window* out_window);
crtui_result crtui_container_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget);
crtui_result crtui_text_create(crtui_context* context, crtui_widget parent, const char* utf8, crtui_widget* out_widget);
crtui_result crtui_button_create(crtui_context* context, crtui_widget parent, const char* utf8, crtui_widget* out_widget);
crtui_result crtui_slider_create(
    crtui_context* context, crtui_widget parent, int32_t min_value, int32_t max_value, crtui_widget* out_widget);
/* A progress bar over the fixed range 0..100 (initially 0). Not focusable. */
crtui_result crtui_progress_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget);
crtui_result crtui_progress_set_value(crtui_context* context, crtui_widget progress, int32_t value);
crtui_result crtui_progress_get_value(crtui_context* context, crtui_widget progress, int32_t* out_value);
crtui_result crtui_surface_view_create(crtui_context* context, crtui_widget parent, crtui_widget* out_widget);

/* Destroys `widget` and its whole subtree. A window's parent is none. */
crtui_result crtui_widget_destroy(crtui_context* context, crtui_widget widget);

crtui_result crtui_widget_get_kind(crtui_context* context, crtui_widget widget, crtui_widget_kind* out_kind);
crtui_result crtui_widget_get_parent(crtui_context* context, crtui_widget widget, crtui_widget* out_parent);

crtui_result crtui_widget_set_bounds(
    crtui_context* context, crtui_widget widget, int32_t x, int32_t y, int32_t width, int32_t height);
crtui_result crtui_widget_get_bounds(
    crtui_context* context, crtui_widget widget, int32_t* out_x, int32_t* out_y, int32_t* out_width,
    int32_t* out_height);
crtui_result crtui_widget_set_visible(crtui_context* context, crtui_widget widget, int visible);
crtui_result crtui_widget_set_enabled(crtui_context* context, crtui_widget widget, int enabled);
crtui_result crtui_widget_set_focusable(crtui_context* context, crtui_widget widget, int focusable);

/* Text and button widgets only. get_text copies at most capacity-1 bytes plus
 * a NUL into buffer and sets *out_length (may be NULL) to the full length. */
crtui_result crtui_widget_set_text(crtui_context* context, crtui_widget widget, const char* utf8);
crtui_result crtui_widget_get_text(
    crtui_context* context, crtui_widget widget, char* buffer, size_t capacity, size_t* out_length);

/* Sliders only. Programmatic changes clamp to [min, max] and do NOT emit
 * VALUE_CHANGED (only user input does). */
crtui_result crtui_slider_set_value(crtui_context* context, crtui_widget slider, int32_t value);
crtui_result crtui_slider_get_value(crtui_context* context, crtui_widget slider, int32_t* out_value);

/* One callback per widget; NULL clears it. */
crtui_result crtui_widget_set_callback(crtui_context* context, crtui_widget widget, crtui_event_fn fn, void* user);

/* ---- Windows ------------------------------------------------------------ */

crtui_result crtui_window_set_size(
    crtui_context* context, crtui_window window, int32_t width, int32_t height, float dpi_scale);
crtui_result crtui_window_get_size(
    crtui_context* context, crtui_window window, int32_t* out_width, int32_t* out_height, float* out_dpi_scale);
crtui_result crtui_window_set_modal(crtui_context* context, crtui_window window, int modal);

/* ---- Rendering (Tranche 1) ---------------------------------------------- */

/* Renders the window's current tree into `pixels` as BGRA8888 (bytes B,G,R,A --
 * the crtgfx_pixel_format BGRA8888_PREMULTIPLIED layout; the window is opaque).
 * `width`/`height` must equal the window's size (crtui_window_set_size(), DPI
 * scale 1 for now) and stride_bytes must be at least width * 4. Every pixel of
 * the buffer is overwritten. UI thread only. Returns CRTUI_ERROR_UNSUPPORTED if
 * this build has no renderer (crtui is built without LVGL); the headless model
 * works either way. The image is drawn by a private LVGL software renderer:
 * crtui never exposes LVGL, and crtgfx presentation is the caller's step. */
crtui_result crtui_window_render(
    crtui_context* context, crtui_window window, void* pixels, size_t stride_bytes, int32_t width, int32_t height);

/* ---- Focus and hit-testing ---------------------------------------------- */

crtui_result crtui_widget_focus(crtui_context* context, crtui_widget widget);
/* CRTUI_ERROR_STATE (and *out_widget = CRTUI_INVALID_WIDGET) if nothing is focused. */
crtui_result crtui_context_get_focus(crtui_context* context, crtui_widget* out_widget);
crtui_result crtui_context_hit_test(
    crtui_context* context, crtui_window window, int32_t x, int32_t y, crtui_widget* out_widget);

#ifdef __cplusplus
}
#endif
