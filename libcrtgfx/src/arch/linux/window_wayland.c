#include "wayland_weston_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <xkbcommon/xkbcommon.h>

/* Opt-in wire-protocol tracing for the wl_seat/wl_keyboard/xkbcommon
 * negotiation path -- set CRTGFX_WAYLAND_DEBUG=1 in the environment to
 * enable, off by default (zero runtime cost beyond one getenv() call the
 * first time it's checked). Added 2026-08-25: this negotiation path had
 * never actually been exercised against a real, visibly-rendering
 * compositor before (WSLg's own RDP/RAIL bridge registered windows but
 * never composited their real pixel content, so no prior session could
 * tell whether keyboard input was even reaching this code at all) --
 * a permanent, opt-in trace facility is more useful here long-term than
 * a one-off debug patch that just gets reverted after each investigation. */
static int crtgfx_wl_debug_enabled(void) {
  static int checked = 0;
  static int enabled = 0;
  if (!checked) {
    const char* v = getenv("CRTGFX_WAYLAND_DEBUG");
    enabled = (v != 0 && v[0] != '0' && v[0] != 0);
    checked = 1;
  }
  return enabled;
}
#define CRTGFX_WL_TRACE(...)                             \
  do {                                                   \
    if (crtgfx_wl_debug_enabled()) {                     \
      fprintf(stderr, "[crtgfx-wayland] " __VA_ARGS__);  \
    }                                                    \
  } while (0)

/* Real Wayland client backend for Linux -- hand-rolled wire protocol, no
 * libwayland-client dependency, matching this project's own no-host-SDK
 * ethos (see libcrtgfx/src/arch/windows/window_win32.c: raw dllimport
 * declarations, no <windows.h>). Opcodes/argument layouts below were taken
 * directly from the real upstream wayland.xml and xdg-shell.xml protocol
 * definitions (core wl_display/wl_registry/wl_compositor/wl_shm/wl_surface
 * and the stable xdg_wm_base/xdg_surface/xdg_toplevel extension), not
 * guessed -- a wrong opcode or argument order is a hard protocol error
 * from the compositor's side, not a soft failure.
 *
 * Scope, matching docs/libcrtgfx_wayland_plan.md's "start with the
 * simplest available path" direction:
 *  - one shared Wayland connection per process (2026-08-29, Phase 1 of the
 *    window/event API completion plan): crtgfx_wl_connection holds the fd
 *    and every registry-bound singleton (wl_compositor/wl_shm/xdg_wm_base/
 *    wl_seat/wl_keyboard/wl_pointer/xkb state), created on the first
 *    crtgfx_host_window_create() call and reused by every window after
 *    that; crtgfx_host_window itself only holds what is genuinely
 *    per-window (its own wl_surface/xdg_surface/xdg_toplevel, its own
 *    wl_shm buffers). Previously each window opened its own independent
 *    connection -- harmless for exactly one window, but a real functional
 *    bug for two: a second connection is a distinct client from the
 *    compositor's own point of view, so its keyboard/pointer focus would
 *    never track this project's own multi-window model at all, and every
 *    window would have needlessly doubled up its own copy of the
 *    compositor/shm/seat bindings and xkb state;
 *  - software (wl_shm) presentation only, no GPU/EGL path yet;
 *  - keyboard and pointer input, including real per-window keyboard-focus
 *    routing (wl_keyboard::enter/leave) and pointer-focus routing
 *    (wl_pointer::enter/leave) now that more than one window can share the
 *    one real wl_seat this backend binds -- via this project's own
 *    libxkbcommon port (libcrtgfx/third_party/xkbcommon/recipe.json) for
 *    keyboard text composition; no wl_touch;
 *  - wl_buffer lifetime is release-tracked: every presented wl_shm buffer
 *    stays mapped/open until the compositor sends wl_buffer::release, then
 *    this backend destroys the wl_buffer object and unmaps/closes its backing
 *    storage. This is the first real frame-lifecycle contract shared with
 *    the higher crtgfx software-frame API;
 *  - partial present is real: crtgfx_host_window_present_software()'s own
 *    damage_rects, when given, become one real wl_surface::damage request
 *    per rect instead of one covering the whole surface (2026-08-30, the
 *    software-frame contract extension in TODO.md);
 *  - CRTGFX_EVENT_FRAME_COMPLETE is driven by a real wl_surface::frame
 *    request/wl_callback::done round trip, genuinely asynchronous (see
 *    crtgfx_host_window::frame_callback_id's own comment) -- unlike
 *    Windows/macOS, whose own presentation is synchronous (2026-08-30);
 *  - object ids are allocated monotonically from one shared per-connection
 *    counter, never recycled (fine for a short-lived process, not for one
 *    that opens/closes many windows over a long run).
 *
 * If no Wayland compositor is reachable at all (no $WAYLAND_DISPLAY/
 * $XDG_RUNTIME_DIR, connection refused, ...) crtgfx_host_window_create()
 * returns CRTGFX_ERROR_UNSUPPORTED -- the same signal the previous stub
 * always returned -- so headless CI keeps working exactly as before.
 * Once a connection succeeds, any further protocol-level failure is
 * treated as a real error (CRTGFX_ERROR_HOST), not silently downgraded. */

#define CRTGFX_WL_TIMEOUT_MS 2000u
#define CRTGFX_WL_MAX_MSG 4096u
#define CRTGFX_WL_NAME_NONE 0xffffffffu
#define CRTGFX_WL_FORMAT_ARGB8888 0u

/* wl_display (object id 1, implicit -- never explicitly created) */
#define WL_DISPLAY_SYNC 0u
#define WL_DISPLAY_GET_REGISTRY 1u
#define WL_DISPLAY_EVENT_ERROR 0u
#define WL_DISPLAY_EVENT_DELETE_ID 1u
/* wl_registry */
#define WL_REGISTRY_BIND 0u
#define WL_REGISTRY_EVENT_GLOBAL 0u
/* wl_callback */
#define WL_CALLBACK_EVENT_DONE 0u
/* wl_compositor */
#define WL_COMPOSITOR_CREATE_SURFACE 0u
/* wl_shm */
#define WL_SHM_CREATE_POOL 0u
/* wl_shm_pool */
#define WL_SHM_POOL_CREATE_BUFFER 0u
#define WL_SHM_POOL_DESTROY 1u
/* wl_buffer */
#define WL_BUFFER_DESTROY 0u
#define WL_BUFFER_EVENT_RELEASE 0u
/* wl_surface -- destroy is request 0 in every core-protocol interface
 * below that has one (confirmed consistent with this file's own already-
 * verified attach=1/damage=2/commit=6, xdg_surface's own get_toplevel=1/
 * ack_configure=4, and xdg_toplevel's own set_title=2, all taken directly
 * from the real upstream .xml -- request 0 being "destroy" is core
 * Wayland/xdg-shell convention, not a guess specific to this backend). */
#define WL_SURFACE_DESTROY 0u
#define WL_SURFACE_ATTACH 1u
#define WL_SURFACE_DAMAGE 2u
/* wl_surface::frame(new_id<wl_callback> callback) -- real wayland.xml
 * request opcode 3 (destroy=0/attach=1/damage=2/frame=3/...). The
 * resulting wl_callback fires exactly one wl_callback::done event "when
 * it is a good time for the client to start drawing a new frame", which
 * in practice only happens once the compositor has actually consumed/
 * composited the just-committed buffer -- the real, standard Wayland
 * mechanism this backend uses as its own genuinely asynchronous
 * CRTGFX_EVENT_FRAME_COMPLETE signal (see crtgfx/window.h's own doc
 * comment on that event type for why this is legitimately different
 * timing from Windows/macOS's synchronous fire). Added 2026-08-30. */
#define WL_SURFACE_FRAME 3u
#define WL_SURFACE_COMMIT 6u
/* wl_surface events (a separate opcode space from the requests just
 * above, real wayland.xml declaration order: enter, leave, ...) -- added
 * 2026-08-30 for CRTGFX_EVENT_DPI_SCALE_CHANGED: tell this backend which
 * wl_output(s) a window's surface currently overlaps, needed to look up
 * that output's own scale (WL_OUTPUT_EVENT_SCALE below). Previously
 * caught only by this file's own generic "anything else is intentionally
 * ignored" catch-all. */
#define WL_SURFACE_EVENT_ENTER 0u
#define WL_SURFACE_EVENT_LEAVE 1u
/* xdg_wm_base */
#define XDG_WM_BASE_GET_XDG_SURFACE 2u
#define XDG_WM_BASE_PONG 3u
#define XDG_WM_BASE_EVENT_PING 0u
/* xdg_surface */
#define XDG_SURFACE_DESTROY 0u
#define XDG_SURFACE_GET_TOPLEVEL 1u
#define XDG_SURFACE_ACK_CONFIGURE 4u
#define XDG_SURFACE_EVENT_CONFIGURE 0u
/* xdg_toplevel */
#define XDG_TOPLEVEL_DESTROY 0u
#define XDG_TOPLEVEL_SET_TITLE 2u
#define XDG_TOPLEVEL_EVENT_CONFIGURE 0u
#define XDG_TOPLEVEL_EVENT_CLOSE 1u
/* wl_seat */
#define WL_SEAT_GET_POINTER 0u
#define WL_SEAT_GET_KEYBOARD 1u
#define WL_SEAT_EVENT_CAPABILITIES 0u
/* Real wayland.xml wl_seat::capability bitfield enum: pointer=1,
 * keyboard=2, touch=4. */
#define WL_SEAT_CAPABILITY_POINTER (1u << 0)
#define WL_SEAT_CAPABILITY_KEYBOARD (1u << 1)
/* wl_output -- added 2026-08-30 for CRTGFX_EVENT_DPI_SCALE_CHANGED.
 * `scale` (the real, standard, integer-only Wayland HiDPI mechanism --
 * 1 = 100%, 2 = 200%, ...) was added in wl_output version 2 (real
 * wayland.xml `since="2"` on that event), so this backend binds wl_
 * output at version 2 specifically, unlike every other global here
 * (bound at version 1, since nothing past their own version-1 event set
 * is ever needed). geometry/mode/done/name/description events are all
 * left unhandled (this backend only ever needs the one integer factor,
 * not physical monitor geometry or naming), matching this file's own
 * "reasoned minimal scope" bar elsewhere (e.g. the key-repeat policy,
 * crtgfx/window.h's own doc comment). */
