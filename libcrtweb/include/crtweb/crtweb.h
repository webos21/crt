#pragma once

/* crtweb: the CRT-owned web runtime and view API (Web Tranche 4).
 *
 * Plain C99. No WebKit, WPE, GLib or JavaScriptCore type appears here or is needed to use it:
 * the implementation (libcrtweb) is built over the PlatformCRT port and hides it. Contract,
 * ownership and threading rules: docs/design/libcrtweb_api_policy.md; evidence:
 * docs/acceptance/crtweb_acceptance.md.
 *
 * Model. A runtime owns a website data store (cookies, cache, storage: ephemeral or in a directory),
 * its web process pool and its views; a process may have several runtimes with isolated data. A view
 * is one page with its own Web process state, size, focus and navigation history. All runtimes belong
 * to the thread that created the first one and share one event loop: crtweb_runtime_pump() on any of
 * them services every runtime. Every callback is delivered from inside a pump, on that thread, never
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

/* ---- network trust ------------------------------------------------------------------------- */

#define CRTWEB_NETWORK_CA_PEM_MAX_SIZE 16384u

typedef struct crtweb_network_config {
  uint32_t struct_size;
  /* NUL-terminated PEM CA bundle, copied by configure. NULL/empty: no trust anchors; HTTPS fails
   * closed. Neither the host certificate store nor CURL_CA_BUNDLE/SSL_CERT_FILE is used. Maximum
   * 16384 bytes excluding NUL (CRT's portable child-process launch configuration budget). */
  const char* ca_pem;
  /* Only 0 or 1. Explicit, unsafe, development-only opt-out of chain AND hostname verification. */
  int insecure_skip_verify_for_local_development;
} crtweb_network_config;

/* Call on the future runtime owner's thread, before the first runtime_create. NULL restores the
 * defaults. Configuration is process-wide: WebKit can share a NetworkProcess between data stores.
 * Once runtime_create initializes the engine it is immutable (even if creation subsequently fails),
 * including after every runtime was destroyed (CRTWEB_ERROR_STATE). Calls must be externally
 * serialized with runtime creation. */
CRTWEB_API crtweb_result crtweb_network_configure(const crtweb_network_config* config);

/* ---- runtime ------------------------------------------------------------------------------- */

typedef struct crtweb_runtime_config {
  uint32_t struct_size;
  /* Directory holding the WebProcess and NetworkProcess executables. NULL: the directory of the
   * running executable. */
  const char* process_directory;
  /* A product token appended to the user agent ("MyApp" gives "... Safari/605.1.15 MyApp/605.1.15":
   * WebKit adds its own version). NULL: none. */
  const char* application_name;
  /* Directory for this runtime's website data: cookies, cache, local storage. NULL: ephemeral (in
   * memory only, gone with the runtime). Two runtimes must not share a directory. */
  const char* data_directory;
  /* A crtweb_cookie_policy; 0 accepts all cookies. */
  uint32_t cookie_policy;
} crtweb_runtime_config;

/* config may be NULL for the defaults. Any number of runtimes may exist, all used from the thread
 * that created the first (CRTWEB_ERROR_STATE from another). The web processes are started lazily by
 * the first view. */
CRTWEB_API crtweb_result crtweb_runtime_create(const crtweb_runtime_config* config, crtweb_runtime** out_runtime);

/* Destroys every remaining view first. Waits up to two seconds for owned Web/Network processes to
 * exit, then terminates unresponsive children and waits up to two more seconds for reaping.
 * A Network process shared with another live runtime is left with that runtime. */
CRTWEB_API void crtweb_runtime_destroy(crtweb_runtime* runtime);

/* Runs the web event loop on the calling (owning) thread: IPC, page work, painting notifications,
 * and the callbacks of every view of every runtime. Returns after up to timeout_ms (0 = one pass without waiting);
 * with a non-zero timeout it returns early as soon as a callback was delivered or a frame became
 * available. Returns CRTWEB_OK, or CRTWEB_ERROR_STATE from another thread. */
CRTWEB_API crtweb_result crtweb_runtime_pump(crtweb_runtime* runtime, uint32_t timeout_ms);

/* ---- website data -------------------------------------------------------------------------- */

