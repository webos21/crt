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
  /* crtgfx/gpu.h's own crtgfx_gpu_fence_wait() only -- a real, expected,
   * non-fatal "not signaled within the given timeout" result, distinct
   * from CRTGFX_ERROR_HOST (a genuine host/device failure). Added
   * 2026-09-03 for the GPU resource contract. */
  CRTGFX_ERROR_TIMEOUT = -4,
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

/* The one real format every backend produces and consumes today. Named
 * fully rather than split into separate alpha-mode/color-space fields:
 * "PREMULTIPLIED" already states the alpha mode explicitly, and every
 * backend's own real presentation path (Linux wl_shm ARGB8888, Windows a
 * DXGI `DXGI_FORMAT_B8G8R8A8_UNORM` swap chain, macOS CGImage) treats the
 * channel bytes as plain sRGB-range integers with no color management
 * applied anywhere in this
 * pipeline -- stated here in the doc comment (2026-08-30, software
 * frame contract work) rather than as a `color_space` struct field with
 * exactly one legal value and no real consumer asking for a second one
 * yet (this project's own Skia bridge, `src/skia_bridge.cc`, does not
 * tag a color space either -- `SkImageInfo::Make()`'s 4-arg overload
 * defaults to none). Add a real field once a real second format/space
 * exists to distinguish, not speculatively ahead of one. */
typedef enum crtgfx_pixel_format {
  CRTGFX_PIXEL_FORMAT_BGRA8888_PREMULTIPLIED = 1,
} crtgfx_pixel_format;

typedef struct crtgfx_framebuffer {
  void* pixels;
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  crtgfx_pixel_format format;
  /* Incremented every time the backing allocation behind `pixels` is
   * actually reallocated/resized (crtgfx_weston_resize_software_buffer(),
   * `src/wayland_weston.c`) -- NOT once per frame, since most frames
   * reuse the same allocation untouched. Added 2026-08-30 (software
   * frame contract work) to make an already-real but previously-implicit
   * invariant checkable: every crtgfx_framebuffer this API hands out is
   * only valid for the begin_frame()/end_frame() pair that produced it
   * (the existing, tight-loop usage every caller and test already
   * follows) -- a caller that mistakenly held onto a `pixels` pointer or
   * `stride` value across a later resize can compare generation numbers
   * to detect that itself, rather than the size mismatch silently
   * producing a wrong-looking image with no diagnostic signal at all. */
  uint64_t generation;
} crtgfx_framebuffer;

/* A single dirty rectangle in framebuffer pixel coordinates (top-left
 * origin, matching every crtgfx_event pointer coordinate's own
 * convention). Added 2026-08-30 for crtgfx_window_end_frame_damaged()
 * below. */
