#pragma once

/* crtweb: the CRT-owned web runtime and view API (Web Tranche 4).
 *
 * Plain C99. No WebKit, WPE, GLib or JavaScriptCore type appears here or is needed to use it:
 * the implementation (libcrtweb) is built over the PlatformCRT port and hides it. Contract,
 * ownership and threading rules: docs/design/libcrtweb_api_policy.md; evidence:
 * docs/acceptance/crtweb_acceptance.md.
 *
 * Model. One runtime owns the shared machinery (Network process, website data, event loop
 * integration). A runtime owns any number of views; each view is one page with its own Web process
 * state, size, focus and navigation history. The application pumps the runtime from the thread that
 * created it; every callback is delivered from inside crtweb_runtime_pump(), on that thread, never
 * from another one and never re-entrantly from an API call.
 *
 * Pixels. A view paints on the CPU. crtweb_view_copy_frame() copies the newest picture out as
 * premultiplied BGRA (byte order B, G, R, A; the AR24 layout of the web surface protocol), origin
 * top-left, in physical pixels (logical size * scale_percent / 100).
 */

#include <stddef.h>
#include <stdint.h>

#if defined(CRTWEB_BUILD_LIBRARY)
#if defined(_WIN32) || defined(__MINGW32__) || defined(CRT_TARGET_OS_WINDOWS)
#define CRTWEB_API __declspec(dllexport)
#else
#define CRTWEB_API __attribute__((visibility("default")))
#endif
#else
#define CRTWEB_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Bumped only by an incompatible change. Every struct below starts with `struct_size`; the caller
 * sets it to sizeof of the struct it was compiled with, so fields can be appended compatibly. */
#define CRTWEB_API_VERSION 1u

typedef enum crtweb_result {
  CRTWEB_OK = 0,
  CRTWEB_WOULD_BLOCK = 1,             /* not an error: nothing to give yet, try again after a pump */
  CRTWEB_ERROR_INVALID_ARGUMENT = -1, /* null/odd argument, bad struct_size, malformed URL */
  CRTWEB_ERROR_STATE = -2,            /* wrong thread, runtime already shut down, view destroyed */
  CRTWEB_ERROR_OUT_OF_MEMORY = -3,
  CRTWEB_ERROR_UNSUPPORTED = -4,
  CRTWEB_ERROR_PLATFORM = -5          /* the web processes could not be started or were lost */
} crtweb_result;

typedef struct crtweb_runtime crtweb_runtime;
typedef struct crtweb_view crtweb_view;

CRTWEB_API uint32_t crtweb_api_version(void);
CRTWEB_API const char* crtweb_result_string(crtweb_result result);

/* ---- runtime ------------------------------------------------------------------------------- */

typedef struct crtweb_runtime_config {
  uint32_t struct_size;
  /* Directory holding the WebProcess and NetworkProcess executables. NULL: the directory of the
   * running executable. */
  const char* process_directory;
  /* A product token appended to the user agent ("MyApp" gives "... Safari/605.1.15 MyApp/605.1.15":
   * WebKit adds its own version). NULL: none. */
  const char* application_name;
} crtweb_runtime_config;

/* config may be NULL for the defaults. At most one runtime exists per process in v1
 * (CRTWEB_ERROR_STATE on the second); the creating thread owns it. The web processes are started
 * lazily by the first view. */
CRTWEB_API crtweb_result crtweb_runtime_create(const crtweb_runtime_config* config, crtweb_runtime** out_runtime);

/* Destroys every remaining view first. Waits (bounded, a few seconds) for the Web process of each
 * to exit. */
CRTWEB_API void crtweb_runtime_destroy(crtweb_runtime* runtime);

/* Runs the web event loop on the calling (owning) thread: IPC, page work, painting notifications,
 * and the callbacks of every view. Returns after up to timeout_ms (0 = one pass without waiting);
 * with a non-zero timeout it returns early as soon as a callback was delivered or a frame became
 * available. Returns CRTWEB_OK, or CRTWEB_ERROR_STATE from another thread. */
