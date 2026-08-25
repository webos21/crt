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
 *  - one Wayland connection per window (no shared/global display object
 *    across multiple windows yet -- crtgfx_host_window_dispatch() has no
 *    window parameter at all, matching Win32's thread-global message
 *    queue, so it operates on a single process-wide "active window"
 *    pointer; a real multi-window design needs a shared connection);
 *  - software (wl_shm) presentation only, no GPU/EGL path yet;
 *  - keyboard input only (2026-08-25, Phase 3 of the "notepad-capability"
 *    plan): wl_seat -> wl_keyboard, real UTF-8 text via this project's own
 *    libxkbcommon port (libcrtgfx/third_party/xkbcommon/recipe.json) --
 *    no wl_pointer/mouse handling yet, and no wl_touch;
 *  - wl_buffer lifetime is release-tracked: every presented wl_shm buffer
 *    stays mapped/open until the compositor sends wl_buffer::release, then
 *    this backend destroys the wl_buffer object and unmaps/closes its backing
 *    storage. This is the first real frame-lifecycle contract shared with
 *    the higher crtgfx software-frame API;
 *  - object ids are allocated monotonically, never recycled (fine for a
 *    short-lived process, not for one that opens/closes many windows).
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
/* wl_surface */
#define WL_SURFACE_ATTACH 1u
#define WL_SURFACE_DAMAGE 2u
#define WL_SURFACE_COMMIT 6u
/* xdg_wm_base */
#define XDG_WM_BASE_GET_XDG_SURFACE 2u
#define XDG_WM_BASE_PONG 3u
#define XDG_WM_BASE_EVENT_PING 0u
/* xdg_surface */
#define XDG_SURFACE_GET_TOPLEVEL 1u
#define XDG_SURFACE_ACK_CONFIGURE 4u
#define XDG_SURFACE_EVENT_CONFIGURE 0u
/* xdg_toplevel */
#define XDG_TOPLEVEL_SET_TITLE 2u
#define XDG_TOPLEVEL_EVENT_CONFIGURE 0u
#define XDG_TOPLEVEL_EVENT_CLOSE 1u
/* wl_seat */
#define WL_SEAT_GET_KEYBOARD 1u
#define WL_SEAT_EVENT_CAPABILITIES 0u
#define WL_SEAT_CAPABILITY_KEYBOARD (1u << 1)
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

struct crtgfx_host_window {
  int fd;
  uint32_t next_id;

  uint32_t registry_id;
  uint32_t sync_callback_id;
  uint32_t compositor_id;
  uint32_t shm_id;
  uint32_t wm_base_id;
  uint32_t surface_id;
  uint32_t xdg_surface_id;
  uint32_t xdg_toplevel_id;
  /* 0 means "no wl_seat/wl_keyboard bound" -- object id 0 is never a real
   * Wayland object (id 1 is always wl_display, the lowest object id this
   * backend or any real compositor ever allocates), so it doubles safely
   * as an explicit "absent" sentinel without a separate bool. wl_seat is
   * optional (unlike wl_compositor/wl_shm/xdg_wm_base): a compositor with
   * no seat at all, or a seat with no keyboard capability, just means no
   * keyboard events are ever queued -- not a connection failure. */
  uint32_t seat_id;
  uint32_t keyboard_id;

  int have_first_configure;
  uint32_t configure_serial;

  struct crtgfx_wl_buffer* buffers;

  /* xkbcommon state for the currently bound keyboard -- xkb_keymap/
   * xkb_state are both null until the real wl_keyboard::keymap event
   * arrives (see wl_dispatch_message()'s own WL_KEYBOARD_EVENT_KEYMAP
   * handling); xkb_context is created once, up front, in crtgfx_host_
   * window_create(), and lives for the whole connection's lifetime. */
  struct xkb_context* xkb_context;
  struct xkb_keymap* xkb_keymap;
  struct xkb_state* xkb_state;

  crtgfx_weston_toplevel* toplevel;
};

/* crtgfx_host_window_dispatch() (see libcrtgfx/include/crtgfx/window.h's
 * crtgfx_window_pump_events()) takes no window argument, matching Win32's
 * thread-global message queue -- see this file's top comment. */