typedef struct crtgfx_damage_rect {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
} crtgfx_damage_rect;

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
/* Added 2026-08-29, Phase 1 of the window/event API completion plan:
 *  - CRTGFX_EVENT_RESIZE fires whenever a window's live size actually
 *    changes (compared against its own previous size, not fired on every
 *    native configure/WM_SIZE/windowDidResize: callback regardless of
 *    whether anything changed) -- queued from the one shared crtgfx_
 *    weston_toplevel_note_size() implementation every host backend
 *    already funnels its own resize notification through, so this is a
 *    single shared-layer change, not three separate per-backend ones.
 *  - CRTGFX_EVENT_CLOSE_REQUESTED fires from the same shared crtgfx_
 *    weston_toplevel_note_close() every backend already calls on a native
 *    close signal (xdg_toplevel::close, WM_CLOSE, windowShouldClose:).
 *    crtgfx_window_should_close() (the original, still-supported polling
 *    API) keeps working unchanged -- this event is an additive way to
 *    learn about the same signal through the event queue instead of a
 *    separate poll, useful once a caller is already event-driven.
 *  - CRTGFX_EVENT_FOCUS_IN/FOCUS_OUT track *keyboard* input focus only
 *    (matching every desktop OS's own definition of "window focus"),
 *    never pointer hover/enter-leave -- Linux fires these from real
 *    wl_keyboard::enter/leave (now that more than one window can share a
 *    seat, the compositor's own enter/leave is what tells this backend
 *    which specific window the seat is currently talking to).
 *  - CRTGFX_EVENT_EXPOSE fires exactly once per window, the first time it
 *    becomes visible (from crtgfx_window_show()'s success path) -- a
 *    narrower, honestly-scoped stand-in for X11's damage-driven Expose
 *    concept, which has no real equivalent on a compositor that always
 *    composites from the client's last-committed buffer (Wayland, Cocoa,
 *    Win32/DWM all work this way): there is nothing to "expose" beyond
 *    "you are now mapped, render your first frame whenever you're ready."
 *    Not re-fired on every later repaint.
 *  - CRTGFX_EVENT_POINTER_SCROLL carries a wheel/trackpad scroll delta
 *    (data.pointer_scroll.dx/dy). Sign/scale follow each host's own wire
 *    convention directly (Wayland wl_pointer::axis's wl_fixed_t value,
 *    Windows WM_MOUSEWHEEL/WM_MOUSEHWHEEL's WHEEL_DELTA-scaled units)
 *    without an extra normalization pass -- reasoned from each protocol's
 *    own documented sign convention, not independently confirmed against
 *    a real physical scroll wheel this session (WSL/WSLg has no such
 *    input device to test with); flagged here rather than silently
 *    assumed, matching this project's own "reasoned but flagged
 *    unverified" discipline.
 *  - CRTGFX_EVENT_DPI_SCALE_CHANGED (data.dpi_scale.scale, 1.0 = 100%) is
 *    delivered on all three hosts as of 2026-08-30: Windows fires it from
 *    real WM_DPICHANGED (after a process-wide SetProcessDpiAwarenessContext
 *    (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) declaration, without
 *    which Windows falls back to bitmap-stretching the window instead of
 *    delivering real per-monitor DPI at all); Linux binds wl_output at
 *    version 2 and fires it from real wl_output::scale combined with
 *    wl_surface::enter/leave (which output a window is currently on);
 *    macOS fires it from real -backingScaleFactor via a real
 *    windowDidChangeBackingProperties: delegate callback, reasoned-but-
 *    not-verified like the rest of this session's macOS work (no host
 *    access) -- see HISTORY.md's 2026-08-30 entries for the full
 *    per-host trail.
 *
 * Added 2026-08-30, software frame contract work:
 *  - CRTGFX_EVENT_FRAME_COMPLETE fires once per crtgfx_window_end_frame()/
 *    crtgfx_window_end_frame_damaged() call, when the host has finished
 *    consuming that submitted buffer -- the real, per-host meaning of
 *    "presentation completion" this backend can actually deliver today
 *    varies by host precision, stated honestly rather than papered over:
 *    Linux requests a real `wl_surface::frame` callback (a genuine
 *    compositor-timed "ready for your next frame" signal, the same
 *    mechanism real Wayland vsync pacing uses); macOS requests a real
 *    `-[CATransaction setCompletionBlock:]` callback on the same
 *    transaction that presents the frame -- also genuinely asynchronous
 *    (never fires inside `-commit` itself; live-measured on real
 *    hardware to land tens of microseconds after `-commit` returns, on
 *    the very next crtgfx_window_pump_events() cycle -- see
 *    src/arch/macos/window_cocoa.c's own crtgfx_cocoa_frame_complete_
 *    invoke for the full account), though still not a real display-vsync
 *    timestamp the way Wayland's callback is, only "Core Animation has
 *    processed this transaction"; Windows (added 2026-08-30, after
 *    empirically confirming no per-window completion signal exists for a
 *    plain GDI window on this project's own real dev machine --
 *    `DwmGetCompositionTimingInfo()` returns `E_INVALIDARG` for a real
 *    GDI `HWND` every time, and its own `hwnd=NULL` global-desktop form
 *    advances regardless of whether this process draws anything at all,
 *    neither of which is a real per-buffer signal) now presents through a
 *    real DXGI flip-model swap chain instead of `StretchDIBits`
 *    specifically to get one: `IDXGISwapChain2::
 *    GetFrameLatencyWaitableObject()` is a genuine per-swap-chain kernel
 *    object the OS signals once that swap chain's own previously
 *    presented buffer has actually been retired, polled non-blocking from
 *    `crtgfx_host_window_dispatch()` (see `src/arch/windows/
 *    window_win32.c`'s own `crtgfx_win_poll_frame_complete()`) -- the
 *    Windows analog of Linux's `wl_surface::frame` and macOS's
 *    `CATransaction` completion block, and, like both of those, not
 *    guaranteed vsync-precise (confirmed for real: an occluded/not-
 *    currently-composited window's own `Present()` still returns a real,
 *    documented non-error status, `DXGI_STATUS_OCCLUDED`, and the
 *    waitable can signal faster than real display refresh in that case --
 *    the same class of caveat Wayland's own callback and CATransaction's
 *    own completion block already carry for a surface that is not
 *    actually visible on screen right now). A caller pacing strictly to
 *    real monitor refresh should not treat any of the three hosts' own
 *    completion timing as vsync-accurate. */
