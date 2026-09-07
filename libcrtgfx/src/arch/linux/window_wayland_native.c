#include "window_wayland_native.h"

#include "wayland_weston_internal.h"

#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <xkbcommon/xkbcommon.h>

/* Native Wayland backend for Linux (2026-09-07, "Finish live GPU
 * presentation everywhere") -- real libwayland-client + real xdg-shell
 * bindings (libcrtgfx/third_party/wayland/recipe.json, built by tools/
 * build_wayland.py; the xdg-shell protocol XML itself comes from a real,
 * separately pinned wayland-protocols sparse checkout, added this same
 * session), unlike the legacy backend (window_wayland.c), which
 * deliberately hand-rolls the wire protocol and has never linked real
 * libwayland-client at all. See window_wayland_native.h's own top comment
 * for the full dual-backend design and why a window cannot move between
 * the two after creation, and docs/libcrtgfx_wayland_plan.md for the
 * user's own 4-phase roadmap this is Phase 2 of.
 *
 * Scope of this vertical slice (deliberately narrower than the legacy
 * backend's own real, multi-window, multi-year-hardened feature set --
 * see window_wayland_native.h's own comment on CRTGFX_WINDOW_GPU_
 * PRESENTATION for why growing this to parity is explicit future work,
 * not an oversight here):
 *  - one shared connection per process, same real "lazily created on the
 *    first native-backend crtgfx_host_window_create(), destroyed once the
 *    last native-backend window closes" lifecycle the legacy backend's
 *    own crtgfx_wl_connection already established -- genuinely a
 *    *second*, independent connection alongside any live legacy one (see
 *    window_wayland_native.h's own top comment for why the two can never
 *    share one);
 *  - wl_compositor + xdg_wm_base + wl_seat globals only, each bound at
 *    protocol version 1 (matching the legacy backend's own "nothing past
 *    version 1 is ever needed" convention) -- no wl_shm/wl_output/
 *    clipboard-related globals are even requested yet;
 *  - one xdg_toplevel per window (get_toplevel only, no xdg_popup), real
 *    configure/ack_configure handshake, real xdg_toplevel::close ->
 *    CRTGFX_EVENT_CLOSE_REQUESTED;
 *  - real keyboard input via this project's own already-ported
 *    xkbcommon, same real keymap-compile-then-translate-each-key shape
 *    the legacy backend already uses (window_wayland.c's own wl_handle_
 *    keyboard_key(), mirrored here) -- CRTGFX_MOD_* modifiers are not
 *    populated, matching that same real, already-documented gap;
 *  - real pointer input (motion/button/scroll), keyed off which window's
 *    surface the compositor's own wl_pointer::enter most recently named,
 *    the same "most recent wins" simplification the legacy backend's own
 *    keyboard/pointer focus tracking already uses;
 *  - NO software (wl_shm) presentation -- crtgfx_native_wl_window_
 *    present_software() always returns CRTGFX_ERROR_UNSUPPORTED. This
 *    backend exists to give Vulkan's own vkCreateWaylandSurfaceKHR() a
 *    live wl_surface/wl_display pair; a real swapchain's own vkQueuePresentKHR
 *    is what actually maps/presents a native-backend window (via src/
 *    arch/linux/gpu_vulkan.c), there is no separate "attach a CPU buffer"
 *    path here the way the legacy backend has.
 *
 * WSL is a deliberate, permanent bypass, not a target to make real
 * presentation work through (matching this project's own prior Vulkan
 * offscreen work's own established WSL stance, and the user's own
 * explicit direction for this session): crtgfx_native_wl_is_wsl() below
 * detects it via the standard, well-known `/proc/sys/kernel/osrelease`
 * signal and makes crtgfx_native_wl_window_create() return CRTGFX_ERROR_
 * UNSUPPORTED immediately, before ever attempting a real wl_display_
 * connect() -- so a native-backend window request on WSL fails exactly
 * the same honest, graceful way a compositor-less headless CI host
 * already does, never a hang or a crash. */

#define CRTGFX_NATIVE_WL_TIMEOUT_MS 2000u
#define CRTGFX_NATIVE_WL_XKB_KEYCODE_OFFSET 8u