typedef enum crtweb_cookie_policy {
  CRTWEB_COOKIES_ALWAYS = 0,     /* accept every cookie */
  CRTWEB_COOKIES_NEVER = 1,      /* accept none */
  CRTWEB_COOKIES_NO_THIRD_PARTY = 2 /* accept cookies of the visited site only */
} crtweb_cookie_policy;

CRTWEB_API crtweb_result crtweb_runtime_set_cookie_policy(crtweb_runtime* runtime, crtweb_cookie_policy policy);

#define CRTWEB_DATA_COOKIES 0x1u /* every cookie of the runtime */
#define CRTWEB_DATA_CACHE 0x2u   /* the HTTP network cache */

/* Removes the given kinds of website data (a bit set) from the runtime. The callback (may be NULL)
 * runs from crtweb_runtime_pump() once every kind has been removed. */
typedef void (*crtweb_data_callback)(crtweb_runtime* runtime, crtweb_result result, void* user_data);
CRTWEB_API crtweb_result crtweb_runtime_clear_data(crtweb_runtime* runtime, uint32_t types,
                                                   crtweb_data_callback callback, void* user_data);

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

/* ---- input method -------------------------------------------------------------------------- */

/* The application's input method owns composition; these calls hand its state to the page
 * (compositionstart/update/end events, the marked text and the committed result).
 * Shows `text_utf8` as the marked (composing) text; the caret sits at `cursor_start` UTF-16 units into
 * it with `cursor_length` selected. */
CRTWEB_API crtweb_result crtweb_view_ime_set_composition(crtweb_view* view, const char* text_utf8,
                                                         uint32_t cursor_start, uint32_t cursor_length);
/* Commits text_utf8 (any UTF-8; a Hangul syllable, a word): replaces the composition if one is active,
 * otherwise inserts at the caret. */
CRTWEB_API crtweb_result crtweb_view_ime_commit(crtweb_view* view, const char* text_utf8);
/* Removes the composition without committing it. */
CRTWEB_API crtweb_result crtweb_view_ime_cancel(crtweb_view* view);

/* ---- callbacks ----------------------------------------------------------------------------- */

typedef enum crtweb_load_event {
  CRTWEB_LOAD_STARTED = 1,   /* a navigation began (url is the requested one) */
  CRTWEB_LOAD_COMMITTED = 2, /* the first bytes of the new document arrived */
  CRTWEB_LOAD_FINISHED = 3,  /* the document and its resources finished */
  CRTWEB_LOAD_FAILED = 4     /* the navigation failed; message describes why */
} crtweb_load_event;

typedef struct crtweb_download_info {
  uint32_t struct_size;
  uint32_t id;                      /* identifies the download in the progress and finished callbacks */
  const char* url;
  const char* suggested_filename;   /* from Content-Disposition or the URL; may be empty */
  const char* mime_type;
  uint64_t expected_length;         /* 0 when unknown */
} crtweb_download_info;

typedef enum crtweb_download_status {
  CRTWEB_DOWNLOAD_OK = 0,
  CRTWEB_DOWNLOAD_FAILED = 1,
  CRTWEB_DOWNLOAD_CANCELLED = 2
} crtweb_download_status;

typedef enum crtweb_dialog_kind {
  CRTWEB_DIALOG_ALERT = 1,
  CRTWEB_DIALOG_CONFIRM = 2,
  CRTWEB_DIALOG_PROMPT = 3,
  CRTWEB_DIALOG_BEFORE_UNLOAD = 4 /* the "leave this page?" confirmation */
} crtweb_dialog_kind;

/* A pending camera/microphone permission request (Tranche 8C). The request is a one-shot token: it is
 * resolved exactly once by crtweb_media_permission_complete(), and is cancelled (denied) by document
 * replacement, view destruction or process loss. No grant exists unless the application gives one.
 * The library owns token storage until its runtime is destroyed: do not free a token, and discard
 * application-held tokens when destroying that runtime. A late completion after view destruction
 * is safely rejected while the owning runtime is still alive. */
typedef struct crtweb_media_permission crtweb_media_permission;

#define CRTWEB_MEDIA_KIND_CAMERA 0x1u
#define CRTWEB_MEDIA_KIND_MICROPHONE 0x2u
#define CRTWEB_MEDIA_KIND_DISPLAY 0x4u