typedef enum crtgfx_event_type {
  CRTGFX_EVENT_NONE = 0,
  CRTGFX_EVENT_KEY_DOWN = 1,
  CRTGFX_EVENT_KEY_UP = 2,
  CRTGFX_EVENT_TEXT = 3,
  CRTGFX_EVENT_POINTER_MOTION = 4,
  CRTGFX_EVENT_POINTER_BUTTON_DOWN = 5,
  CRTGFX_EVENT_POINTER_BUTTON_UP = 6,
  CRTGFX_EVENT_RESIZE = 7,
  CRTGFX_EVENT_CLOSE_REQUESTED = 8,
  CRTGFX_EVENT_FOCUS_IN = 9,
  CRTGFX_EVENT_FOCUS_OUT = 10,
  CRTGFX_EVENT_EXPOSE = 11,
  CRTGFX_EVENT_POINTER_SCROLL = 12,
  CRTGFX_EVENT_DPI_SCALE_CHANGED = 13,
  CRTGFX_EVENT_FRAME_COMPLETE = 14,
} crtgfx_event_type;

/* Key-repeat policy (decided 2026-08-29, Phase 1): a held key's repeat is
 * whatever the host OS/compositor's own repeat behavior produces --
 * Wayland re-delivers real wl_keyboard::key press events at the seat's
 * own advertised rate (wl_keyboard::repeat_info), Win32 re-delivers real
 * WM_KEYDOWN/WM_CHAR messages the same way, Cocoa re-delivers real
 * NSEventTypeKeyDown the same way -- this backend does not suppress,
 * re-time, or synthesize repeat on its own, and crtgfx_event carries no
 * separate "is this a repeat" flag. Chosen over defining a project-owned
 * repeat rate because every host already solves this correctly and
 * consistently with its own platform's system-wide repeat-rate setting;
 * reimplementing it here would only risk disagreeing with that setting,
 * not improve on it. A caller that specifically needs to distinguish an
 * initial press from a repeat (rare -- most consumers, including a text
 * editor, want repeats to behave exactly like distinct presses) can still
 * do so itself by tracking KEY_UP between two KEY_DOWNs for the same
 * keycode. */

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
    struct {
      double dx;
      double dy;
    } pointer_scroll;
    struct {
      uint32_t width;
      uint32_t height;
    } resize;
    struct {
      /* 1.0 = 100% (the historical, still-common baseline). See
       * CRTGFX_EVENT_DPI_SCALE_CHANGED's own comment above -- no backend
       * fires this event yet, so no real value has ever been produced by
       * this field; it exists so the wire shape is stable once one does. */
      double scale;
    } dpi_scale;
    /* CRTGFX_EVENT_CLOSE_REQUESTED/FOCUS_IN/FOCUS_OUT/EXPOSE/
     * FRAME_COMPLETE carry no payload -- the event type alone is the
     * whole message, and the crtgfx_window* a caller already polled it
     * from is the "which window" answer. */
  } data;
} crtgfx_event;