#define WL_OUTPUT_BIND_VERSION 2u
#define WL_OUTPUT_EVENT_SCALE 3u
/* wl_pointer -- event opcodes are declaration order in wayland.xml
 * (enter, leave, motion, button, axis, frame since v5, ...); this
 * backend only ever binds wl_seat/wl_pointer at version 1, so nothing
 * past axis is ever sent. surface_x/surface_y/value are wl_fixed_t, a
 * 24.8 signed fixed-point number packed into an int32 -- confirmed
 * directly against real upstream wayland-util.h's own wl_fixed_to_
 * double() (`return f / 256.0;`), not assumed. */
#define WL_POINTER_EVENT_ENTER 0u
#define WL_POINTER_EVENT_LEAVE 1u
#define WL_POINTER_EVENT_MOTION 2u
#define WL_POINTER_EVENT_BUTTON 3u
#define WL_POINTER_EVENT_AXIS 4u
/* wl_pointer::axis's own axis enum: vertical_scroll=0, horizontal_scroll=1
 * (real wayland.xml enum, not guessed). */
#define WL_POINTER_AXIS_VERTICAL_SCROLL 0u
#define WL_POINTER_AXIS_HORIZONTAL_SCROLL 1u
#define WL_POINTER_BUTTON_STATE_RELEASED 0u
#define WL_POINTER_BUTTON_STATE_PRESSED 1u
/* Real Linux evdev button codes (linux/input-event-codes.h -- confirmed
 * against a real copy of that header, matching this file's own already-
 * established discipline for KEY_* codes). wl_pointer::button's own doc
 * says the button field *is* one of these, not a separate enum. */
#define CRTGFX_BTN_LEFT 0x110u
#define CRTGFX_BTN_RIGHT 0x111u
#define CRTGFX_BTN_MIDDLE 0x112u
/* wl_keyboard -- event opcodes are declaration order in wayland.xml
 * (keymap, enter, leave, key, modifiers, repeat_info since v4); request
 * opcode (release, since v3) is never sent by this backend, which only
 * ever binds wl_seat/wl_keyboard at version 1. */
#define WL_KEYBOARD_EVENT_KEYMAP 0u
#define WL_KEYBOARD_EVENT_ENTER 1u
#define WL_KEYBOARD_EVENT_LEAVE 2u
#define WL_KEYBOARD_EVENT_KEY 3u
#define WL_KEYBOARD_EVENT_MODIFIERS 4u
#define WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 1u
#define WL_KEYBOARD_KEY_STATE_RELEASED 0u
#define WL_KEYBOARD_KEY_STATE_PRESSED 1u
/* wl_keyboard::key's own raw keycode is a Linux evdev code; xkbcommon's
 * own keycode space is offset by +8 (a real, historical X11-compatibility
 * offset xkbcommon inherited) -- confirmed directly in wayland.xml's own
 * wl_keyboard::keymap event doc ("to determine the xkb keycode, clients
 * must add 8 to the key event keycode") and by reading real upstream
 * libxkbcommon's own tools/interactive-wayland.c (its EVDEV_OFFSET, used
 * exactly this way against a real wl_keyboard::key event). Applied only
 * at the xkbcommon call boundary below -- crtgfx_event's own public
 * keycode field (crtgfx/window.h) stays a plain evdev code. */
#define CRTGFX_WL_XKB_KEYCODE_OFFSET 8u

struct crtgfx_wl_buffer {
  uint32_t id;
  int fd;
  void* data;
  uint32_t size;
  struct crtgfx_wl_buffer* next;
};

/* One tracked wl_output global -- a real multi-monitor setup can
 * advertise more than one, so this is a list, not a single field the
 * way compositor_id/shm_id/wm_base_id are (those are real Wayland
 * singletons; wl_output is not). Added 2026-08-30 for CRTGFX_EVENT_
 * DPI_SCALE_CHANGED. */
struct crtgfx_wl_output {
  uint32_t id;
  /* 1 until a real wl_output::scale event says otherwise -- 1 (100%) is
   * also real wayland.xml's own documented default for a compositor that
   * never sends scale at all (pre-v2 wl_output, or genuinely 100%). */
  int32_t scale;
  struct crtgfx_wl_output* next;
};

/* Shared, process-wide Wayland connection -- see this file's own top
 * comment for why one shared connection replaces the previous one-
 * connection-per-window design. Created lazily by the first crtgfx_host_
 * window_create() call (crtgfx_wl_connection_create() below) and torn
 * down once the last crtgfx_host_window sharing it is destroyed
 * (crtgfx_wl_connection_destroy(), called from crtgfx_host_window_
 * destroy() when its own windows list goes empty). */
struct crtgfx_wl_connection {
  int fd;
  uint32_t next_id;

  uint32_t registry_id;
  uint32_t sync_callback_id;
  uint32_t compositor_id;
  uint32_t shm_id;
  uint32_t wm_base_id;
  /* 0 means "no wl_seat/wl_keyboard/wl_pointer bound" -- object id 0 is
   * never a real Wayland object (id 1 is always wl_display, the lowest
   * object id this backend or any real compositor ever allocates), so it
   * doubles safely as an explicit "absent" sentinel without a separate
   * bool. wl_seat is optional (unlike wl_compositor/wl_shm/xdg_wm_base):
   * a compositor with no seat at all, or a seat with no keyboard/pointer
   * capability, just means no keyboard/pointer events are ever queued --
   * not a connection failure. */
  uint32_t seat_id;
  uint32_t keyboard_id;
  uint32_t pointer_id;

  /* wl_pointer::button carries no x/y of its own -- wayland.xml's own
   * doc says "The location of the click is given by the last motion,
   * warp or enter event", so the most recent one has to be tracked here
   * to fill in crtgfx_event.data.pointer_button.x/y on a button event. */
  double pointer_x;
  double pointer_y;
  /* wl_surface object id of whichever window currently has pointer/
   * keyboard focus, per the real wl_pointer::enter/leave and
   * wl_keyboard::enter/leave events (see wl_dispatch_message() below) --
   * 0 means "no window focused" (see the seat_id comment just above for
   * why 0 is always a safe sentinel). The seat is bound once, shared by
   * every window on this connection, so a connection-wide pointer/
   * keyboard event has to be routed to the one specific crtgfx_host_
   * window it actually belongs to now that more than one can exist --
   * this is exactly the piece the previous one-connection-per-window
   * design never needed, because there was only ever one window to route
   * to in the first place. */
  uint32_t pointer_focus_surface_id;
  uint32_t keyboard_focus_surface_id;

  /* xkbcommon state for the currently bound keyboard -- xkb_keymap/
   * xkb_state are both null until the real wl_keyboard::keymap event
   * arrives (see wl_dispatch_message()'s own WL_KEYBOARD_EVENT_KEYMAP
   * handling); xkb_context is created once, up front, and lives for the
   * whole connection's lifetime. One keyboard/keymap/state total, shared
   * by every window -- a real Wayland seat has exactly one keyboard
   * group, not one per surface. */
  struct xkb_context* xkb_context;
  struct xkb_keymap* xkb_keymap;
  struct xkb_state* xkb_state;

  /* Every wl_output this connection has bound (see struct crtgfx_wl_
   * output's own comment) -- added 2026-08-30 for CRTGFX_EVENT_DPI_
   * SCALE_CHANGED. */
  struct crtgfx_wl_output* outputs;

  /* Every live crtgfx_host_window sharing this connection (singly linked
   * via crtgfx_host_window::next below) -- used both for object-id-based
   * routing (crtgfx_wl_find_window_by_surface()/_owning_buffer() below)
   * and as the reference count for when to tear the connection itself
   * down (crtgfx_host_window_destroy() frees it once this list goes
   * empty). */
  struct crtgfx_host_window* windows;
};

struct crtgfx_host_window {
  struct crtgfx_wl_connection* conn;
  struct crtgfx_host_window* next;

  uint32_t surface_id;
  uint32_t xdg_surface_id;
  uint32_t xdg_toplevel_id;

  int have_first_configure;
  uint32_t configure_serial;

  struct crtgfx_wl_buffer* buffers;

  /* wl_output id this window's own surface most recently entered (real
   * wl_surface::enter/leave, see wl_dispatch_message() below) -- 0 means
   * "unknown/not yet entered any tracked output". A window briefly
   * spanning two outputs (e.g. mid-drag across monitors) simplifies to
   * "whichever it most recently entered", matching this file's own
   * "reasoned minimal scope" bar elsewhere -- a compositor's own
   * wl_surface::enter/leave ordering already reflects which output it
   * considers primary for that surface at any given moment, this does
   * not invent its own arbitration on top of that. Added 2026-08-30 for
   * CRTGFX_EVENT_DPI_SCALE_CHANGED. */
  uint32_t current_output_id;

  /* Object id of this window's own currently-outstanding wl_surface::
   * frame request (see WL_SURFACE_FRAME's own comment), or 0 if none is
   * pending. At most one outstanding at a time: crtgfx_host_window_
   * present_software() below only requests a new one once the previous
   * one's own wl_callback::done has actually arrived (or none was ever
   * requested yet) -- not because the protocol forbids more than one in
   * flight, but because requesting a second before the first fires would
   * leak the first's own object id for no observable benefit (this
   * backend never recycles object ids -- see this file's own top-of-file
   * scope note). Added 2026-08-30 for CRTGFX_EVENT_FRAME_COMPLETE. */
  uint32_t frame_callback_id;

  crtgfx_weston_toplevel* toplevel;
};

/* The one shared connection for this process, or null if no window is
 * currently open. crtgfx_host_window_dispatch() (see crtgfx/window.h's
 * crtgfx_window_pump_events()) takes no window argument, matching Win32's
 * thread-global message queue -- see this file's top comment; with a
 * shared connection this is now the natural, correct shape rather than a
 * limitation, since one poll()/drain pass on this one fd already carries
 * traffic for every window. */