CRTWEB_API crtweb_result crtweb_runtime_pump(crtweb_runtime* runtime, uint32_t timeout_ms);

/* ---- views --------------------------------------------------------------------------------- */

typedef struct crtweb_view_config {
  uint32_t struct_size;
  uint32_t width;         /* logical pixels, > 0 */
  uint32_t height;        /* logical pixels, > 0 */
  uint32_t scale_percent; /* device scale * 100; 0 means 100. v1 supports 100 only. */
} crtweb_view_config;

CRTWEB_API crtweb_result crtweb_view_create(crtweb_runtime* runtime, const crtweb_view_config* config,
                                            crtweb_view** out_view);
/* No callback is delivered after this returns. */
CRTWEB_API void crtweb_view_destroy(crtweb_view* view);

/* ---- navigation ---------------------------------------------------------------------------- */

/* url is UTF-8. file:, http: and https: are accepted; about:blank loads an empty page. The load is
 * asynchronous: observe it through on_load. */
CRTWEB_API crtweb_result crtweb_view_load_url(crtweb_view* view, const char* url);
/* html is UTF-8; base_url (NULL means about:blank) resolves relative URLs. */
CRTWEB_API crtweb_result crtweb_view_load_html(crtweb_view* view, const char* html, const char* base_url);
CRTWEB_API crtweb_result crtweb_view_stop(crtweb_view* view);
CRTWEB_API crtweb_result crtweb_view_reload(crtweb_view* view);
/* CRTWEB_WOULD_BLOCK when there is no such history entry (nothing was done). */
CRTWEB_API crtweb_result crtweb_view_go_back(crtweb_view* view);
CRTWEB_API crtweb_result crtweb_view_go_forward(crtweb_view* view);
CRTWEB_API int crtweb_view_can_go_back(const crtweb_view* view);
CRTWEB_API int crtweb_view_can_go_forward(const crtweb_view* view);

/* Copies the current title / URL (UTF-8, NUL-terminated, truncated to `capacity`) and always stores
 * the full length needed including the NUL in *out_needed (may be NULL). Empty string when unset. */
CRTWEB_API crtweb_result crtweb_view_get_title(const crtweb_view* view, char* buffer, size_t capacity,
                                               size_t* out_needed);
CRTWEB_API crtweb_result crtweb_view_get_url(const crtweb_view* view, char* buffer, size_t capacity,
                                             size_t* out_needed);

/* ---- script -------------------------------------------------------------------------------- */

/* Evaluates script in the main frame. The callback gets the result converted to a string (as
 * JavaScript's String(value); arrays join with commas, objects are "[object Object]"), or error != 0
 * with `result` NULL when the script threw, produced no value (undefined), or the page went away.
 * Delivered from crtweb_runtime_pump(). */
typedef void (*crtweb_script_callback)(crtweb_view* view, const char* result_utf8, int error, void* user_data);
CRTWEB_API crtweb_result crtweb_view_evaluate_script(crtweb_view* view, const char* script_utf8,
                                                     crtweb_script_callback callback, void* user_data);

/* ---- geometry and focus -------------------------------------------------------------------- */

CRTWEB_API crtweb_result crtweb_view_set_size(crtweb_view* view, uint32_t width, uint32_t height,
                                              uint32_t scale_percent);
CRTWEB_API crtweb_result crtweb_view_set_focus(crtweb_view* view, int focused);
CRTWEB_API crtweb_result crtweb_view_set_visible(crtweb_view* view, int visible);

/* ---- frames -------------------------------------------------------------------------------- */

typedef struct crtweb_frame_info {
  uint32_t struct_size;
  uint64_t serial; /* grows whenever the picture changed; 0 before the first paint */
  uint32_t width;  /* physical pixels of the view's current size */
  uint32_t height;
} crtweb_frame_info;

CRTWEB_API crtweb_result crtweb_view_get_frame_info(const crtweb_view* view, crtweb_frame_info* out_info);