/* Multi-window contract (2026-08-29, Phase 1): crtgfx_window_create() may
 * be called more than once per process. Every host backend multiplexes
 * every live window over one shared, process-wide native event source --
 * one Wayland connection on Linux (crtgfx_wl_connection, one wl_display
 * fd shared by every window's own wl_surface/xdg_toplevel rather than a
 * separate connection per window), one thread message queue on Windows
 * (PeekMessageA(..., hWnd=NULL, ...) already dispatches every HWND on the
 * calling thread), one NSApplication run loop on macOS -- so a single
 * crtgfx_window_pump_events() call receives and routes native events for
 * every window that exists, not just one. Each window's own crtgfx_
 * window_poll_event() then drains only that window's own event queue
 * (crtgfx_weston_toplevel::event_queue, allocated per crtgfx_window, never
 * shared). There is no artificial cap on how many windows may exist at
 * once; a host's own real resource limits are the only ceiling.
 *
 * Threading contract, matching every real backend's own native
 * requirement, not just this project's own preference: crtgfx_window_
 * create()/_destroy()/_show()/_pump_events()/_poll_event() must all be
 * called from the same single thread for a given process (Win32 messages
 * are thread-queue-bound by the OS itself; Cocoa's NSApplication run loop
 * is documented main-thread-only; this backend's own Wayland client has
 * no internal locking at all). Calling any of them from a second thread
 * is undefined behavior this API does not attempt to guard against.
 *
 * crtgfx_window_pump_events(timeout_ms) blocks the calling thread for up
 * to timeout_ms milliseconds waiting for at least one native event across
 * every live window, returning earlier once one arrives; 0 means "poll
 * once, do not block at all". It is the only function that actually
 * receives new native events -- crtgfx_window_poll_event() only drains
 * whatever crtgfx_window_pump_events() already queued.
 *
 * Event queue overflow/ordering (already implemented per-window in
 * crtgfx_weston_toplevel_note_event(), documented here for the public
 * contract): each window's own queue is a fixed-capacity FIFO ring buffer
 * (CRTGFX_EVENT_QUEUE_CAPACITY, currently 64) -- strict arrival order is
 * always preserved for events that do get queued, and once a window's own
 * queue is full, a newly arriving event for that window is dropped rather
 * than overwriting the oldest still-unread one, so a caller that falls
 * behind loses its most recent events, never a silent gap in the middle
 * of ones it already started reading. One window's queue can never
 * overflow because of another window's own traffic -- queues are strictly
 * per-window, not shared. */
int crtgfx_window_create(const crtgfx_window_desc* desc, crtgfx_window** out_window);
void crtgfx_window_destroy(crtgfx_window* window);
int crtgfx_window_show(crtgfx_window* window);
int crtgfx_window_pump_events(uint32_t timeout_ms);
int crtgfx_window_get_size(crtgfx_window* window, uint32_t* out_width, uint32_t* out_height);
int crtgfx_window_should_close(crtgfx_window* window);
/* Software frame buffer ownership (documented explicitly 2026-08-30 --
 * every host already satisfied this, just not stated together in one
 * place before): from a successful crtgfx_window_begin_frame() until the
 * matching crtgfx_window_end_frame()/_end_frame_damaged() call returns,
 * the returned crtgfx_framebuffer's own `pixels` allocation is owned by
 * the caller (the producer) -- free to write into it, never to be read
 * or written by anything else. Once that end_frame call returns, the
 * caller's own ownership ends and the allocation is once again this
 * library's own to manage (realloc it on the next resize, or reuse it
 * for the next begin_frame -- see crtgfx_framebuffer::generation's own
 * comment above), regardless of whether the host has *finished*
 * consuming the pixels it was just handed (CRTGFX_EVENT_FRAME_COMPLETE
 * reports that separately, asynchronously, and a caller does not need to
 * wait for it before calling begin_frame() again). Every real backend
 * satisfies "the caller's next begin_frame() never corrupts pixels the
 * host has not consumed yet" the same way in spirit -- guarantee the
 * host has its own independent copy or confirmation before end_frame()
 * returns -- but via different real mechanisms: Linux keeps the
 * submitted `wl_shm` buffer itself alive and gates reusing that specific
 * allocation on the compositor's own real `wl_buffer::release` (the
 * producer/consumer handoff is the buffer object itself, not a copy);
 * Windows' `ID3D11DeviceContext::UpdateSubresource()` and macOS's
 * `CGDataProviderCreateWithData()` path both copy the caller's pixels
 * into host-owned storage before end_frame() returns, so the original
 * allocation is immediately safe to reuse without waiting on anything
 * host-side at all. Both are the same
 * contract kept, through a different real policy -- not a gap on
 * Windows/macOS, and not something a caller needs to know to use this
 * API correctly either way. */