static struct crtgfx_wl_connection* crtgfx_wl_conn;

/* Only populated during crtgfx_wl_connection_create()'s initial registry
 * roundtrip; passed through wl_dispatch_message() as an optional pointer. */
struct crtgfx_wl_bootstrap {
  uint32_t compositor_name;
  uint32_t shm_name;
  uint32_t wm_base_name;
  uint32_t seat_name;
  int sync_done;
};

static size_t wl_put_u32(unsigned char* buf, size_t off, uint32_t v) {
  memcpy(buf + off, &v, 4);
  return off + 4;
}

static uint32_t wl_get_u32(const unsigned char* body, size_t off) {
  uint32_t v;
  memcpy(&v, body + off, 4);
  return v;
}

static size_t wl_put_string(unsigned char* buf, size_t cap, size_t off, const char* s) {
  size_t len = strlen(s) + 1u; /* wire length includes the NUL terminator */
  size_t padded = (len + 3u) & ~(size_t)3u;

  if (off + 4u + padded > cap) {
    return (size_t)-1;
  }
  off = wl_put_u32(buf, off, (uint32_t)len);
  memcpy(buf + off, s, len);
  memset(buf + off + len, 0, padded - len);
  return off + padded;
}

static size_t wl_get_string(const unsigned char* body, size_t off, char* out, size_t out_cap) {
  uint32_t len = wl_get_u32(body, off);
  size_t padded = (((size_t)len) + 3u) & ~(size_t)3u;
  size_t copy = (size_t)len;

  if (copy >= out_cap) {
    copy = out_cap - 1u;
  }
  memcpy(out, body + off + 4u, copy);
  out[copy] = 0;
  return off + 4u + padded;
}

/* Header is object_id(u32) followed by one word packing (size << 16) |
 * opcode, size counted from the start of the header (real upstream wire
 * format -- see this file's top comment). fd, when >= 0, rides as
 * SCM_RIGHTS ancillary data on the same sendmsg() call (only
 * wl_shm::create_pool needs this). Takes the shared connection directly
 * (not a crtgfx_host_window) -- every request this backend ever sends
 * travels over the one shared fd, whether or not it is about a specific
 * window's own object. */
static int wl_send(
    struct crtgfx_wl_connection* conn, uint32_t object_id, uint32_t opcode, const void* body,
    size_t body_len, int pass_fd) {
  uint32_t header[2];
  struct iovec iov[2];
  struct msghdr msg;
  char cmsgbuf[CMSG_SPACE(sizeof(int))];
  ssize_t sent;
  size_t total;

  if (8u + body_len > 0xffffu) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  header[0] = object_id;
  header[1] = (uint32_t)(((8u + body_len) << 16) | (opcode & 0xffffu));

  iov[0].iov_base = header;
  iov[0].iov_len = sizeof(header);
  iov[1].iov_base = (void*)body;
  iov[1].iov_len = body_len;

  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = iov;
  msg.msg_iovlen = (body_len != 0u) ? 2 : 1;

  if (pass_fd >= 0) {
    struct cmsghdr* cmsg;

    memset(cmsgbuf, 0, sizeof(cmsgbuf));
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    memcpy(CMSG_DATA(cmsg), &pass_fd, sizeof(int));
  }

  total = sizeof(header) + body_len;
  sent = sendmsg(conn->fd, &msg, 0);
  return (sent == (ssize_t)total) ? CRTGFX_OK : CRTGFX_ERROR_HOST;
}

/* Finds the crtgfx_host_window (out of every one sharing `conn`) whose
 * own wl_surface, xdg_surface, or xdg_toplevel object id matches `id` --
 * the routing primitive every per-window event branch in
 * wl_dispatch_message() needs now that more than one window can share a
 * connection. Returns null for id 0 or no match (a message for a window
 * already destroyed, or a genuinely unrelated object -- both are
 * legitimate, not bugs, so callers treat null as "ignore, not an error"). */
static struct crtgfx_host_window* crtgfx_wl_find_window_by_surface(
    struct crtgfx_wl_connection* conn, uint32_t id) {
  struct crtgfx_host_window* w;

  if (conn == 0 || id == 0) {
    return 0;
  }
  for (w = conn->windows; w != 0; w = w->next) {
    if (w->surface_id == id || w->xdg_surface_id == id || w->xdg_toplevel_id == id) {
      return w;
    }
  }
  return 0;
}

/* Same idea as crtgfx_wl_find_window_by_surface(), for wl_buffer ids --
 * buffers are per-window (each window presents its own wl_shm content),
 * so a wl_buffer::release has to be checked against every window sharing
 * the connection, not just one. */
static struct crtgfx_host_window* crtgfx_wl_find_window_owning_buffer(
    struct crtgfx_wl_connection* conn, uint32_t id) {
  struct crtgfx_host_window* w;

  if (conn == 0) {
    return 0;
  }
  for (w = conn->windows; w != 0; w = w->next) {
    struct crtgfx_wl_buffer* b;

    for (b = w->buffers; b != 0; b = b->next) {
      if (b->id == id) {
        return w;
      }
    }
  }
  return 0;
}

/* Finds the crtgfx_host_window whose own pending wl_surface::frame
 * callback (frame_callback_id) matches `id` -- separate from crtgfx_wl_
 * find_window_by_surface() above since a frame callback's own object id
 * is not one of that function's three fixed per-window ids (surface/
 * xdg_surface/xdg_toplevel), even though it lives in the same shared
 * per-connection id space. Added 2026-08-30 for CRTGFX_EVENT_FRAME_
 * COMPLETE. */
static struct crtgfx_host_window* crtgfx_wl_find_window_by_frame_callback(
    struct crtgfx_wl_connection* conn, uint32_t id) {
  struct crtgfx_host_window* w;

  if (conn == 0 || id == 0) {
    return 0;
  }
  for (w = conn->windows; w != 0; w = w->next) {
    if (w->frame_callback_id == id) {
      return w;
    }
  }
  return 0;
}

/* Finds a tracked wl_output by its own object id (see struct crtgfx_wl_
 * output's own comment) -- added 2026-08-30 for CRTGFX_EVENT_DPI_SCALE_
 * CHANGED. */
static struct crtgfx_wl_output* crtgfx_wl_find_output(struct crtgfx_wl_connection* conn, uint32_t id) {
  struct crtgfx_wl_output* o;

  if (conn == 0 || id == 0) {
    return 0;
  }
  for (o = conn->outputs; o != 0; o = o->next) {
    if (o->id == id) {
      return o;
    }
  }
  return 0;
}

/* Queues CRTGFX_EVENT_DPI_SCALE_CHANGED for `host` using whatever output
 * it is currently on (host->current_output_id) -- called both when a
 * window enters a (possibly new) output and when an already-current
 * output's own scale changes, so either ordering of wl_surface::enter
 * vs. wl_output::scale (not guaranteed by the protocol) still ends up
 * correct once both pieces of information are known. Does nothing if the
 * window's current output is unknown or that output's scale has not
 * been learned yet (both real, legitimate states, not errors). Added
 * 2026-08-30. */
static void crtgfx_wl_note_dpi_scale(struct crtgfx_host_window* host) {
  struct crtgfx_wl_output* output;
  crtgfx_event event = {0};

  if (host == 0 || host->current_output_id == 0) {
    return;
  }
  output = crtgfx_wl_find_output(host->conn, host->current_output_id);
  if (output == 0) {
    return;
  }
  event.type = CRTGFX_EVENT_DPI_SCALE_CHANGED;
  event.data.dpi_scale.scale = (double)output->scale;
  crtgfx_weston_toplevel_note_event(host->toplevel, &event);
}

static void wl_buffer_destroy_storage(struct crtgfx_wl_buffer* buffer) {
  if (buffer == 0) {
    return;
  }
  if (buffer->data != 0) {
    munmap(buffer->data, buffer->size);
  }
  if (buffer->fd >= 0) {
    close(buffer->fd);
  }
  free(buffer);
}

/* Returns 1 if `id` was actually a tracked wl_buffer belonging to some
 * window on `conn` (and destroys it), 0 otherwise -- the caller needs
 * this to know whether it may safely treat the message as fully handled.
 * See its own call site's comment: wl_buffer::release and wl_seat::
 * capabilities are BOTH opcode 0 (every Wayland interface numbers its own
 * events from 0 independently), so "opcode == WL_BUFFER_EVENT_RELEASE"
 * alone is not enough to identify a message -- only checking that
 * object_id genuinely names a buffer this backend created actually
 * disambiguates it. */
static int wl_destroy_released_buffer(struct crtgfx_wl_connection* conn, uint32_t id) {
  struct crtgfx_host_window* w = crtgfx_wl_find_window_owning_buffer(conn, id);
  struct crtgfx_wl_buffer** link;

  if (w == 0) {
    return 0;
  }
  link = &w->buffers;
  while (*link != 0) {
    struct crtgfx_wl_buffer* buffer = *link;

    if (buffer->id == id) {
      *link = buffer->next;
      (void)wl_send(conn, id, WL_BUFFER_DESTROY, 0, 0, -1);
      wl_buffer_destroy_storage(buffer);
      return 1;
    }
    link = &buffer->next;
  }
  return 0;
}

static void wl_destroy_all_buffers(struct crtgfx_host_window* host) {
  struct crtgfx_wl_buffer* buffer;

  if (host == 0) {
    return;
  }
  buffer = host->buffers;
  host->buffers = 0;
  while (buffer != 0) {
    struct crtgfx_wl_buffer* next = buffer->next;
    wl_buffer_destroy_storage(buffer);
    buffer = next;
  }
}