/* Real Linux evdev button codes (linux/input-event-codes.h) -- same real
 * values window_wayland.c's own CRTGFX_BTN_LEFT/RIGHT/MIDDLE use, kept as
 * a separate, identically-named-in-spirit set here rather than shared:
 * the two backends are deliberately independent translation units (see
 * window_wayland_native.h's own top comment), and these three constants
 * are stable, spec-frozen values, not something that could ever drift
 * between two copies. */
#define CRTGFX_NATIVE_BTN_LEFT 0x110u
#define CRTGFX_NATIVE_BTN_RIGHT 0x111u
#define CRTGFX_NATIVE_BTN_MIDDLE 0x112u

/* Opt-in wire-level tracing, same env var window_wayland.c's own
 * CRTGFX_WAYLAND_DEBUG already uses -- both backends share one on/off
 * switch since a real session debugging Wayland input/lifecycle issues
 * has no reason to care which backend a given window happens to use. */
static int crtgfx_native_wl_debug_enabled(void) {
  static int checked = 0;
  static int enabled = 0;
  if (!checked) {
    const char* v = getenv("CRTGFX_WAYLAND_DEBUG");
    enabled = (v != 0 && v[0] != '0' && v[0] != 0);
    checked = 1;
  }
  return enabled;
}
#define CRTGFX_NATIVE_WL_TRACE(...)                          \
  do {                                                       \
    if (crtgfx_native_wl_debug_enabled()) {                  \
      fprintf(stderr, "[crtgfx-wayland-native] " __VA_ARGS__); \
    }                                                         \
  } while (0)

struct crtgfx_native_wl_connection {
  struct wl_display* display;
  struct wl_registry* registry;
  struct wl_compositor* compositor;
  struct xdg_wm_base* wm_base;
  struct wl_seat* seat;
  struct wl_keyboard* keyboard;
  struct wl_pointer* pointer;

  struct xkb_context* xkb_context;
  struct xkb_keymap* xkb_keymap;
  struct xkb_state* xkb_state;

  struct crtgfx_native_wl_window* windows;
  struct crtgfx_native_wl_window* keyboard_focus;
  struct crtgfx_native_wl_window* pointer_focus;
  double pointer_x;
  double pointer_y;
};

/* `backend_tag` MUST stay the first field -- see window_wayland_native.h's
 * own top comment (crtgfx_wl_backend_tag()) for why. */
struct crtgfx_native_wl_window {
  uint32_t backend_tag;
  struct crtgfx_native_wl_connection* conn;
  struct crtgfx_native_wl_window* next;
  crtgfx_weston_toplevel* toplevel;
  struct wl_surface* surface;
  struct xdg_surface* xdg_surface;
  struct xdg_toplevel* xdg_toplevel;
  int has_first_configure;
};

/* One shared, process-wide connection -- same real single-connection
 * design as window_wayland.c's own crtgfx_wl_conn, independently. */
static struct crtgfx_native_wl_connection* g_native_wl_conn;

static int crtgfx_native_wl_str_contains_ci(const char* haystack, size_t haystack_len, const char* needle) {
  size_t needle_len = strlen(needle);
  size_t i;
  if (needle_len == 0 || haystack_len < needle_len) {
    return 0;
  }
  for (i = 0; i + needle_len <= haystack_len; ++i) {
    size_t j;
    int match = 1;
    for (j = 0; j < needle_len; ++j) {
      char a = haystack[i + j];
      char b = needle[j];
      if (a >= 'A' && a <= 'Z') {
        a = (char)(a - 'A' + 'a');
      }
      if (b >= 'A' && b <= 'Z') {
        b = (char)(b - 'A' + 'a');
      }
      if (a != b) {
        match = 0;
        break;
      }
    }
    if (match) {
      return 1;
    }
  }
  return 0;
}

/* Standard WSL detection (real, well-known signal: WSL's own kernel
 * reports a osrelease string containing "microsoft", case varying by
 * version -- e.g. real WSL2 reports "...-microsoft-standard-WSL2"). Raw
 * open()/read()/close() rather than stdio, matching this project's own
 * low-level-syscall style elsewhere in the Wayland backends (window_
 * wayland.c never uses stdio for anything other than its own opt-in
 * CRTGFX_WL_TRACE). Cached after the first real check -- this can never
 * change within one process's lifetime. */