static struct crtgfx_host_window* crtgfx_wl_active;

/* Only populated during crtgfx_host_window_create()'s initial registry
 * roundtrip; passed through wl_dispatch_message() as an optional pointer. */
struct crtgfx_wl_bootstrap {
  uint32_t compositor_name;
  uint32_t shm_name;
  uint32_t wm_base_name;
  uint32_t seat_name; /* CRTGFX_WL_NAME_NONE if the compositor has no wl_seat at all */
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
 * wl_shm::create_pool needs this). */
static int wl_send(
    struct crtgfx_host_window* host, uint32_t object_id, uint32_t opcode, const void* body,
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
  sent = sendmsg(host->fd, &msg, 0);
  return (sent == (ssize_t)total) ? CRTGFX_OK : CRTGFX_ERROR_HOST;
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

static void wl_destroy_released_buffer(struct crtgfx_host_window* host, uint32_t id) {
  struct crtgfx_wl_buffer** link;

  if (host == 0) {
    return;
  }
  link = &host->buffers;
  while (*link != 0) {
    struct crtgfx_wl_buffer* buffer = *link;

    if (buffer->id == id) {
      *link = buffer->next;
      (void)wl_send(host, id, WL_BUFFER_DESTROY, 0, 0, -1);
      wl_buffer_destroy_storage(buffer);
      return;
    }
    link = &buffer->next;
  }
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
    struct crtgfx_host_window* host, uint32_t* out_object_id, uint32_t* out_opcode,
    unsigned char* body, size_t body_cap, size_t* out_body_len, uint32_t timeout_ms, int* out_fd) {
  uint32_t header[2];
  uint32_t opcode;
  uint32_t size;
  size_t body_len;
  int rc;

  if (out_fd != 0) {
    *out_fd = -1;
  }
  rc = wl_read_exact(host->fd, header, sizeof(header), timeout_ms, out_fd);
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
    rc = wl_read_exact(host->fd, body, body_len, timeout_ms, out_fd);
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
 * xkbcommon state and queues the resulting crtgfx_event(s): always a
 * KEY_DOWN/KEY_UP, and -- for a press that actually produces text (not
 * Escape/arrows/bare-modifier/...) -- a following TEXT event carrying the
 * real, already-composed UTF-8 xkb_state_key_get_utf8() returns. Does
 * nothing if no keymap has been compiled yet (host->xkb_state == 0):
 * a real compositor always sends wl_keyboard::keymap before the first
 * key event on a freshly bound keyboard, but a key press that outraces
 * it on a genuinely broken compositor should not crash on a null state. */
static void wl_handle_keyboard_key(struct crtgfx_host_window* host, uint32_t key, uint32_t state) {
  crtgfx_event event;
  xkb_keycode_t xkb_keycode;

  if (host->xkb_state == 0) {
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
  crtgfx_weston_toplevel_note_event(host->toplevel, &event);

  if (state != WL_KEYBOARD_KEY_STATE_PRESSED) {
    return;
  }
  xkb_keycode = (xkb_keycode_t)(key + CRTGFX_WL_XKB_KEYCODE_OFFSET);
  memset(&event, 0, sizeof(event));
  event.type = CRTGFX_EVENT_TEXT;
  if (xkb_state_key_get_utf8(host->xkb_state, xkb_keycode, event.data.text.utf8,
                             sizeof(event.data.text.utf8)) > 0) {
    crtgfx_weston_toplevel_note_event(host->toplevel, &event);
  }
}

static void wl_dispatch_message(
    struct crtgfx_host_window* host, struct crtgfx_wl_bootstrap* boot, uint32_t object_id,
    uint32_t opcode, const unsigned char* body, size_t body_len, int fd) {
  (void)body_len;

  if (boot != 0 && object_id == host->registry_id && opcode == WL_REGISTRY_EVENT_GLOBAL) {
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
    }
    return;
  }
  if (boot != 0 && object_id == host->sync_callback_id && opcode == WL_CALLBACK_EVENT_DONE) {
    boot->sync_done = 1;
    return;
  }
  if (object_id == host->wm_base_id && opcode == XDG_WM_BASE_EVENT_PING) {
    unsigned char out[4];

    wl_put_u32(out, 0, wl_get_u32(body, 0));
    wl_send(host, host->wm_base_id, XDG_WM_BASE_PONG, out, sizeof(out), -1);
    return;
  }
  if (object_id == host->xdg_surface_id && opcode == XDG_SURFACE_EVENT_CONFIGURE) {
    host->configure_serial = wl_get_u32(body, 0);
    host->have_first_configure = 1;
    return;
  }
  if (object_id == host->xdg_toplevel_id && opcode == XDG_TOPLEVEL_EVENT_CONFIGURE) {
    uint32_t width = wl_get_u32(body, 0);
    uint32_t height = wl_get_u32(body, 4);

    if (width != 0u && height != 0u) {
      crtgfx_weston_toplevel_note_size(host->toplevel, width, height);
    }
    return;
  }
  if (object_id == host->xdg_toplevel_id && opcode == XDG_TOPLEVEL_EVENT_CLOSE) {
    crtgfx_weston_toplevel_note_close(host->toplevel);
    return;
  }
  if (opcode == WL_BUFFER_EVENT_RELEASE) {
    wl_destroy_released_buffer(host, object_id);
    return;
  }
  if (host->seat_id != 0 && object_id == host->seat_id && opcode == WL_SEAT_EVENT_CAPABILITIES) {
    uint32_t capabilities = wl_get_u32(body, 0);
    unsigned char out[4];

    CRTGFX_WL_TRACE("seat: capabilities=0x%x keyboard_bit=%d\n", capabilities,
                     (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0u);
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0u && host->keyboard_id == 0u) {
      host->keyboard_id = host->next_id++;
      wl_put_u32(out, 0, host->keyboard_id);
      wl_send(host, host->seat_id, WL_SEAT_GET_KEYBOARD, out, sizeof(out), -1);
      CRTGFX_WL_TRACE("seat: requesting keyboard id=%u\n", host->keyboard_id);
    }
    return;
  }
  if (host->keyboard_id != 0 && object_id == host->keyboard_id) {
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
       * wl_keyboard at version 1 (see crtgfx_host_window_create()). */
      mapping = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
      close(fd);
      if (mapping == MAP_FAILED) {
        return;
      }
      if (host->xkb_state != 0) {
        xkb_state_unref(host->xkb_state);
        host->xkb_state = 0;
      }
      if (host->xkb_keymap != 0) {
        xkb_keymap_unref(host->xkb_keymap);
        host->xkb_keymap = 0;
      }
      /* size - 1: the mapped keymap string is NUL-terminated and `size`
       * (the wire value) includes that trailing NUL -- matching real
       * upstream libxkbcommon's own example exactly (confirmed by
       * reading it directly, not guessed: passing the NUL byte itself
       * into xkb_keymap_new_from_buffer()'s own length argument is not
       * what any real client does). */
      if (size > 0u) {
        host->xkb_keymap = xkb_keymap_new_from_buffer(
            host->xkb_context, (const char*)mapping, (size_t)(size - 1u),
            XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
      }
      munmap(mapping, size);
      if (host->xkb_keymap != 0) {
        host->xkb_state = xkb_state_new(host->xkb_keymap);
      }
      CRTGFX_WL_TRACE("keyboard: keymap compiled=%d state=%d\n", host->xkb_keymap != 0,
                       host->xkb_state != 0);
      return;
    }
    if (opcode == WL_KEYBOARD_EVENT_KEY) {
      uint32_t key = wl_get_u32(body, 8);
      uint32_t state = wl_get_u32(body, 12);

      CRTGFX_WL_TRACE("keyboard: key=%u state=%u (xkb_state=%d)\n", key, state,
                       host->xkb_state != 0);
      wl_handle_keyboard_key(host, key, state);
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
      xkb_state_update_mask(host->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
      return;
    }
    /* wl_keyboard::enter/leave/repeat_info: intentionally not acted on
     * (no focus tracking, no client-side key-repeat timer yet), but still
     * traced -- opcode alone (enter=1, leave=2, repeat_info=5) tells a
     * real diagnostic session whether the compositor ever gave this
     * surface keyboard focus at all, which is the single most useful
     * signal for "window opens but never receives input" reports (a
     * missing/absent trace line here means the compositor's own window
     * manager never focused the window -- a real environment/WM
     * condition, not a protocol bug in this client). */
    CRTGFX_WL_TRACE("keyboard: event opcode=%u (1=enter 2=leave 5=repeat_info)\n", opcode);
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

static int wl_pump(struct crtgfx_host_window* host, uint32_t timeout_ms) {
  struct pollfd pfd;
  int pr;

  if (host == 0 || host->fd < 0) {
    return CRTGFX_OK;
  }
  pfd.fd = host->fd;
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

    if (wl_recv_message(host, &object_id, &opcode, body, sizeof(body), &body_len, 0, &recv_fd) != CRTGFX_OK) {
      break;
    }
    wl_dispatch_message(host, 0, object_id, opcode, body, body_len, recv_fd);

    pfd.revents = 0;
    if (poll(&pfd, 1, 0) <= 0) {
      break;
    }
  }
  return CRTGFX_OK;
}

static int wl_connect(struct crtgfx_host_window* host) {
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
  host->fd = fd;
  return CRTGFX_OK;
}

int crtgfx_host_window_create(const crtgfx_window_desc* desc, crtgfx_weston_toplevel* toplevel) {
  struct crtgfx_host_window* host;
  struct crtgfx_wl_bootstrap boot;
  unsigned char body[512];
  size_t off;
  int rc;

  if (desc == 0 || toplevel == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }

  host = (struct crtgfx_host_window*)calloc(1, sizeof(*host));
  if (host == 0) {
    return CRTGFX_ERROR_HOST;
  }
  host->fd = -1;
  host->toplevel = toplevel;
  host->next_id = 2;

  /* XKB_CONTEXT_NO_FLAGS: the real, documented "just give me a normal
   * context" value (0) -- created once, up front, and kept for this
   * connection's whole lifetime, independent of whether a keymap ever
   * actually arrives (a compositor with no wl_seat, or a seat with no
   * keyboard capability, just means this context never gets used). */
  host->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (host->xkb_context == 0) {
    free(host);
    return CRTGFX_ERROR_HOST;
  }

  rc = wl_connect(host);
  if (rc != CRTGFX_OK) {
    xkb_context_unref(host->xkb_context);
    free(host);
    return rc;
  }

  memset(&boot, 0, sizeof(boot));
  boot.compositor_name = CRTGFX_WL_NAME_NONE;
  boot.shm_name = CRTGFX_WL_NAME_NONE;
  boot.wm_base_name = CRTGFX_WL_NAME_NONE;
  boot.seat_name = CRTGFX_WL_NAME_NONE;

  host->registry_id = host->next_id++;
  off = wl_put_u32(body, 0, host->registry_id);
  if (wl_send(host, 1, WL_DISPLAY_GET_REGISTRY, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  host->sync_callback_id = host->next_id++;
  off = wl_put_u32(body, 0, host->sync_callback_id);
  if (wl_send(host, 1, WL_DISPLAY_SYNC, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  while (!boot.sync_done) {
    uint32_t object_id;
    uint32_t opcode;
    unsigned char rbody[CRTGFX_WL_MAX_MSG];
    size_t rlen;
    int recv_fd;

    rc = wl_recv_message(host, &object_id, &opcode, rbody, sizeof(rbody), &rlen, CRTGFX_WL_TIMEOUT_MS, &recv_fd);
    if (rc != CRTGFX_OK) {
      goto fail;
    }
    wl_dispatch_message(host, &boot, object_id, opcode, rbody, rlen, recv_fd);
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

  host->compositor_id = host->next_id++;
  off = wl_put_u32(body, 0, boot.compositor_name);
  off = wl_put_string(body, sizeof(body), off, "wl_compositor");
  off = wl_put_u32(body, off, 1);
  off = wl_put_u32(body, off, host->compositor_id);
  if (wl_send(host, host->registry_id, WL_REGISTRY_BIND, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  host->shm_id = host->next_id++;
  off = wl_put_u32(body, 0, boot.shm_name);
  off = wl_put_string(body, sizeof(body), off, "wl_shm");
  off = wl_put_u32(body, off, 1);
  off = wl_put_u32(body, off, host->shm_id);
  if (wl_send(host, host->registry_id, WL_REGISTRY_BIND, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  host->wm_base_id = host->next_id++;
  off = wl_put_u32(body, 0, boot.wm_base_name);
  off = wl_put_string(body, sizeof(body), off, "xdg_wm_base");
  off = wl_put_u32(body, off, 1);
  off = wl_put_u32(body, off, host->wm_base_id);
  if (wl_send(host, host->registry_id, WL_REGISTRY_BIND, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  /* wl_seat, unlike compositor/shm/wm_base just above, is optional: a
   * compositor with no seat at all just means no keyboard events are ever
   * queued, not a connection failure (see this file's own top-of-file
   * scope note). Its own wl_seat::capabilities event (sent "on binding to
   * the seat global", per wayland.xml's own doc) -- and, once that
   * arrives with the keyboard bit set, the wl_keyboard::keymap/key/
   * modifiers events that follow -- get processed by wl_dispatch_message()
   * exactly like any other event, whichever loop below happens to read
   * them first; no extra explicit roundtrip is added here for it. */
  if (boot.seat_name != CRTGFX_WL_NAME_NONE) {
    host->seat_id = host->next_id++;
    off = wl_put_u32(body, 0, boot.seat_name);
    off = wl_put_string(body, sizeof(body), off, "wl_seat");
    off = wl_put_u32(body, off, 1);
    off = wl_put_u32(body, off, host->seat_id);
    if (wl_send(host, host->registry_id, WL_REGISTRY_BIND, body, off, -1) != CRTGFX_OK) {
      rc = CRTGFX_ERROR_HOST;
      goto fail;
    }
    CRTGFX_WL_TRACE("seat: bind requested name=%u id=%u\n", boot.seat_name, host->seat_id);
  } else {
    CRTGFX_WL_TRACE("seat: no wl_seat global advertised by this compositor\n");
  }

  host->surface_id = host->next_id++;
  off = wl_put_u32(body, 0, host->surface_id);
  if (wl_send(host, host->compositor_id, WL_COMPOSITOR_CREATE_SURFACE, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  host->xdg_surface_id = host->next_id++;
  off = wl_put_u32(body, 0, host->xdg_surface_id);
  off = wl_put_u32(body, off, host->surface_id);
  if (wl_send(host, host->wm_base_id, XDG_WM_BASE_GET_XDG_SURFACE, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  host->xdg_toplevel_id = host->next_id++;
  off = wl_put_u32(body, 0, host->xdg_toplevel_id);
  if (wl_send(host, host->xdg_surface_id, XDG_SURFACE_GET_TOPLEVEL, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  off = wl_put_string(body, sizeof(body), 0, desc->title != 0 ? desc->title : "crtgfx");
  if (off == (size_t)-1 ||
      wl_send(host, host->xdg_toplevel_id, XDG_TOPLEVEL_SET_TITLE, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  /* Initial "null commit" (no buffer attached yet) -- required by
   * xdg-shell to trigger the first configure sequence below. */
  if (wl_send(host, host->surface_id, WL_SURFACE_COMMIT, 0, 0, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  while (!host->have_first_configure) {
    uint32_t object_id;
    uint32_t opcode;
    unsigned char rbody[CRTGFX_WL_MAX_MSG];
    size_t rlen;
    int recv_fd;

    rc = wl_recv_message(host, &object_id, &opcode, rbody, sizeof(rbody), &rlen, CRTGFX_WL_TIMEOUT_MS, &recv_fd);
    if (rc != CRTGFX_OK) {
      goto fail;
    }
    wl_dispatch_message(host, 0, object_id, opcode, rbody, rlen, recv_fd);
  }

  off = wl_put_u32(body, 0, host->configure_serial);
  if (wl_send(host, host->xdg_surface_id, XDG_SURFACE_ACK_CONFIGURE, body, off, -1) != CRTGFX_OK) {
    rc = CRTGFX_ERROR_HOST;
    goto fail;
  }

  toplevel->host = host;
  crtgfx_wl_active = host;
  return CRTGFX_OK;

fail:
  if (host->xkb_state != 0) {
    xkb_state_unref(host->xkb_state);
  }
  if (host->xkb_keymap != 0) {
    xkb_keymap_unref(host->xkb_keymap);
  }
  xkb_context_unref(host->xkb_context);
  close(host->fd);
  free(host);
  return (rc == CRTGFX_ERROR_UNSUPPORTED) ? CRTGFX_ERROR_UNSUPPORTED : CRTGFX_ERROR_HOST;
}

void crtgfx_host_window_destroy(crtgfx_host_window* host) {
  if (host == 0) {
    return;
  }
  if (crtgfx_wl_active == host) {
    crtgfx_wl_active = 0;
  }
  wl_destroy_all_buffers(host);
  if (host->xkb_state != 0) {
    xkb_state_unref(host->xkb_state);
  }
  if (host->xkb_keymap != 0) {
    xkb_keymap_unref(host->xkb_keymap);
  }
  xkb_context_unref(host->xkb_context);
  if (host->fd >= 0) {
    close(host->fd);
  }
  free(host);
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
  return wl_pump(crtgfx_wl_active, timeout_ms);
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
    crtgfx_host_window* host, const void* pixels, uint32_t width, uint32_t height, uint32_t stride) {
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
  size = stride * height;
  (void)wl_pump(host, 0);

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

  pool_id = host->next_id++;
  off = wl_put_u32(body, 0, pool_id);
  off = wl_put_u32(body, off, (uint32_t)size);
  if (wl_send(host, host->shm_id, WL_SHM_CREATE_POOL, body, off, memfd) != CRTGFX_OK) {
    munmap(mapping, size);
    close(memfd);
    return CRTGFX_ERROR_HOST;
  }

  buffer_id = host->next_id++;
  off = wl_put_u32(body, 0, buffer_id);
  off = wl_put_u32(body, off, 0); /* offset */
  off = wl_put_u32(body, off, width);
  off = wl_put_u32(body, off, height);
  off = wl_put_u32(body, off, stride);
  off = wl_put_u32(body, off, CRTGFX_WL_FORMAT_ARGB8888);
  if (wl_send(host, pool_id, WL_SHM_POOL_CREATE_BUFFER, body, off, -1) != CRTGFX_OK) {
    munmap(mapping, size);
    close(memfd);
    return CRTGFX_ERROR_HOST;
  }
  submitted = (struct crtgfx_wl_buffer*)calloc(1, sizeof(*submitted));
  if (submitted == 0) {
    wl_send(host, buffer_id, WL_BUFFER_DESTROY, 0, 0, -1);
    munmap(mapping, size);
    close(memfd);
    return CRTGFX_ERROR_HOST;
  }
  /* A buffer created through a pool stays valid after the pool itself is
   * destroyed (real protocol guarantee) -- destroy it right away so the
   * object id table doesn't grow across repeated presents. */
  wl_send(host, pool_id, WL_SHM_POOL_DESTROY, 0, 0, -1);

  off = wl_put_u32(body, 0, buffer_id);
  off = wl_put_u32(body, off, 0);
  off = wl_put_u32(body, off, 0);
  if (wl_send(host, host->surface_id, WL_SURFACE_ATTACH, body, off, -1) != CRTGFX_OK) {
    free(submitted);
    munmap(mapping, size);
    close(memfd);
    return CRTGFX_ERROR_HOST;
  }

  off = wl_put_u32(body, 0, 0);
  off = wl_put_u32(body, off, 0);
  off = wl_put_u32(body, off, width);
  off = wl_put_u32(body, off, height);
  if (wl_send(host, host->surface_id, WL_SURFACE_DAMAGE, body, off, -1) != CRTGFX_OK) {
    free(submitted);
    munmap(mapping, size);
    close(memfd);
    return CRTGFX_ERROR_HOST;
  }

  if (wl_send(host, host->surface_id, WL_SURFACE_COMMIT, 0, 0, -1) != CRTGFX_OK) {
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