/* Copies the top-left `width` x `height` pixels of the view into pixels (rows `stride_bytes` apart,
 * at least width * 4). CRTWEB_OK: complete picture, *out_serial (may be NULL) is its serial.
 * CRTWEB_WOULD_BLOCK: nothing painted yet, or the requested size is not the size the page has
 * painted for (a resize is in flight); nothing was written. Call again after a pump. */
CRTWEB_API crtweb_result crtweb_view_copy_frame(crtweb_view* view, void* pixels, uint32_t width, uint32_t height,
                                                size_t stride_bytes, uint64_t* out_serial);

/* ---- input --------------------------------------------------------------------------------- */

#define CRTWEB_MODIFIER_CONTROL 0x01u
#define CRTWEB_MODIFIER_SHIFT 0x02u
#define CRTWEB_MODIFIER_ALT 0x04u
#define CRTWEB_MODIFIER_META 0x08u
#define CRTWEB_MODIFIER_CAPS_LOCK 0x10u

typedef enum crtweb_view_pointer_action {
  CRTWEB_VIEW_POINTER_MOVE = 1,
  CRTWEB_VIEW_POINTER_DOWN = 2,
  CRTWEB_VIEW_POINTER_UP = 3
} crtweb_view_pointer_action;

/* Coordinates are view-local logical pixels. button: 1 primary, 2 middle, 3 secondary (0 for a
 * move). */
CRTWEB_API crtweb_result crtweb_view_pointer(crtweb_view* view, crtweb_view_pointer_action action, uint32_t button,
                                             double x, double y, uint32_t modifiers);
/* Deltas in logical pixels; positive y scrolls the content up (the wheel turning away from you). */
CRTWEB_API crtweb_result crtweb_view_wheel(crtweb_view* view, double x, double y, double delta_x, double delta_y);
/* keysym: an X11/XKB keysym; keycode: the Linux evdev code + 8 (both required, as in the web
 * surface protocol). */
CRTWEB_API crtweb_result crtweb_view_key(crtweb_view* view, int down, int repeat, uint32_t keysym, uint32_t keycode,
                                         uint32_t modifiers);
/* Committed text (UTF-8). v1 types ASCII through a US layout; composition is not supported. */
CRTWEB_API crtweb_result crtweb_view_text(crtweb_view* view, const char* utf8);

/* ---- callbacks ----------------------------------------------------------------------------- */

typedef enum crtweb_load_event {
  CRTWEB_LOAD_STARTED = 1,   /* a navigation began (url is the requested one) */
  CRTWEB_LOAD_COMMITTED = 2, /* the first bytes of the new document arrived */
  CRTWEB_LOAD_FINISHED = 3,  /* the document and its resources finished */
  CRTWEB_LOAD_FAILED = 4     /* the navigation failed; message describes why */
} crtweb_load_event;

typedef struct crtweb_view_callbacks {
  uint32_t struct_size;
  void* user_data;
  /* Any member may be NULL. */
  void (*on_load)(crtweb_view* view, crtweb_load_event event, const char* url_utf8, const char* message_utf8,
                  void* user_data);
  void (*on_title_changed)(crtweb_view* view, const char* title_utf8, void* user_data);
  void (*on_url_changed)(crtweb_view* view, const char* url_utf8, void* user_data);
  /* The picture changed; fetch it with crtweb_view_copy_frame(). Coalesced: at most one per pump. */
  void (*on_frame)(crtweb_view* view, uint64_t serial, void* user_data);
  /* A main-frame navigation is about to start. Return non-zero to allow it, 0 to cancel it.
   * Without this callback every navigation is allowed. */
  int (*on_navigation_request)(crtweb_view* view, const char* url_utf8, void* user_data);
  /* The Web process of this view crashed or was killed; the page is blank until a new load. */
  void (*on_process_terminated)(crtweb_view* view, void* user_data);
} crtweb_view_callbacks;

/* Replaces the callback set (NULL clears it). The struct is copied. */
CRTWEB_API crtweb_result crtweb_view_set_callbacks(crtweb_view* view, const crtweb_view_callbacks* callbacks);

#ifdef __cplusplus
}
#endif