static int crtgfx_native_wl_is_wsl(void) {
  static int checked = 0;
  static int is_wsl = 0;
  if (!checked) {
    int fd = open("/proc/sys/kernel/osrelease", O_RDONLY);
    if (fd >= 0) {
      char buf[256];
      ssize_t n = read(fd, buf, sizeof(buf) - 1);
      close(fd);
      if (n > 0) {
        is_wsl = crtgfx_native_wl_str_contains_ci(buf, (size_t)n, "microsoft");
      }
    }
    checked = 1;
    CRTGFX_NATIVE_WL_TRACE("wsl detection: is_wsl=%d\n", is_wsl);
  }
  return is_wsl;
}

static struct crtgfx_native_wl_window* crtgfx_native_wl_find_window_by_surface(
    struct crtgfx_native_wl_connection* conn, struct wl_surface* surface) {
  struct crtgfx_native_wl_window* w;
  for (w = conn->windows; w != 0; w = w->next) {
    if (w->surface == surface) {
      return w;
    }
  }
  return 0;
}

/* ---- xdg_wm_base ---- */

static void crtgfx_native_wm_base_ping(void* data, struct xdg_wm_base* wm_base, uint32_t serial) {
  (void)data;
  xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener crtgfx_native_wm_base_listener = {
    .ping = crtgfx_native_wm_base_ping,
};

/* ---- xdg_surface / xdg_toplevel ---- */

static void crtgfx_native_xdg_surface_configure(void* data, struct xdg_surface* surface, uint32_t serial) {
  struct crtgfx_native_wl_window* w = (struct crtgfx_native_wl_window*)data;
  xdg_surface_ack_configure(surface, serial);
  w->has_first_configure = 1;
  /* Real xdg-shell convention: a commit after ack_configure finalizes the
   * acknowledgement, even with no new buffer attached -- a Vulkan-only
   * window never attaches a CPU buffer here at all (see this file's own
   * top comment); the real map/first-visible-frame happens once this
   * window's own swapchain performs its first vkQueuePresentKHR(). */
  wl_surface_commit(w->surface);
}

static const struct xdg_surface_listener crtgfx_native_xdg_surface_listener = {
    .configure = crtgfx_native_xdg_surface_configure,
};

static void crtgfx_native_xdg_toplevel_configure(
    void* data, struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states) {
  struct crtgfx_native_wl_window* w = (struct crtgfx_native_wl_window*)data;
  (void)toplevel;
  (void)states;
  if (width > 0 && height > 0) {
    crtgfx_weston_toplevel_note_size(w->toplevel, (uint32_t)width, (uint32_t)height);
  }
}

static void crtgfx_native_xdg_toplevel_close(void* data, struct xdg_toplevel* toplevel) {
  struct crtgfx_native_wl_window* w = (struct crtgfx_native_wl_window*)data;
  (void)toplevel;
  crtgfx_weston_toplevel_note_close(w->toplevel);
}

static const struct xdg_toplevel_listener crtgfx_native_xdg_toplevel_listener = {
    .configure = crtgfx_native_xdg_toplevel_configure,
    .close = crtgfx_native_xdg_toplevel_close,
};

/* ---- wl_keyboard ---- */

static void crtgfx_native_keyboard_keymap(
    void* data, struct wl_keyboard* keyboard, uint32_t format, int32_t fd, uint32_t size) {
  struct crtgfx_native_wl_connection* conn = (struct crtgfx_native_wl_connection*)data;
  char* map_str;
  (void)keyboard;

  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || conn->xkb_context == 0) {
    close(fd);
    return;
  }
  map_str = (char*)mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (map_str == MAP_FAILED) {
    close(fd);
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
  /* size-1: the real wl_keyboard::keymap event's own `size` includes the
   * mapped region's trailing NUL (real protocol convention, matching
   * window_wayland.c's own equivalent comment) -- xkb_keymap_new_from_
   * buffer()'s own length argument wants the string length excluding it. */
  conn->xkb_keymap =
      xkb_keymap_new_from_buffer(conn->xkb_context, map_str, size - 1, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
  munmap(map_str, size);
  close(fd);
  if (conn->xkb_keymap != 0) {
    conn->xkb_state = xkb_state_new(conn->xkb_keymap);
  }
  CRTGFX_NATIVE_WL_TRACE("keyboard: keymap compiled ok=%d\n", conn->xkb_state != 0);
}

static void crtgfx_native_keyboard_enter(
    void* data, struct wl_keyboard* keyboard, uint32_t serial, struct wl_surface* surface,
    struct wl_array* keys) {
  struct crtgfx_native_wl_connection* conn = (struct crtgfx_native_wl_connection*)data;
  struct crtgfx_native_wl_window* w = crtgfx_native_wl_find_window_by_surface(conn, surface);
  (void)keyboard;
  (void)serial;
  (void)keys;
  if (w != 0) {
    conn->keyboard_focus = w;
    crtgfx_weston_toplevel_note_focus(w->toplevel, 1);
  }
}

static void crtgfx_native_keyboard_leave(
    void* data, struct wl_keyboard* keyboard, uint32_t serial, struct wl_surface* surface) {
  struct crtgfx_native_wl_connection* conn = (struct crtgfx_native_wl_connection*)data;
  (void)keyboard;
  (void)serial;
  if (conn->keyboard_focus != 0 && conn->keyboard_focus->surface == surface) {
    crtgfx_weston_toplevel_note_focus(conn->keyboard_focus->toplevel, 0);
    conn->keyboard_focus = 0;
  }
}

static void crtgfx_native_keyboard_key(
    void* data, struct wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
  struct crtgfx_native_wl_connection* conn = (struct crtgfx_native_wl_connection*)data;
  crtgfx_event event;
  xkb_keycode_t xkb_keycode;
  (void)keyboard;
  (void)serial;
  (void)time;

  if (conn->xkb_state == 0 || conn->keyboard_focus == 0) {
    return;
  }
  memset(&event, 0, sizeof(event));
  event.type = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? CRTGFX_EVENT_KEY_DOWN : CRTGFX_EVENT_KEY_UP;
  event.data.key.keycode = key;
  crtgfx_weston_toplevel_note_event(conn->keyboard_focus->toplevel, &event);

  if (state != WL_KEYBOARD_KEY_STATE_PRESSED) {
    return;
  }
  xkb_keycode = (xkb_keycode_t)(key + CRTGFX_NATIVE_WL_XKB_KEYCODE_OFFSET);
  memset(&event, 0, sizeof(event));
  event.type = CRTGFX_EVENT_TEXT;
  if (xkb_state_key_get_utf8(conn->xkb_state, xkb_keycode, event.data.text.utf8, sizeof(event.data.text.utf8)) >
      0) {
    crtgfx_weston_toplevel_note_event(conn->keyboard_focus->toplevel, &event);
  }
}

static void crtgfx_native_keyboard_modifiers(
    void* data, struct wl_keyboard* keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
    uint32_t mods_locked, uint32_t group) {
  struct crtgfx_native_wl_connection* conn = (struct crtgfx_native_wl_connection*)data;
  (void)keyboard;
  (void)serial;
  if (conn->xkb_state != 0) {
    xkb_state_update_mask(conn->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
  }
}

static const struct wl_keyboard_listener crtgfx_native_keyboard_listener = {
    .keymap = crtgfx_native_keyboard_keymap,
    .enter = crtgfx_native_keyboard_enter,
    .leave = crtgfx_native_keyboard_leave,
    .key = crtgfx_native_keyboard_key,
    .modifiers = crtgfx_native_keyboard_modifiers,
};

/* ---- wl_pointer ---- */

static void crtgfx_native_pointer_enter(
    void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface, wl_fixed_t sx,
    wl_fixed_t sy) {
  struct crtgfx_native_wl_connection* conn = (struct crtgfx_native_wl_connection*)data;
  struct crtgfx_native_wl_window* w = crtgfx_native_wl_find_window_by_surface(conn, surface);
  (void)pointer;
  (void)serial;
  if (w != 0) {
    conn->pointer_focus = w;
    conn->pointer_x = wl_fixed_to_double(sx);
    conn->pointer_y = wl_fixed_to_double(sy);
  }
}

static void crtgfx_native_pointer_leave(
    void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface) {
  struct crtgfx_native_wl_connection* conn = (struct crtgfx_native_wl_connection*)data;
  (void)pointer;
  (void)serial;
  if (conn->pointer_focus != 0 && conn->pointer_focus->surface == surface) {
    conn->pointer_focus = 0;
  }
}

static void crtgfx_native_pointer_motion(
    void* data, struct wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
  struct crtgfx_native_wl_connection* conn = (struct crtgfx_native_wl_connection*)data;
  crtgfx_event event;
  (void)pointer;
  (void)time;
  if (conn->pointer_focus == 0) {
    return;
  }
  conn->pointer_x = wl_fixed_to_double(sx);
  conn->pointer_y = wl_fixed_to_double(sy);
  memset(&event, 0, sizeof(event));
  event.type = CRTGFX_EVENT_POINTER_MOTION;
  event.data.pointer_motion.x = conn->pointer_x;
  event.data.pointer_motion.y = conn->pointer_y;
  crtgfx_weston_toplevel_note_event(conn->pointer_focus->toplevel, &event);
}

static void crtgfx_native_pointer_button(
    void* data, struct wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
  struct crtgfx_native_wl_connection* conn = (struct crtgfx_native_wl_connection*)data;
  crtgfx_event event;
  uint32_t crt_button;
  (void)pointer;
  (void)serial;
  (void)time;
  if (conn->pointer_focus == 0) {
    return;
  }
  switch (button) {
    case CRTGFX_NATIVE_BTN_LEFT:
      crt_button = CRTGFX_POINTER_BUTTON_LEFT;
      break;
    case CRTGFX_NATIVE_BTN_RIGHT:
      crt_button = CRTGFX_POINTER_BUTTON_RIGHT;
      break;
    case CRTGFX_NATIVE_BTN_MIDDLE:
      crt_button = CRTGFX_POINTER_BUTTON_MIDDLE;
      break;
    default:
      return;
  }
  memset(&event, 0, sizeof(event));
  event.type = (state == WL_POINTER_BUTTON_STATE_PRESSED) ? CRTGFX_EVENT_POINTER_BUTTON_DOWN
                                                            : CRTGFX_EVENT_POINTER_BUTTON_UP;
  event.data.pointer_button.button = crt_button;
  event.data.pointer_button.x = conn->pointer_x;
  event.data.pointer_button.y = conn->pointer_y;
  crtgfx_weston_toplevel_note_event(conn->pointer_focus->toplevel, &event);
}

static void crtgfx_native_pointer_axis(
    void* data, struct wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
  struct crtgfx_native_wl_connection* conn = (struct crtgfx_native_wl_connection*)data;
  crtgfx_event event;
  double v;
  (void)pointer;
  (void)time;
  if (conn->pointer_focus == 0) {
    return;
  }
  v = wl_fixed_to_double(value);
  memset(&event, 0, sizeof(event));
  event.type = CRTGFX_EVENT_POINTER_SCROLL;
  if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
    event.data.pointer_scroll.dx = v;
  } else {
    event.data.pointer_scroll.dy = v;
  }
  crtgfx_weston_toplevel_note_event(conn->pointer_focus->toplevel, &event);
}

static const struct wl_pointer_listener crtgfx_native_pointer_listener = {
    .enter = crtgfx_native_pointer_enter,
    .leave = crtgfx_native_pointer_leave,
    .motion = crtgfx_native_pointer_motion,
    .button = crtgfx_native_pointer_button,
    .axis = crtgfx_native_pointer_axis,
};

/* ---- wl_seat ---- */

static void crtgfx_native_seat_capabilities(void* data, struct wl_seat* seat, uint32_t capabilities) {
  struct crtgfx_native_wl_connection* conn = (struct crtgfx_native_wl_connection*)data;
  CRTGFX_NATIVE_WL_TRACE(
      "seat: capabilities=0x%x keyboard_bit=%d pointer_bit=%d\n", capabilities,
      (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0, (capabilities & WL_SEAT_CAPABILITY_POINTER) != 0);
  /* Capability *removal* (a keyboard/pointer this seat used to have going
   * away mid-session) is not handled -- a real, known, narrower-than-
   * legacy simplification matching this file's own top-comment scope; a
   * seat losing a device mid-session is rare in practice and this is a
   * first vertical slice, not yet full parity. */
  if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0 && conn->keyboard == 0) {
    conn->keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(conn->keyboard, &crtgfx_native_keyboard_listener, conn);
  }
  if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0 && conn->pointer == 0) {
    conn->pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(conn->pointer, &crtgfx_native_pointer_listener, conn);
  }
}

static const struct wl_seat_listener crtgfx_native_seat_listener = {
    .capabilities = crtgfx_native_seat_capabilities,
};

/* ---- wl_registry ---- */

static void crtgfx_native_registry_global(
    void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
  struct crtgfx_native_wl_connection* conn = (struct crtgfx_native_wl_connection*)data;
  (void)version;
  /* Every global here bound at version 1 -- see this file's own top
   * comment for why nothing past it is needed by this vertical slice. */
  if (strcmp(interface, "wl_compositor") == 0) {
    conn->compositor = (struct wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 1);
  } else if (strcmp(interface, "xdg_wm_base") == 0) {
    conn->wm_base = (struct xdg_wm_base*)wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener(conn->wm_base, &crtgfx_native_wm_base_listener, conn);
  } else if (strcmp(interface, "wl_seat") == 0) {
    conn->seat = (struct wl_seat*)wl_registry_bind(registry, name, &wl_seat_interface, 1);
    wl_seat_add_listener(conn->seat, &crtgfx_native_seat_listener, conn);
  }
}

static void crtgfx_native_registry_global_remove(void* data, struct wl_registry* registry, uint32_t name) {
  /* Not tracked -- see this file's own top comment on scope (no wl_output/
   * clipboard/multi-seat bookkeeping yet either). */
  (void)data;
  (void)registry;
  (void)name;
}

static const struct wl_registry_listener crtgfx_native_registry_listener = {
    .global = crtgfx_native_registry_global,
    .global_remove = crtgfx_native_registry_global_remove,
};

/* ---- connection lifecycle + event pump ---- */

static void crtgfx_native_wl_connection_destroy(struct crtgfx_native_wl_connection* conn) {
  if (conn == 0) {
    return;
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
  if (conn->keyboard != 0) {
    wl_keyboard_destroy(conn->keyboard);
  }
  if (conn->pointer != 0) {
    wl_pointer_destroy(conn->pointer);
  }
  if (conn->seat != 0) {
    wl_seat_destroy(conn->seat);
  }
  if (conn->wm_base != 0) {
    xdg_wm_base_destroy(conn->wm_base);
  }
  if (conn->compositor != 0) {
    wl_compositor_destroy(conn->compositor);
  }
  if (conn->registry != 0) {
    wl_registry_destroy(conn->registry);
  }
  if (conn->display != 0) {
    wl_display_disconnect(conn->display);
  }
  free(conn);
}

/* Real, canonical thread-safe libwayland-client event pump (the exact
 * prepare_read/flush/poll/read_events/dispatch_pending sequence real
 * upstream libwayland-client's own documentation prescribes for a client
 * that wants a real, bounded-timeout wait rather than wl_display_
 * dispatch()'s own unconditionally-blocking behavior) -- timeout_ms == 0
 * polls once without blocking, matching crtgfx_window_pump_events()'s own
 * documented contract (crtgfx/gpu.h... crtgfx/window.h's own multi-window
 * contract comment). A poll() timeout (nothing arrived) is a normal,
 * non-error outcome, not distinguished from "something arrived and was
 * dispatched" in the return value -- matching window_wayland.c's own
 * wl_pump() contract exactly. */
static int crtgfx_native_wl_pump(struct crtgfx_native_wl_connection* conn, uint32_t timeout_ms) {
  struct pollfd pfd;
  int poll_result;

  if (conn == 0) {
    return CRTGFX_OK;
  }
  while (wl_display_prepare_read(conn->display) != 0) {
    if (wl_display_dispatch_pending(conn->display) < 0) {
      return CRTGFX_ERROR_HOST;
    }
  }
  if (wl_display_flush(conn->display) < 0 && errno != EAGAIN) {
    wl_display_cancel_read(conn->display);
    return CRTGFX_ERROR_HOST;
  }
  pfd.fd = wl_display_get_fd(conn->display);
  pfd.events = POLLIN;
  pfd.revents = 0;
  poll_result = poll(&pfd, 1, (int)timeout_ms);
  if (poll_result <= 0) {
    wl_display_cancel_read(conn->display);
    return (poll_result < 0) ? CRTGFX_ERROR_HOST : CRTGFX_OK;
  }
  if (wl_display_read_events(conn->display) < 0) {
    return CRTGFX_ERROR_HOST;
  }
  if (wl_display_dispatch_pending(conn->display) < 0) {
    return CRTGFX_ERROR_HOST;
  }
  return CRTGFX_OK;
}

int crtgfx_native_wl_window_create(const crtgfx_window_desc* desc, crtgfx_weston_toplevel* toplevel) {
  struct crtgfx_native_wl_connection* conn;
  struct crtgfx_native_wl_window* w;
  int created_connection = 0;
  struct timespec deadline;
  struct timespec now;

  if (desc == 0 || toplevel == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  if (crtgfx_native_wl_is_wsl()) {
    /* Deliberate, permanent bypass -- see this file's own top comment. */
    return CRTGFX_ERROR_UNSUPPORTED;
  }

  conn = g_native_wl_conn;
  if (conn == 0) {
    conn = (struct crtgfx_native_wl_connection*)calloc(1, sizeof(*conn));
    if (conn == 0) {
      return CRTGFX_ERROR_HOST;
    }
    conn->display = wl_display_connect(0);
    if (conn->display == 0) {
      /* No compositor reachable -- same real, honest "no usable host
       * backend right now" contract every other backend uses. */
      free(conn);
      return CRTGFX_ERROR_UNSUPPORTED;
    }
    conn->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    conn->registry = wl_display_get_registry(conn->display);
    wl_registry_add_listener(conn->registry, &crtgfx_native_registry_listener, conn);
    if (wl_display_roundtrip(conn->display) < 0) {
      crtgfx_native_wl_connection_destroy(conn);
      return CRTGFX_ERROR_HOST;
    }
    if (conn->compositor == 0 || conn->wm_base == 0) {
      /* A real compositor is reachable but does not advertise a required
       * global (e.g. no xdg-shell support at all) -- same honest
       * CRTGFX_ERROR_UNSUPPORTED contract, not a crash. */
      crtgfx_native_wl_connection_destroy(conn);
      return CRTGFX_ERROR_UNSUPPORTED;
    }
    g_native_wl_conn = conn;
    created_connection = 1;
  }

  w = (struct crtgfx_native_wl_window*)calloc(1, sizeof(*w));
  if (w == 0) {
    if (created_connection) {
      crtgfx_native_wl_connection_destroy(conn);
      g_native_wl_conn = 0;
    }
    return CRTGFX_ERROR_HOST;
  }
  w->backend_tag = CRTGFX_WL_BACKEND_TAG_NATIVE;
  w->conn = conn;
  w->toplevel = toplevel;

  w->surface = wl_compositor_create_surface(conn->compositor);
  w->xdg_surface = xdg_wm_base_get_xdg_surface(conn->wm_base, w->surface);
  xdg_surface_add_listener(w->xdg_surface, &crtgfx_native_xdg_surface_listener, w);
  w->xdg_toplevel = xdg_surface_get_toplevel(w->xdg_surface);
  xdg_toplevel_add_listener(w->xdg_toplevel, &crtgfx_native_xdg_toplevel_listener, w);
  if (desc->title != 0) {
    xdg_toplevel_set_title(w->xdg_toplevel, desc->title);
  }
  toplevel->width = desc->width;
  toplevel->height = desc->height;
  wl_surface_commit(w->surface);

  /* Real bring-up gate, same real reasoning as window_wayland.c's own
   * crtgfx_wl_window_attach(): block (bounded by CRTGFX_NATIVE_WL_TIMEOUT_
   * MS) until the compositor's own first xdg_surface::configure actually
   * arrives, so a caller's very first crtgfx_window_get_size() already
   * sees the compositor's own real initial size. */
  clock_gettime(CLOCK_MONOTONIC, &deadline);
  deadline.tv_sec += (time_t)(CRTGFX_NATIVE_WL_TIMEOUT_MS / 1000u);
  deadline.tv_nsec += (long)(CRTGFX_NATIVE_WL_TIMEOUT_MS % 1000u) * 1000000L;
  if (deadline.tv_nsec >= 1000000000L) {
    deadline.tv_nsec -= 1000000000L;
    deadline.tv_sec += 1;
  }
  while (!w->has_first_configure) {
    long remaining_ms;
    clock_gettime(CLOCK_MONOTONIC, &now);
    remaining_ms =
        (long)(deadline.tv_sec - now.tv_sec) * 1000L + (deadline.tv_nsec - now.tv_nsec) / 1000000L;
    if (remaining_ms <= 0 || crtgfx_native_wl_pump(conn, (uint32_t)remaining_ms) != CRTGFX_OK) {
      xdg_toplevel_destroy(w->xdg_toplevel);
      xdg_surface_destroy(w->xdg_surface);
      wl_surface_destroy(w->surface);
      free(w);
      if (created_connection) {
        crtgfx_native_wl_connection_destroy(conn);
        g_native_wl_conn = 0;
      }
      return CRTGFX_ERROR_HOST;
    }
  }

  w->next = conn->windows;
  conn->windows = w;
  toplevel->host = (crtgfx_host_window*)w;
  return CRTGFX_OK;
}

void crtgfx_native_wl_window_destroy(void* host) {
  struct crtgfx_native_wl_window* w = (struct crtgfx_native_wl_window*)host;
  struct crtgfx_native_wl_connection* conn;
  struct crtgfx_native_wl_window** link;

  if (w == 0) {
    return;
  }
  conn = w->conn;
  if (conn->keyboard_focus == w) {
    conn->keyboard_focus = 0;
  }
  if (conn->pointer_focus == w) {
    conn->pointer_focus = 0;
  }
  if (w->xdg_toplevel != 0) {
    xdg_toplevel_destroy(w->xdg_toplevel);
  }
  if (w->xdg_surface != 0) {
    xdg_surface_destroy(w->xdg_surface);
  }
  if (w->surface != 0) {
    wl_surface_destroy(w->surface);
  }
  link = &conn->windows;
  while (*link != 0) {
    if (*link == w) {
      *link = w->next;
      break;
    }
    link = &(*link)->next;
  }
  free(w);

  if (conn->windows == 0) {
    crtgfx_native_wl_connection_destroy(conn);
    g_native_wl_conn = 0;
  }
}

int crtgfx_native_wl_window_show(void* host) {
  if (host == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  /* Same real reasoning as window_wayland.c's own crtgfx_host_window_
   * show(): Wayland has no separate "show" request; this window becomes
   * visible once its own swapchain performs a real vkQueuePresentKHR(). */
  return CRTGFX_OK;
}

int crtgfx_native_wl_window_get_size(void* host, uint32_t* out_width, uint32_t* out_height) {
  struct crtgfx_native_wl_window* w = (struct crtgfx_native_wl_window*)host;
  if (w == 0 || out_width == 0 || out_height == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  *out_width = w->toplevel->width;
  *out_height = w->toplevel->height;
  return CRTGFX_OK;
}

int crtgfx_native_wl_window_present_software(
    void* host, const void* pixels, uint32_t width, uint32_t height, uint32_t stride,
    const crtgfx_damage_rect* damage_rects, uint32_t damage_rect_count) {
  (void)pixels;
  (void)width;
  (void)height;
  (void)stride;
  (void)damage_rects;
  (void)damage_rect_count;
  if (host == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  /* See this file's own top comment and window_wayland_native.h's own
   * comment on this function -- a real, later Phase 3 addition, not
   * implemented yet. */
  return CRTGFX_ERROR_UNSUPPORTED;
}

int crtgfx_native_wl_dispatch(uint32_t timeout_ms) {
  return crtgfx_native_wl_pump(g_native_wl_conn, timeout_ms);
}

int crtgfx_native_wl_has_connection(void) {
  return g_native_wl_conn != 0;
}

int crtgfx_native_wl_get_surface_handles(
    const crtgfx_weston_toplevel* toplevel, void** out_display, void** out_surface) {
  const struct crtgfx_native_wl_window* w;
  if (toplevel == 0 || toplevel->host == 0 || out_display == 0 || out_surface == 0) {
    return 0;
  }
  if (crtgfx_wl_backend_tag(toplevel->host) != CRTGFX_WL_BACKEND_TAG_NATIVE) {
    return 0;
  }
  w = (const struct crtgfx_native_wl_window*)toplevel->host;
  *out_display = (void*)w->conn->display;
  *out_surface = (void*)w->surface;
  return 1;
}