/* out_fd (may be NULL): receives an SCM_RIGHTS fd if the kernel delivers
 * one with this call, else left untouched by this call (never reset to
 * -1 -- see wl_recv_message()'s own comment on why the header and body
 * reads share one out_fd across both calls). Real POSIX/Linux fact this
 * whole function exists to respect, not an implementation preference:
 * ancillary data (SCM_RIGHTS) on an AF_UNIX SOCK_STREAM socket is
 * delivered on the specific recvmsg() call that actually consumes the
 * byte(s) it was sent alongside -- a plain read() cannot retrieve it at
 * all (the kernel silently drops it), so *every* receive on this
 * connection has to go through recvmsg() with a real control-message
 * buffer attached, not just the ones a caller happens to expect an fd
 * on. wl_keyboard::keymap (the only event this backend's own protocol
 * subset ever receives an fd for) sends its fd alongside the very first
 * bytes of that message, i.e. together with the 8-byte header -- but
 * this function does not special-case that; it is correct for a
 * compositor to have split those bytes across sendmsg() calls
 * differently, so both this function's callers (the header read and the
 * body read in wl_recv_message()) are equally prepared to receive it. */
static int wl_read_exact(int fd, void* buf, size_t len, uint32_t timeout_ms, int* out_fd) {
  unsigned char* p = (unsigned char*)buf;
  size_t got = 0;

  while (got < len) {
    struct pollfd pfd;
    int pr;
    struct msghdr msg;
    struct iovec iov;
    char cmsgbuf[CMSG_SPACE(sizeof(int))];
    ssize_t n;

    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    pr = poll(&pfd, 1, (int)timeout_ms);
    if (pr < 0) {
      if (errno == EINTR) {
        continue;
      }
      return CRTGFX_ERROR_HOST;
    }
    if (pr == 0) {
      return CRTGFX_ERROR_HOST;
    }

    iov.iov_base = p + got;
    iov.iov_len = len - got;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    n = recvmsg(fd, &msg, 0);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return CRTGFX_ERROR_HOST;
    }
    if (n == 0) {
      return CRTGFX_ERROR_HOST;
    }
    if (out_fd != 0) {
      struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);

      if (cmsg != 0 && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        memcpy(out_fd, CMSG_DATA(cmsg), sizeof(int));
      }
    }
    got += (size_t)n;
  }
  return CRTGFX_OK;
}

/* out_fd (may be NULL): set to -1 before either underlying wl_read_exact()
 * call, then left as whichever of the two (if either) actually received
 * an SCM_RIGHTS fd -- see wl_read_exact()'s own comment for why both the
 * header and body reads have to be prepared to receive it. A caller that
 * passes NULL here (every current call site except the ones expecting a
 * wl_keyboard::keymap event) still receives messages correctly; it just
 * cannot learn about an fd that arrived, which would otherwise leak the
 * fd (never closed) -- WL_KEYBOARD_EVENT_KEYMAP is the only event this
 * backend's own protocol subset ever sends an fd with, so every other
 * caller passing NULL is safe by construction, not by luck. */
static int wl_recv_message(
    struct crtgfx_wl_connection* conn, uint32_t* out_object_id, uint32_t* out_opcode,
    unsigned char* body, size_t body_cap, size_t* out_body_len, uint32_t timeout_ms, int* out_fd) {
  uint32_t header[2];
  uint32_t opcode;
  uint32_t size;
  size_t body_len;
  int rc;

  if (out_fd != 0) {
    *out_fd = -1;
  }
  rc = wl_read_exact(conn->fd, header, sizeof(header), timeout_ms, out_fd);
  if (rc != CRTGFX_OK) {
    return rc;
  }
  opcode = header[1] & 0xffffu;
  size = (header[1] >> 16) & 0xffffu;
  if (size < 8u) {
    return CRTGFX_ERROR_HOST;
  }
  body_len = (size_t)(size - 8u);
  if (body_len > body_cap) {
    return CRTGFX_ERROR_HOST;
  }
  if (body_len != 0u) {
    rc = wl_read_exact(conn->fd, body, body_len, timeout_ms, out_fd);
    if (rc != CRTGFX_OK) {
      return rc;
    }
  }
  *out_object_id = header[0];
  *out_opcode = opcode;
  *out_body_len = body_len;
  return CRTGFX_OK;
}

/* Feeds a real wl_keyboard::key event through this backend's own
 * xkbcommon state and queues the resulting crtgfx_event(s) on whichever
 * window currently holds keyboard focus (conn->keyboard_focus_surface_id,
 * set from real wl_keyboard::enter/leave -- see wl_dispatch_message()):
 * always a KEY_DOWN/KEY_UP, and -- for a press that actually produces
 * text (not Escape/arrows/bare-modifier/...) -- a following TEXT event
 * carrying the real, already-composed UTF-8 xkb_state_key_get_utf8()
 * returns. Does nothing if no keymap has been compiled yet (conn->
 * xkb_state == 0) or no window currently has keyboard focus: a real
 * compositor always sends wl_keyboard::keymap before the first key event
 * on a freshly bound keyboard, and always sends wl_keyboard::enter before
 * the first key event too, but a key press that outraces either on a
 * genuinely broken compositor should not crash on a null state/target. */
static void wl_handle_keyboard_key(struct crtgfx_wl_connection* conn, uint32_t key, uint32_t state) {
  struct crtgfx_host_window* target;
  crtgfx_event event;
  xkb_keycode_t xkb_keycode;

  if (conn->xkb_state == 0) {
    return;
  }
  target = crtgfx_wl_find_window_by_surface(conn, conn->keyboard_focus_surface_id);
  if (target == 0) {
    return;
  }
  memset(&event, 0, sizeof(event));
  event.type = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? CRTGFX_EVENT_KEY_DOWN : CRTGFX_EVENT_KEY_UP;
  event.data.key.keycode = key;
  /* CRTGFX_MOD_* is not populated here -- xkb_state_mod_name_is_active()
   * needs real modifier-name strings (XKB_MOD_NAME_SHIFT/.../CTRL/...)
   * this backend has not wired up yet; left as 0 (no modifiers reported)
   * rather than guessed at, matching this project's own "don't claim a
   * gap is closed until it really is" discipline. A future pass can add
   * this without changing the event shape at all. */
  crtgfx_weston_toplevel_note_event(target->toplevel, &event);

  if (state != WL_KEYBOARD_KEY_STATE_PRESSED) {
    return;
  }
  xkb_keycode = (xkb_keycode_t)(key + CRTGFX_WL_XKB_KEYCODE_OFFSET);
  memset(&event, 0, sizeof(event));
  event.type = CRTGFX_EVENT_TEXT;
  if (xkb_state_key_get_utf8(conn->xkb_state, xkb_keycode, event.data.text.utf8,
                             sizeof(event.data.text.utf8)) > 0) {
    crtgfx_weston_toplevel_note_event(target->toplevel, &event);
  }
}