int crtgfx_window_begin_frame(crtgfx_window* window, crtgfx_framebuffer* out_framebuffer);
/* Presents the whole framebuffer -- a thin convenience wrapper over
 * crtgfx_window_end_frame_damaged(window, 0, 0) below (an empty/null
 * damage-rect list means "everything changed", the only behavior this
 * function ever had before 2026-08-30). Prefer crtgfx_window_end_frame_
 * damaged() directly once a caller can cheaply track what actually
 * changed -- see that function's own comment for why. */
int crtgfx_window_end_frame(crtgfx_window* window);
/* Added 2026-08-30 (software frame contract work): presents only the
 * pixels inside `damage_rects` (each in framebuffer pixel coordinates),
 * letting a host that supports partial present skip re-sending/
 * re-compositing the untouched remainder of the frame -- a real
 * optimization on Linux (multiple real wl_surface::damage requests
 * instead of one covering the whole surface). `damage_rects` may be
 * null / `damage_rect_count` may be 0 to mean "the whole frame changed"
 * (crtgfx_window_end_frame()'s own behavior). Rects outside the current
 * framebuffer bounds, or an empty list on a host that has no partial-
 * present path of its own, are never an error -- every host remains free
 * to present more than what was strictly requested (in particular,
 * macOS's own CALayer.contents replacement is inherently whole-image;
 * see src/arch/macos/window_cocoa.c's own comment). This is a real,
 * honest per-host capability difference, not a contract violation on
 * hosts that cannot do better -- a caller must never depend on anything
 * outside its declared damage actually staying unchanged on screen. */
int crtgfx_window_end_frame_damaged(
    crtgfx_window* window, const crtgfx_damage_rect* damage_rects, uint32_t damage_rect_count);
/* Pops the oldest queued input event into *out_event. Returns CRTGFX_OK
 * with out_event->type != CRTGFX_EVENT_NONE if an event was popped,
 * CRTGFX_OK with out_event->type == CRTGFX_EVENT_NONE if the queue was
 * empty, or an error code. Call after crtgfx_window_pump_events(), which
 * is what actually receives and queues new native events -- this only
 * drains the queue crtgfx_window_pump_events() already filled. */
int crtgfx_window_poll_event(crtgfx_window* window, crtgfx_event* out_event);

/* Testing-only hook (added 2026-08-30, Phase 2 of the window/event API
 * completion plan): pushes `*event` onto `window`'s own queue exactly as
 * if a real host backend had just received it -- the same internal call
 * (crtgfx_weston_toplevel_note_event()) every real backend already
 * makes, exposed here so this project's own deterministic regression
 * tests can exercise queue ordering, overflow, and multi-window routing
 * without needing real OS input delivery or a synthetic-input tool this
 * project's own hosts do not reliably have (no wtype/ydotool-equivalent
 * on Wayland/Win32/Cocoa). A real application has no reason to call
 * this -- it does not simulate anything a real backend does (no size/
 * focus/close side effects, just the queue push) and bypasses
 * crtgfx_window_pump_events() entirely, matching how a mocked event
 * differs from the real native one it stands in for. Subject to the
 * exact same per-window FIFO-with-drop-newest-on-overflow policy
 * crtgfx_window_poll_event() documents above (crtgfx/window.h's own
 * multi-window contract comment). Returns CRTGFX_ERROR_INVALID_ARGUMENT
 * if window or event is null, CRTGFX_OK otherwise (including when the
 * queue was already full and the event was silently dropped -- the same
 * "caller fell behind" case a real backend's own queue push already
 * treats as normal, not an error). */
int crtgfx_window_inject_event(crtgfx_window* window, const crtgfx_event* event);

#ifdef __cplusplus
}
#endif
