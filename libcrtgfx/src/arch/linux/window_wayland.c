#include "wayland_weston_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

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
 *  - no keyboard/pointer/wl_seat input handling yet;
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

  int have_first_configure;
  uint32_t configure_serial;

  struct crtgfx_wl_buffer* buffers;

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

static int wl_read_exact(int fd, void* buf, size_t len, uint32_t timeout_ms) {
  unsigned char* p = (unsigned char*)buf;
  size_t got = 0;

  while (got < len) {
    struct pollfd pfd;
    int pr;
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
    n = read(fd, p + got, len - got);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return CRTGFX_ERROR_HOST;
    }
    if (n == 0) {
      return CRTGFX_ERROR_HOST;
    }
    got += (size_t)n;
  }
  return CRTGFX_OK;
}

static int wl_recv_message(
    struct crtgfx_host_window* host, uint32_t* out_object_id, uint32_t* out_opcode,
    unsigned char* body, size_t body_cap, size_t* out_body_len, uint32_t timeout_ms) {
  uint32_t header[2];
  uint32_t opcode;
  uint32_t size;
  size_t body_len;
  int rc;

  rc = wl_read_exact(host->fd, header, sizeof(header), timeout_ms);
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
    rc = wl_read_exact(host->fd, body, body_len, timeout_ms);
    if (rc != CRTGFX_OK) {
      return rc;
    }
  }
  *out_object_id = header[0];
  *out_opcode = opcode;
  *out_body_len = body_len;
  return CRTGFX_OK;
}

static void wl_dispatch_message(
    struct crtgfx_host_window* host, struct crtgfx_wl_bootstrap* boot, uint32_t object_id,
    uint32_t opcode, const unsigned char* body, size_t body_len) {
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
  /* Anything else (wl_display::error/delete_id, wl_surface::enter/leave,
   * ...) is intentionally ignored. The message body has already been fully
   * consumed by wl_recv_message() either way, so ignoring it here is always
   * safe (never leaves the stream mis-aligned). */
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

    if (wl_recv_message(host, &object_id, &opcode, body, sizeof(body), &body_len, 0) != CRTGFX_OK) {
      break;
    }
    wl_dispatch_message(host, 0, object_id, opcode, body, body_len);

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

  rc = wl_connect(host);
  if (rc != CRTGFX_OK) {
    free(host);
    return rc;
  }

  memset(&boot, 0, sizeof(boot));
  boot.compositor_name = CRTGFX_WL_NAME_NONE;
  boot.shm_name = CRTGFX_WL_NAME_NONE;
  boot.wm_base_name = CRTGFX_WL_NAME_NONE;

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

    rc = wl_recv_message(host, &object_id, &opcode, rbody, sizeof(rbody), &rlen, CRTGFX_WL_TIMEOUT_MS);
    if (rc != CRTGFX_OK) {
      goto fail;
    }
    wl_dispatch_message(host, &boot, object_id, opcode, rbody, rlen);
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

    rc = wl_recv_message(host, &object_id, &opcode, rbody, sizeof(rbody), &rlen, CRTGFX_WL_TIMEOUT_MS);
    if (rc != CRTGFX_OK) {
      goto fail;
    }
    wl_dispatch_message(host, 0, object_id, opcode, rbody, rlen);
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