static void wl_dispatch_message(
    struct crtgfx_wl_connection* conn, struct crtgfx_wl_bootstrap* boot, uint32_t object_id,
    uint32_t opcode, const unsigned char* body, size_t body_len, int fd) {
  (void)body_len;

  if (boot != 0 && object_id == conn->registry_id && opcode == WL_REGISTRY_EVENT_GLOBAL) {
    uint32_t name = wl_get_u32(body, 0);
    char interface[128];

    wl_get_string(body, 4, interface, sizeof(interface));
    if (strcmp(interface, "wl_compositor") == 0) {
      boot->compositor_name = name;
    } else if (strcmp(interface, "wl_shm") == 0) {
      boot->shm_name = name;
    } else if (strcmp(interface, "xdg_wm_base") == 0) {
      boot->wm_base_name = name;
    } else if (strcmp(interface, "wl_seat") == 0) {
      boot->seat_name = name;
      CRTGFX_WL_TRACE("registry: wl_seat name=%u\n", name);
    } else if (strcmp(interface, "wl_output") == 0) {
      /* Bound immediately, not deferred into crtgfx_wl_bootstrap like the
       * singleton globals above -- a real multi-monitor compositor can
       * advertise more than one wl_output, so there is no single "the"
       * name to store and bind later the way compositor_name/shm_name/
       * wm_base_name/seat_name do. Bound at WL_OUTPUT_BIND_VERSION (2),
       * not version 1 like every other global here -- see that macro's
       * own comment for why version 2 specifically is required to ever
       * receive wl_output::scale at all. */
      struct crtgfx_wl_output* output = (struct crtgfx_wl_output*)calloc(1, sizeof(*output));
      if (output != 0) {
        unsigned char out[64];
        size_t o;

        output->id = conn->next_id++;
        output->scale = 1;
        o = wl_put_u32(out, 0, name);
        o = wl_put_string(out, sizeof(out), o, "wl_output");
        o = wl_put_u32(out, o, WL_OUTPUT_BIND_VERSION);
        o = wl_put_u32(out, o, output->id);
        if (wl_send(conn, conn->registry_id, WL_REGISTRY_BIND, out, o, -1) == CRTGFX_OK) {
          output->next = conn->outputs;
          conn->outputs = output;
          CRTGFX_WL_TRACE("registry: wl_output name=%u id=%u\n", name, output->id);
        } else {
          free(output);
        }
      }
    }
    return;
  }
  if (boot != 0 && object_id == conn->sync_callback_id && opcode == WL_CALLBACK_EVENT_DONE) {
    boot->sync_done = 1;
    return;
  }
  if (boot == 0 && opcode == WL_CALLBACK_EVENT_DONE) {
    /* Same disambiguation-by-object-id discipline as wl_buffer::release
     * below (also opcode 0 on its own interface) -- a wl_callback::done
     * for some other window's own frame_callback_id, or for an already-
     * destroyed window (crtgfx_wl_find_window_by_frame_callback() finds
     * nothing once a window is unlinked from conn->windows), is not an
     * error, just nothing to do here. */
    struct crtgfx_host_window* fw = crtgfx_wl_find_window_by_frame_callback(conn, object_id);

    if (fw != 0) {
      crtgfx_event event = {0};

      fw->frame_callback_id = 0;
      event.type = CRTGFX_EVENT_FRAME_COMPLETE;
      crtgfx_weston_toplevel_note_event(fw->toplevel, &event);
      return;
    }
  }
  if (object_id == conn->wm_base_id && opcode == XDG_WM_BASE_EVENT_PING) {
    unsigned char out[4];

    wl_put_u32(out, 0, wl_get_u32(body, 0));
    wl_send(conn, conn->wm_base_id, XDG_WM_BASE_PONG, out, sizeof(out), -1);
    return;
  }
  if (object_id == conn->seat_id || object_id == conn->keyboard_id ||
      object_id == conn->pointer_id) {
    /* Handled in the seat/keyboard/pointer blocks below -- checked first,
     * ahead of the per-window xdg_surface/xdg_toplevel branches, purely
     * so those id comparisons never have to run for what is by far the
     * highest-frequency traffic on this connection (pointer motion). */
  } else {
    struct crtgfx_host_window* w = crtgfx_wl_find_window_by_surface(conn, object_id);

    if (w != 0 && object_id == w->xdg_surface_id && opcode == XDG_SURFACE_EVENT_CONFIGURE) {
      w->configure_serial = wl_get_u32(body, 0);
      w->have_first_configure = 1;
      return;
    }
    if (w != 0 && object_id == w->xdg_toplevel_id && opcode == XDG_TOPLEVEL_EVENT_CONFIGURE) {
      uint32_t width = wl_get_u32(body, 0);
      uint32_t height = wl_get_u32(body, 4);

      if (width != 0u && height != 0u) {
        crtgfx_weston_toplevel_note_size(w->toplevel, width, height);
      }
      return;
    }
    if (w != 0 && object_id == w->xdg_toplevel_id && opcode == XDG_TOPLEVEL_EVENT_CLOSE) {
      crtgfx_weston_toplevel_note_close(w->toplevel);
      return;
    }
    if (w != 0 && object_id == w->surface_id && opcode == WL_SURFACE_EVENT_ENTER) {
      /* output(object,0) -- the only argument on this event. Added
       * 2026-08-30 for CRTGFX_EVENT_DPI_SCALE_CHANGED; see struct
       * crtgfx_host_window::current_output_id's own comment for the
       * "most recent entered wins" simplification. */
      w->current_output_id = wl_get_u32(body, 0);
      crtgfx_wl_note_dpi_scale(w);
      return;
    }
    if (w != 0 && object_id == w->surface_id && opcode == WL_SURFACE_EVENT_LEAVE) {
      uint32_t left_output_id = wl_get_u32(body, 0);

      if (w->current_output_id == left_output_id) {
        w->current_output_id = 0;
      }
      return;
    }
  }
  {
    struct crtgfx_wl_output* output = crtgfx_wl_find_output(conn, object_id);

    if (output != 0 && opcode == WL_OUTPUT_EVENT_SCALE) {
      /* factor(int,0) -- the only argument on this event. May arrive
       * before or after the wl_surface::enter that first associates a
       * window with this output (protocol does not guarantee an order
       * here) -- crtgfx_wl_note_dpi_scale() is called for every window
       * currently on this output so either ordering ends up correct. */
      struct crtgfx_host_window* w2;

      output->scale = (int32_t)wl_get_u32(body, 0);
      CRTGFX_WL_TRACE("output: id=%u scale=%d\n", output->id, output->scale);
      for (w2 = conn->windows; w2 != 0; w2 = w2->next) {
        if (w2->current_output_id == output->id) {
          crtgfx_wl_note_dpi_scale(w2);
        }
      }
      return;
    }
  }
  /* Real, found bug (2026-08-25, real-hardware test): wl_buffer::release
   * and wl_seat::capabilities are BOTH opcode 0 -- every Wayland
   * interface numbers its own events starting from 0 independently, so
   * opcode alone never disambiguates *which* interface's event this is,
   * only object_id does. This branch used to check opcode alone and
   * unconditionally `return`, which meant wl_seat::capabilities (also
   * opcode 0) got silently swallowed here before it ever reached the
   * real handler below -- wl_destroy_released_buffer() itself was
   * harmless on a non-buffer id (its own internal loop just finds
   * nothing and no-ops), but the unconditional early `return` after it
   * discarded the message regardless. Confirmed via this file's own
   * CRTGFX_WAYLAND_DEBUG=1 tracing on real hardware: the "seat:
   * capabilities=..." trace line never printed at all, only "seat: bind
   * requested" -- keyboard input never worked because the keyboard was
   * simply never requested in the first place (WL_SEAT_GET_KEYBOARD
   * only gets sent from inside the capabilities handler). Fixed by only
   * treating the message as consumed when object_id genuinely named a
   * buffer this backend created; otherwise fall through so the real
   * owner of opcode 0 on this object_id gets a chance to handle it. */
  if (opcode == WL_BUFFER_EVENT_RELEASE && wl_destroy_released_buffer(conn, object_id)) {
    return;
  }
  if (conn->seat_id != 0 && object_id == conn->seat_id && opcode == WL_SEAT_EVENT_CAPABILITIES) {
    uint32_t capabilities = wl_get_u32(body, 0);
    unsigned char out[4];

    CRTGFX_WL_TRACE("seat: capabilities=0x%x keyboard_bit=%d pointer_bit=%d\n", capabilities,
                     (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0u,
                     (capabilities & WL_SEAT_CAPABILITY_POINTER) != 0u);
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0u && conn->keyboard_id == 0u) {
      conn->keyboard_id = conn->next_id++;
      wl_put_u32(out, 0, conn->keyboard_id);
      wl_send(conn, conn->seat_id, WL_SEAT_GET_KEYBOARD, out, sizeof(out), -1);
      CRTGFX_WL_TRACE("seat: requesting keyboard id=%u\n", conn->keyboard_id);
    }
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0u && conn->pointer_id == 0u) {
      conn->pointer_id = conn->next_id++;
      wl_put_u32(out, 0, conn->pointer_id);
      wl_send(conn, conn->seat_id, WL_SEAT_GET_POINTER, out, sizeof(out), -1);
      CRTGFX_WL_TRACE("seat: requesting pointer id=%u\n", conn->pointer_id);
    }
    return;
  }
  if (conn->pointer_id != 0 && object_id == conn->pointer_id) {
    if (opcode == WL_POINTER_EVENT_ENTER) {
      /* serial(uint,0), surface(object,4), surface_x(fixed,8),
       * surface_y(fixed,12). Reading `surface` here (not done before
       * multi-window support existed, since there was only ever one
       * possible destination) is what lets pointer motion/button/axis
       * below route to the right window. */
      uint32_t surface_id = wl_get_u32(body, 4);
      int32_t x_fixed = (int32_t)wl_get_u32(body, 8);
      int32_t y_fixed = (int32_t)wl_get_u32(body, 12);

      conn->pointer_focus_surface_id = surface_id;
      conn->pointer_x = (double)x_fixed / 256.0;
      conn->pointer_y = (double)y_fixed / 256.0;
      CRTGFX_WL_TRACE("pointer: enter surface=%u x=%.2f y=%.2f\n", surface_id, conn->pointer_x,
                       conn->pointer_y);
      return;
    }
    if (opcode == WL_POINTER_EVENT_LEAVE) {
      /* serial(uint,0), surface(object,4). */
      uint32_t surface_id = wl_get_u32(body, 4);

      if (conn->pointer_focus_surface_id == surface_id) {
        conn->pointer_focus_surface_id = 0;
      }
      CRTGFX_WL_TRACE("pointer: leave surface=%u\n", surface_id);
      return;
    }
    if (opcode == WL_POINTER_EVENT_MOTION) {
      /* time(uint,0), surface_x(fixed,4), surface_y(fixed,8). */
      int32_t x_fixed = (int32_t)wl_get_u32(body, 4);
      int32_t y_fixed = (int32_t)wl_get_u32(body, 8);
      struct crtgfx_host_window* target;

      conn->pointer_x = (double)x_fixed / 256.0;
      conn->pointer_y = (double)y_fixed / 256.0;
      target = crtgfx_wl_find_window_by_surface(conn, conn->pointer_focus_surface_id);
      if (target != 0) {
        crtgfx_event event = {0};
        event.type = CRTGFX_EVENT_POINTER_MOTION;
        event.data.pointer_motion.x = conn->pointer_x;
        event.data.pointer_motion.y = conn->pointer_y;
        crtgfx_weston_toplevel_note_event(target->toplevel, &event);
      }
      return;
    }
    if (opcode == WL_POINTER_EVENT_BUTTON) {
      /* serial(uint,0), time(uint,4), button(uint,8), state(uint,12). */
      uint32_t button = wl_get_u32(body, 8);
      uint32_t state = wl_get_u32(body, 12);
      uint32_t crtgfx_button;
      struct crtgfx_host_window* target;

      if (button == CRTGFX_BTN_LEFT) {
        crtgfx_button = CRTGFX_POINTER_BUTTON_LEFT;
      } else if (button == CRTGFX_BTN_RIGHT) {
        crtgfx_button = CRTGFX_POINTER_BUTTON_RIGHT;
      } else if (button == CRTGFX_BTN_MIDDLE) {
        crtgfx_button = CRTGFX_POINTER_BUTTON_MIDDLE;
      } else {
        CRTGFX_WL_TRACE("pointer: unmapped button=0x%x state=%u ignored\n", button, state);
        return; /* a real but unmapped button (side/extra buttons, ...) -- not queued */
      }
      target = crtgfx_wl_find_window_by_surface(conn, conn->pointer_focus_surface_id);
      if (target != 0) {
        crtgfx_event event = {0};
        event.type = (state == WL_POINTER_BUTTON_STATE_PRESSED) ? CRTGFX_EVENT_POINTER_BUTTON_DOWN
                                                                  : CRTGFX_EVENT_POINTER_BUTTON_UP;
        event.data.pointer_button.button = crtgfx_button;
        event.data.pointer_button.x = conn->pointer_x;
        event.data.pointer_button.y = conn->pointer_y;
        crtgfx_weston_toplevel_note_event(target->toplevel, &event);
      }
      return;
    }
    if (opcode == WL_POINTER_EVENT_AXIS) {
      /* time(uint,0), axis(uint,4), value(fixed,8). Sign/scale taken
       * directly from the wire value -- see crtgfx/window.h's own
       * CRTGFX_EVENT_POINTER_SCROLL doc comment for why this is flagged
       * reasoned-but-not-physically-verified this session (no real
       * scroll wheel reachable from WSL/WSLg). */
      uint32_t axis = wl_get_u32(body, 4);
      int32_t value_fixed = (int32_t)wl_get_u32(body, 8);
      double value = (double)value_fixed / 256.0;
      struct crtgfx_host_window* target = crtgfx_wl_find_window_by_surface(
          conn, conn->pointer_focus_surface_id);

      if (target != 0 && (axis == WL_POINTER_AXIS_VERTICAL_SCROLL ||
                           axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)) {
        crtgfx_event event = {0};
        event.type = CRTGFX_EVENT_POINTER_SCROLL;
        if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
          event.data.pointer_scroll.dy = value;
        } else {
          event.data.pointer_scroll.dx = value;
        }
        crtgfx_weston_toplevel_note_event(target->toplevel, &event);
      }
      return;
    }
    /* wl_pointer::frame/...: intentionally not acted on (this backend
     * only ever binds wl_pointer at version 1, so frame -- a v5 addition
     * -- is never actually sent; kept as a traced catch-all in case a
     * future version bump changes that). */
    CRTGFX_WL_TRACE("pointer: event opcode=%u (0=enter 1=leave 2=motion 3=button 4=axis)\n", opcode);
    return;
  }
  if (conn->keyboard_id != 0 && object_id == conn->keyboard_id) {
    if (opcode == WL_KEYBOARD_EVENT_KEYMAP) {
      uint32_t format = wl_get_u32(body, 0);
      uint32_t size = wl_get_u32(body, 4);
      void* mapping;

      CRTGFX_WL_TRACE("keyboard: keymap event format=%u size=%u fd=%d\n", format, size, fd);
      if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || fd < 0) {
        CRTGFX_WL_TRACE("keyboard: keymap rejected (format=%u fd=%d)\n", format, fd);
        if (fd >= 0) {
          close(fd);
        }
        return;
      }
      /* MAP_SHARED, matching real upstream libxkbcommon's own tools/
       * interactive-wayland.c example client exactly (confirmed by
       * reading it directly) -- MAP_PRIVATE only becomes a real
       * *requirement* from wl_keyboard version 7 onwards (wayland.xml's
       * own doc comment), and this backend only ever binds wl_seat/
       * wl_keyboard at version 1 (see crtgfx_wl_connection_create()). */
      mapping = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
      close(fd);
      if (mapping == MAP_FAILED) {
        return;
      }
      if (conn->xkb_state != 0) {
        xkb_state_unref(conn->xkb_state);
        conn->xkb_state = 0;
      }
      if (conn->xkb_keymap != 0) {
        xkb_keymap_unref(conn->xkb_keymap);
        conn->xkb_keymap = 0;
      }
      /* size - 1: the mapped keymap string is NUL-terminated and `size`
       * (the wire value) includes that trailing NUL -- matching real
       * upstream libxkbcommon's own example exactly (confirmed by
       * reading it directly, not guessed: passing the NUL byte itself
       * into xkb_keymap_new_from_buffer()'s own length argument is not
       * what any real client does). */
      if (size > 0u) {
        conn->xkb_keymap = xkb_keymap_new_from_buffer(
            conn->xkb_context, (const char*)mapping, (size_t)(size - 1u),
            XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
      }
      munmap(mapping, size);
      if (conn->xkb_keymap != 0) {
        conn->xkb_state = xkb_state_new(conn->xkb_keymap);
      }
      CRTGFX_WL_TRACE("keyboard: keymap compiled=%d state=%d\n", conn->xkb_keymap != 0,
                       conn->xkb_state != 0);
      return;
    }
    if (opcode == WL_KEYBOARD_EVENT_ENTER) {
      /* serial(uint,0), surface(object,4), keys(array,8) -- the array's
       * own contents (currently pressed keys) are not needed here, only
       * which surface gained focus. Fires CRTGFX_EVENT_FOCUS_IN on that
       * window via the shared crtgfx_weston_toplevel_note_focus() (see
       * wayland_weston.c), the same function Windows/macOS will call
       * from their own native focus signal. */
      uint32_t surface_id = wl_get_u32(body, 4);
      struct crtgfx_host_window* target = crtgfx_wl_find_window_by_surface(conn, surface_id);

      conn->keyboard_focus_surface_id = surface_id;
      CRTGFX_WL_TRACE("keyboard: enter surface=%u\n", surface_id);
      if (target != 0) {
        crtgfx_weston_toplevel_note_focus(target->toplevel, 1);
      }
      return;
    }
    if (opcode == WL_KEYBOARD_EVENT_LEAVE) {
      /* serial(uint,0), surface(object,4). */
      uint32_t surface_id = wl_get_u32(body, 4);
      struct crtgfx_host_window* target = crtgfx_wl_find_window_by_surface(conn, surface_id);

      if (conn->keyboard_focus_surface_id == surface_id) {
        conn->keyboard_focus_surface_id = 0;
      }
      CRTGFX_WL_TRACE("keyboard: leave surface=%u\n", surface_id);
      if (target != 0) {
        crtgfx_weston_toplevel_note_focus(target->toplevel, 0);
      }
      return;
    }
    if (opcode == WL_KEYBOARD_EVENT_KEY) {
      uint32_t key = wl_get_u32(body, 8);
      uint32_t state = wl_get_u32(body, 12);

      CRTGFX_WL_TRACE("keyboard: key=%u state=%u (xkb_state=%d)\n", key, state,
                       conn->xkb_state != 0);
      wl_handle_keyboard_key(conn, key, state);
      return;
    }
    if (opcode == WL_KEYBOARD_EVENT_MODIFIERS) {
      uint32_t mods_depressed = wl_get_u32(body, 4);
      uint32_t mods_latched = wl_get_u32(body, 8);
      uint32_t mods_locked = wl_get_u32(body, 12);
      uint32_t group = wl_get_u32(body, 16);

      CRTGFX_WL_TRACE("keyboard: modifiers depressed=%u latched=%u locked=%u group=%u\n",
                       mods_depressed, mods_latched, mods_locked, group);

      /* xkb_state_update_mask(), not xkb_state_update_key(): a real
       * Wayland *client* (this backend) is required to use the mask-
       * based update -- confirmed by reading xkb_state_update_key()'s own
       * doc comment directly ("intended for *server* applications and
       * should not be used by *client* applications"). depressed_layout/
       * latched_layout=0, locked_layout=group: matching real upstream
       * libxkbcommon's own tools/interactive-wayland.c kbd_modifiers()
       * exactly (confirmed by reading it directly, not guessed). */
      xkb_state_update_mask(conn->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
      return;
    }
    /* wl_keyboard::repeat_info: intentionally not acted on -- see crtgfx/
     * window.h's own key-repeat policy comment (pass through the host's
     * own repeat, never a project-owned timer). */
    CRTGFX_WL_TRACE("keyboard: event opcode=%u (5=repeat_info)\n", opcode);
    return;
  }
  /* Anything else (wl_display::error/delete_id, wl_surface::enter/leave,
   * ...) is intentionally ignored. The message body has already been fully
   * consumed by wl_recv_message() either way, so ignoring it here is always
   * safe (never leaves the stream mis-aligned). Closing a stray fd here
   * too: WL_KEYBOARD_EVENT_KEYMAP is the only event this backend's own
   * protocol subset ever attaches one to, and it is always fully consumed
   * (closed) by that branch above -- this is a defensive backstop against
   * an fd leak, not a path any real compositor this backend talks to is
   * expected to exercise. */
  if (fd >= 0) {
    close(fd);
  }
}