typedef struct crtweb_media_permission_info {
  uint32_t struct_size;
  const char* requesting_origin_utf8; /* the document that called getUserMedia */
  const char* top_level_origin_utf8;  /* the page's top-level origin */
  uint32_t kinds;                     /* CRTWEB_MEDIA_KIND_* the request needs */
  uint32_t camera_count;              /* camera devices a grant can pick from (opaque, privacy-filtered) */
} crtweb_media_permission_info;

/* A pending JavaScript dialog. The page is blocked until crtweb_dialog_complete() is called, from
 * inside the callback or later. */
typedef struct crtweb_dialog crtweb_dialog;

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
  /* The Web process of this view crashed or was killed. Reported once per termination; frame copy
   * returns WOULD_BLOCK until a new paint. The same view supports reload/load_url/load_html for
   * recovery, including from this callback. The application chooses whether/what to reload. */
  void (*on_process_terminated)(crtweb_view* view, void* user_data);

  /* A response that is not shown (an attachment or a type the engine cannot display) is a download.
   * Write the destination path (UTF-8, NUL-terminated, at most `capacity` bytes) into `path` and return
   * non-zero to save it there (an existing file is overwritten), or return 0 to cancel. Without this
   * callback downloads are cancelled. */
  int (*on_download_request)(crtweb_view* view, const crtweb_download_info* info, char* path, size_t capacity,
                             void* user_data);
  void (*on_download_progress)(crtweb_view* view, uint32_t id, uint64_t received_bytes, uint64_t expected_bytes,
                               void* user_data);
  void (*on_download_finished)(crtweb_view* view, uint32_t id, crtweb_download_status status, const char* path_utf8,
                               void* user_data);

  /* window.open() / target=_blank. Return non-zero to let the page open a new window: the library then
   * creates a new view in the same runtime (the opener's size) and reports it with on_popup_created
   * so the application can set its callbacks and show it. Without this callback popups are blocked. */
  int (*on_popup_request)(crtweb_view* opener, const char* url_utf8, void* user_data);
  void (*on_popup_created)(crtweb_view* opener, crtweb_view* popup, void* user_data);
  /* window.close() from the page: the application decides whether to destroy the view. */
  void (*on_close_requested)(crtweb_view* view, void* user_data);

  /* alert/confirm/prompt/beforeunload. Answer with crtweb_dialog_complete(). Without this callback an
   * alert is dismissed, a confirm and a prompt are cancelled and a beforeunload allows leaving. */
  void (*on_dialog)(crtweb_view* view, crtweb_dialog* dialog, crtweb_dialog_kind kind, const char* message_utf8,
                    const char* default_text_utf8, void* user_data);

  /* getUserMedia needs the application's decision (Tranche 8C; video camera only). Answer with
   * crtweb_media_permission_complete(). Without this callback every request is denied. A request for
   * the microphone or the screen is never granted by this API: grant fails with
   * CRTWEB_ERROR_UNSUPPORTED and the request is denied. */
  void (*on_media_permission)(crtweb_view* view, crtweb_media_permission* request,
                              const crtweb_media_permission_info* info, void* user_data);
} crtweb_view_callbacks;

/* Resolves a media permission request once. grant: non-zero allows the camera `camera_index` of the
 * request (0 is the first); zero denies. A second completion, or one after the request was cancelled,
 * returns CRTWEB_ERROR_INVALID_ARGUMENT. */
CRTWEB_API crtweb_result crtweb_media_permission_complete(crtweb_media_permission* request, int grant,
                                                          uint32_t camera_index);

/* Revokes camera access of this view: every track it holds ends and its capture source is released;
 * another view's authorization is untouched. Further requests ask the application again. */
CRTWEB_API crtweb_result crtweb_view_revoke_media(crtweb_view* view);

/* Completes a dialog exactly once. accepted: OK/true (alert: ignored); text_utf8: a prompt's answer
 * (NULL means empty). Destroying the view completes its pending dialogs as cancelled. */
CRTWEB_API crtweb_result crtweb_dialog_complete(crtweb_dialog* dialog, int accepted, const char* text_utf8);

/* Replaces the callback set (NULL clears it). The struct is copied. */
CRTWEB_API crtweb_result crtweb_view_set_callbacks(crtweb_view* view, const crtweb_view_callbacks* callbacks);

#ifdef __cplusplus
}
#endif