static int wl_pump(struct crtgfx_wl_connection* conn, uint32_t timeout_ms) {
  struct pollfd pfd;
  int pr;

  if (conn == 0 || conn->fd < 0) {
    return CRTGFX_OK;
  }
  pfd.fd = conn->fd;
  pfd.events = POLLIN;
  pfd.revents = 0;
  pr = poll(&pfd, 1, (int)timeout_ms);
  if (pr <= 0) {
    return CRTGFX_OK;
  }
  for (;;) {
    uint32_t object_id;
    uint32_t opcode;
    unsigned char body[CRTGFX_WL_MAX_MSG];
    size_t body_len;
    int recv_fd;

    if (wl_recv_message(conn, &object_id, &opcode, body, sizeof(body), &body_len, 0, &recv_fd) != CRTGFX_OK) {
      break;
    }
    wl_dispatch_message(conn, 0, object_id, opcode, body, body_len, recv_fd);

    pfd.revents = 0;
    if (poll(&pfd, 1, 0) <= 0) {
      break;
    }
  }
  return CRTGFX_OK;
}

static int wl_connect(struct crtgfx_wl_connection* conn) {
  const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
  const char* display = getenv("WAYLAND_DISPLAY");
  struct sockaddr_un addr;
  int fd;

  if (runtime_dir == 0 || runtime_dir[0] == 0) {
    return CRTGFX_ERROR_UNSUPPORTED;
  }
  if (display == 0 || display[0] == 0) {
    display = "wayland-0";
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (display[0] == '/') {
    if (strlen(display) >= sizeof(addr.sun_path)) {
      return CRTGFX_ERROR_UNSUPPORTED;
    }
    strcpy(addr.sun_path, display);
  } else {
    if (strlen(runtime_dir) + 1u + strlen(display) >= sizeof(addr.sun_path)) {
      return CRTGFX_ERROR_UNSUPPORTED;
    }
    strcpy(addr.sun_path, runtime_dir);
    strcat(addr.sun_path, "/");
    strcat(addr.sun_path, display);
  }

  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return CRTGFX_ERROR_UNSUPPORTED;
  }
  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    close(fd);
    return CRTGFX_ERROR_UNSUPPORTED;
  }
  conn->fd = fd;
  return CRTGFX_OK;
}

static void crtgfx_wl_connection_destroy(struct crtgfx_wl_connection* conn) {
  struct crtgfx_wl_output* output;

  if (conn == 0) {
    return;
  }
  output = conn->outputs;
  while (output != 0) {
    struct crtgfx_wl_output* next = output->next;
    free(output);
    output = next;
  }
  if (conn->xkb_state != 0) {
    xkb_state_unref(conn->xkb_state);
  }
  if (conn->xkb_keymap != 0) {
    xkb_keymap_unref(conn->xkb_keymap);
  }
  if (conn->xkb_context != 0) {
    xkb_context_unref(conn->xkb_context);
  }
  if (conn->fd >= 0) {
    close(conn->fd);
  }
  free(conn);
}

/* Connects and binds every connection-wide global this backend needs
 * (wl_compositor/wl_shm/xdg_wm_base, plus the optional wl_seat) -- called
 * once, by the first crtgfx_host_window_create() in the process. Every
 * later call reuses the resulting connection instead of calling this
 * again (see crtgfx_host_window_create() below). */
static int crtgfx_wl_connection_create(struct crtgfx_wl_connection** out_conn) {
  struct crtgfx_wl_connection* conn;
  struct crtgfx_wl_bootstrap boot;
  unsigned char body[512];
  size_t off;
  int rc;

  conn = (struct crtgfx_wl_connection*)calloc(1, sizeof(*conn));
  if (conn == 0) {
    return CRTGFX_ERROR_HOST;
  }
  conn->fd = -1;
  conn->next_id = 2;

  /* XKB_CONTEXT_NO_FLAGS: the real, documented "just give me a normal
   * context" value (0) -- created once, up front, and kept for this
   * connection's whole lifetime, independent of whether a keymap ever
   * actually arrives (a compositor with no wl_seat, or a seat with no
   * keyboard capability, just means this context never gets used). */
  conn->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (conn->xkb_context == 0) {
    free(conn);
    return CRTGFX_ERROR_HOST;
  }

  rc = wl_connect(conn);
  if (rc != CRTGFX_OK) {
    crtgfx_wl_connection_destroy(conn);
    return rc;
  }

  memset(&boot, 0, sizeof(boot));
  boot.compositor_name = CRTGFX_WL_NAME_NONE;
  boot.shm_name = CRTGFX_WL_NAME_NONE;
  boot.wm_base_name = CRTGFX_WL_NAME_NONE;
  boot.seat_name = CRTGFX_WL_NAME_NONE;

  conn->registry_id = conn->next_id++;
  off = wl_put_u32(body, 0, conn->registry_id);
  if (wl_send(conn, 1, WL_DISPLAY_GET_REGISTRY, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  conn->sync_callback_id = conn->next_id++;
  off = wl_put_u32(body, 0, conn->sync_callback_id);
  if (wl_send(conn, 1, WL_DISPLAY_SYNC, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  while (!boot.sync_done) {
    uint32_t object_id;
    uint32_t opcode;
    unsigned char rbody[CRTGFX_WL_MAX_MSG];
    size_t rlen;
    int recv_fd;

    rc = wl_recv_message(conn, &object_id, &opcode, rbody, sizeof(rbody), &rlen, CRTGFX_WL_TIMEOUT_MS, &recv_fd);
    if (rc != CRTGFX_OK) {
      goto fail;
    }
    wl_dispatch_message(conn, &boot, object_id, opcode, rbody, rlen, recv_fd);
  }

  if (boot.compositor_name == CRTGFX_WL_NAME_NONE || boot.shm_name == CRTGFX_WL_NAME_NONE ||
      boot.wm_base_name == CRTGFX_WL_NAME_NONE) {
    /* A real Wayland connection, but the compositor doesn't advertise a
     * global this backend needs -- treat as unsupported, not a hard
     * error: no known real compositor lacks these (wl_compositor/wl_shm
     * are core-protocol-mandatory, xdg_wm_base is the universal modern
     * shell protocol), but a future/unusual one legitimately might. */
    rc = CRTGFX_ERROR_UNSUPPORTED;
    goto fail;
  }

  conn->compositor_id = conn->next_id++;
  off = wl_put_u32(body, 0, boot.compositor_name);
  off = wl_put_string(body, sizeof(body), off, "wl_compositor");
  off = wl_put_u32(body, off, 1);
  off = wl_put_u32(body, off, conn->compositor_id);
  if (wl_send(conn, conn->registry_id, WL_REGISTRY_BIND, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  conn->shm_id = conn->next_id++;
  off = wl_put_u32(body, 0, boot.shm_name);
  off = wl_put_string(body, sizeof(body), off, "wl_shm");
  off = wl_put_u32(body, off, 1);
  off = wl_put_u32(body, off, conn->shm_id);
  if (wl_send(conn, conn->registry_id, WL_REGISTRY_BIND, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  conn->wm_base_id = conn->next_id++;
  off = wl_put_u32(body, 0, boot.wm_base_name);
  off = wl_put_string(body, sizeof(body), off, "xdg_wm_base");
  off = wl_put_u32(body, off, 1);
  off = wl_put_u32(body, off, conn->wm_base_id);
  if (wl_send(conn, conn->registry_id, WL_REGISTRY_BIND, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  /* wl_seat, unlike compositor/shm/wm_base just above, is optional: a
   * compositor with no seat at all just means no keyboard/pointer events
   * are ever queued, not a connection failure (see this file's own
   * top-of-file scope note). Its own wl_seat::capabilities event (sent
   * "on binding to the seat global", per wayland.xml's own doc) -- and,
   * once that arrives with the keyboard/pointer bit set, the events that
   * follow -- get processed by wl_dispatch_message() exactly like any
   * other event, whichever loop below happens to read them first; no
   * extra explicit roundtrip is added here for it. */
  if (boot.seat_name != CRTGFX_WL_NAME_NONE) {
    conn->seat_id = conn->next_id++;
    off = wl_put_u32(body, 0, boot.seat_name);
    off = wl_put_string(body, sizeof(body), off, "wl_seat");
    off = wl_put_u32(body, off, 1);
    off = wl_put_u32(body, off, conn->seat_id);
    if (wl_send(conn, conn->registry_id, WL_REGISTRY_BIND, body, off, -1) != CRTGFX_OK) {
      rc = CRTGFX_ERROR_HOST;
      goto fail;
    }
    CRTGFX_WL_TRACE("seat: bind requested name=%u id=%u\n", boot.seat_name, conn->seat_id);
  } else {
    CRTGFX_WL_TRACE("seat: no wl_seat global advertised by this compositor\n");
  }

  *out_conn = conn;
  return CRTGFX_OK;

fail:
  crtgfx_wl_connection_destroy(conn);
  return (rc == CRTGFX_ERROR_UNSUPPORTED) ? CRTGFX_ERROR_UNSUPPORTED : CRTGFX_ERROR_HOST;
}

/* Creates this window's own wl_surface/xdg_surface/xdg_toplevel on an
 * already-bound `conn` (shared or freshly created -- the caller does not
 * distinguish) and waits for the first real xdg_surface::configure, the
 * same per-window bring-up crtgfx_host_window_create() always did, just
 * factored out so it can run against a connection that may already have
 * other windows on it. */
static int crtgfx_wl_window_attach(
    struct crtgfx_wl_connection* conn, const crtgfx_window_desc* desc,
    crtgfx_weston_toplevel* toplevel, struct crtgfx_host_window** out_host) {
  struct crtgfx_host_window* host;
  unsigned char body[512];
  size_t off;
  int rc;

  host = (struct crtgfx_host_window*)calloc(1, sizeof(*host));
  if (host == 0) {
    return CRTGFX_ERROR_HOST;
  }
  host->conn = conn;
  host->toplevel = toplevel;

  host->surface_id = conn->next_id++;
  off = wl_put_u32(body, 0, host->surface_id);
  if (wl_send(conn, conn->compositor_id, WL_COMPOSITOR_CREATE_SURFACE, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  host->xdg_surface_id = conn->next_id++;
  off = wl_put_u32(body, 0, host->xdg_surface_id);
  off = wl_put_u32(body, off, host->surface_id);
  if (wl_send(conn, conn->wm_base_id, XDG_WM_BASE_GET_XDG_SURFACE, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  host->xdg_toplevel_id = conn->next_id++;
  off = wl_put_u32(body, 0, host->xdg_toplevel_id);
  if (wl_send(conn, host->xdg_surface_id, XDG_SURFACE_GET_TOPLEVEL, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  off = wl_put_string(body, sizeof(body), 0, desc->title != 0 ? desc->title : "crtgfx");
  if (off == (size_t)-1 ||
      wl_send(conn, host->xdg_toplevel_id, XDG_TOPLEVEL_SET_TITLE, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  /* Initial "null commit" (no buffer attached yet) -- required by
   * xdg-shell to trigger the first configure sequence below. */
  if (wl_send(conn, host->surface_id, WL_SURFACE_COMMIT, 0, 0, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  /* Temporarily linked into conn->windows *before* the first configure
   * arrives, purely so wl_dispatch_message()'s object-id routing (which
   * walks conn->windows) can find this window while waiting for it below
   * -- unlinked again on failure, left linked by the caller (crtgfx_host_
   * window_create()) on success. Sharing a connection with sibling
   * windows means their own traffic can legitimately interleave with
   * this wait, unlike the old one-connection-per-window design where
   * every message read here was guaranteed to be about this window. */
  host->next = conn->windows;
  conn->windows = host;

  while (!host->have_first_configure) {
    uint32_t object_id;
    uint32_t opcode;
    unsigned char rbody[CRTGFX_WL_MAX_MSG];
    size_t rlen;
    int recv_fd;

    rc = wl_recv_message(conn, &object_id, &opcode, rbody, sizeof(rbody), &rlen, CRTGFX_WL_TIMEOUT_MS, &recv_fd);
    if (rc != CRTGFX_OK) {
      conn->windows = host->next;
      goto fail;
    }
    wl_dispatch_message(conn, 0, object_id, opcode, rbody, rlen, recv_fd);
  }

  off = wl_put_u32(body, 0, host->configure_serial);
  if (wl_send(conn, host->xdg_surface_id, XDG_SURFACE_ACK_CONFIGURE, body, off, -1) != CRTGFX_OK) {
    conn->windows = host->next;
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  *out_host = host;
  return CRTGFX_OK;

fail:
  free(host);
  return rc;
}

int crtgfx_host_window_create(const crtgfx_window_desc* desc, crtgfx_weston_toplevel* toplevel) {
  struct crtgfx_host_window* host;
  int created_connection = 0;
  int rc;

  if (desc == 0 || toplevel == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }

  if (crtgfx_wl_conn == 0) {
    rc = crtgfx_wl_connection_create(&crtgfx_wl_conn);
    if (rc != CRTGFX_OK) {
      return rc;
    }
    created_connection = 1;
  }

  rc = crtgfx_wl_window_attach(crtgfx_wl_conn, desc, toplevel, &host);
  if (rc != CRTGFX_OK) {
    if (created_connection) {
      crtgfx_wl_connection_destroy(crtgfx_wl_conn);
      crtgfx_wl_conn = 0;
    }
    return rc;
  }

  toplevel->host = host;
  return CRTGFX_OK;
}

void crtgfx_host_window_destroy(crtgfx_host_window* host) {
  struct crtgfx_wl_connection* conn;
  struct crtgfx_host_window** link;

  if (host == 0) {
    return;
  }
  conn = host->conn;

  if (conn->pointer_focus_surface_id == host->surface_id) {
    conn->pointer_focus_surface_id = 0;
  }
  if (conn->keyboard_focus_surface_id == host->surface_id) {
    conn->keyboard_focus_surface_id = 0;
  }

  wl_destroy_all_buffers(host);

  /* Explicit per-window object teardown -- required now that the
   * connection is shared: closing conn->fd (which used to destroy every
   * server-side object implicitly, back when each window had its own
   * connection) is now only correct once every window sharing it is
   * gone. A window being destroyed while sibling windows remain has to
   * ask the compositor to destroy just its own three objects instead;
   * best-effort (return values ignored) since there is nothing useful to
   * do about a failed destroy request on an object already being torn
   * down locally either way. */
  if (host->xdg_toplevel_id != 0) {
    (void)wl_send(conn, host->xdg_toplevel_id, XDG_TOPLEVEL_DESTROY, 0, 0, -1);
  }
  if (host->xdg_surface_id != 0) {
    (void)wl_send(conn, host->xdg_surface_id, XDG_SURFACE_DESTROY, 0, 0, -1);
  }
  if (host->surface_id != 0) {
    (void)wl_send(conn, host->surface_id, WL_SURFACE_DESTROY, 0, 0, -1);
  }

  link = &conn->windows;
  while (*link != 0) {
    if (*link == host) {
      *link = host->next;
      break;
    }
    link = &(*link)->next;
  }
  free(host);

  if (conn->windows == 0) {
    crtgfx_wl_connection_destroy(conn);
    crtgfx_wl_conn = 0;
  }
}

int crtgfx_host_window_show(crtgfx_host_window* host) {
  if (host == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  /* Wayland has no separate "show" request: a toplevel becomes visible/
   * mapped once a real buffer is attached and committed, which
   * crtgfx_host_window_present_software() already does on the first
   * end_frame() call. Nothing to do here. */
  return CRTGFX_OK;
}

int crtgfx_host_window_dispatch(uint32_t timeout_ms) {
  return wl_pump(crtgfx_wl_conn, timeout_ms);
}

int crtgfx_host_window_get_size(crtgfx_host_window* host, uint32_t* out_width, uint32_t* out_height) {
  if (host == 0 || out_width == 0 || out_height == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  *out_width = host->toplevel->width;
  *out_height = host->toplevel->height;
  return CRTGFX_OK;
}

int crtgfx_host_window_present_software(
    crtgfx_host_window* host, const void* pixels, uint32_t width, uint32_t height, uint32_t stride,
    const crtgfx_damage_rect* damage_rects, uint32_t damage_rect_count) {
  struct crtgfx_wl_connection* conn;
  int memfd;
  void* mapping;
  struct crtgfx_wl_buffer* submitted;
  uint32_t size;
  uint32_t pool_id;
  uint32_t buffer_id;
  unsigned char body[32];
  size_t off;

  if (host == 0 || pixels == 0 || width == 0 || height == 0 || stride < width * 4u) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  if (stride > UINT32_MAX / height) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  conn = host->conn;
  size = stride * height;
  (void)wl_pump(conn, 0);

  memfd = memfd_create("crtgfx-shm", 0);
  if (memfd < 0) {
    return CRTGFX_ERROR_HOST;
  }
  if (ftruncate(memfd, (off_t)size) != 0) {
    close(memfd);
    return CRTGFX_ERROR_HOST;
  }
  mapping = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);
  if (mapping == MAP_FAILED) {
    close(memfd);
    return CRTGFX_ERROR_HOST;
  }
  memcpy(mapping, pixels, size);

  pool_id = conn->next_id++;
  off = wl_put_u32(body, 0, pool_id);
  off = wl_put_u32(body, off, (uint32_t)size);
  if (wl_send(conn, conn->shm_id, WL_SHM_CREATE_POOL, body, off, memfd) != CRTGFX_OK) {
    munmap(mapping, size);
    close(memfd);
    return CRTGFX_ERROR_HOST;
  }

  buffer_id = conn->next_id++;
  off = wl_put_u32(body, 0, buffer_id);
  off = wl_put_u32(body, off, 0); /* offset */
  off = wl_put_u32(body, off, width);
  off = wl_put_u32(body, off, height);
  off = wl_put_u32(body, off, stride);
  off = wl_put_u32(body, off, CRTGFX_WL_FORMAT_ARGB8888);
  if (wl_send(conn, pool_id, WL_SHM_POOL_CREATE_BUFFER, body, off, -1) != CRTGFX_OK) {
    munmap(mapping, size);
    close(memfd);
    return CRTGFX_ERROR_HOST;
  }
  submitted = (struct crtgfx_wl_buffer*)calloc(1, sizeof(*submitted));
  if (submitted == 0) {
    wl_send(conn, buffer_id, WL_BUFFER_DESTROY, 0, 0, -1);
    munmap(mapping, size);
    close(memfd);
    return CRTGFX_ERROR_HOST;
  }
  /* A buffer created through a pool stays valid after the pool itself is
   * destroyed (real protocol guarantee) -- destroy it right away so the
   * object id table doesn't grow across repeated presents. */
  wl_send(conn, pool_id, WL_SHM_POOL_DESTROY, 0, 0, -1);

  off = wl_put_u32(body, 0, buffer_id);
  off = wl_put_u32(body, off, 0);
  off = wl_put_u32(body, off, 0);
  if (wl_send(conn, host->surface_id, WL_SURFACE_ATTACH, body, off, -1) != CRTGFX_OK) {
    free(submitted);
    munmap(mapping, size);
    close(memfd);
    return CRTGFX_ERROR_HOST;
  }

  /* damage_rects/damage_rect_count null/0 means "the whole frame changed"
   * (crtgfx/window.h's own crtgfx_window_end_frame_damaged() contract) --
   * unchanged from this function's own previous, only behavior: one
   * wl_surface::damage covering the whole surface. Otherwise a real,
   * separate wl_surface::damage request per caller-supplied rect -- the
   * protocol allows any number of damage requests before the one commit
   * below, the compositor unions them itself, so this is a genuine
   * partial-present optimization, not an approximation of one. Added
   * 2026-08-30. */
  if (damage_rects == 0 || damage_rect_count == 0u) {
    off = wl_put_u32(body, 0, 0);
    off = wl_put_u32(body, off, 0);
    off = wl_put_u32(body, off, width);
    off = wl_put_u32(body, off, height);
    if (wl_send(conn, host->surface_id, WL_SURFACE_DAMAGE, body, off, -1) != CRTGFX_OK) {
      free(submitted);
      munmap(mapping, size);
      close(memfd);
      return CRTGFX_ERROR_HOST;
    }
  } else {
    uint32_t i;

    for (i = 0; i < damage_rect_count; i += 1u) {
      const crtgfx_damage_rect* rect = &damage_rects[i];

      if (rect->width == 0u || rect->height == 0u) {
        continue; /* a real but degenerate rect -- no-op, not a protocol error */
      }
      off = wl_put_u32(body, 0, rect->x);
      off = wl_put_u32(body, off, rect->y);
      off = wl_put_u32(body, off, rect->width);
      off = wl_put_u32(body, off, rect->height);
      if (wl_send(conn, host->surface_id, WL_SURFACE_DAMAGE, body, off, -1) != CRTGFX_OK) {
        free(submitted);
        munmap(mapping, size);
        close(memfd);
        return CRTGFX_ERROR_HOST;
      }
    }
  }

  /* Request this present's own wl_callback::done notification (see
   * WL_SURFACE_FRAME's own comment) before the commit below, the same
   * ordering real upstream clients use -- only if no previous one is
   * still outstanding (see crtgfx_host_window::frame_callback_id's own
   * comment for why at most one is ever requested at a time). Best-effort:
   * if this request fails to send, CRTGFX_EVENT_FRAME_COMPLETE simply
   * never fires for this present, which is a real, honest degradation
   * (the caller's own event queue -- not the actual present, already
   * committed by this point) rather than treated as a fatal error here. */
  if (host->frame_callback_id == 0u) {
    uint32_t new_callback_id = conn->next_id++;

    off = wl_put_u32(body, 0, new_callback_id);
    if (wl_send(conn, host->surface_id, WL_SURFACE_FRAME, body, off, -1) == CRTGFX_OK) {
      host->frame_callback_id = new_callback_id;
    }
  }

  if (wl_send(conn, host->surface_id, WL_SURFACE_COMMIT, 0, 0, -1) != CRTGFX_OK) {
    free(submitted);
    munmap(mapping, size);
    close(memfd);
    return CRTGFX_ERROR_HOST;
  }

  submitted->id = buffer_id;
  submitted->fd = memfd;
  submitted->data = mapping;
  submitted->size = size;
  submitted->next = host->buffers;
  host->buffers = submitted;
  return CRTGFX_OK;
}
