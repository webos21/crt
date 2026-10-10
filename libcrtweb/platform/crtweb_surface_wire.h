/* CRT web surface wire protocol, version 1 (Web Tranche 3A).
 *
 * The frozen contract between a browser *frame producer* (today: the WPEPlatform module in
 * libcrtweb/platform/wpe-crt, running in a native WPE UI process) and a *consumer* (a CRT
 * application that shows the frame as a crtui SurfaceView and feeds input back). Plain C, fixed-width
 * little-endian fields, no WebKit, GLib or CRT type: both worlds include this header unchanged.
 * The semantics are specified in docs/acceptance/crtweb_acceptance.md ("Frame producer and input
 * contracts"); the comments here are the field-level summary.
 *
 * Transport: one AF_UNIX SOCK_SEQPACKET connection; one message per datagram (a `crtweb_wire_header`
 * followed by its payload). BUFFER_ADD carries one file descriptor (SCM_RIGHTS) of a shared-memory
 * object (memfd). The consumer listens, the producer connects.
 *
 * Pixels: format CRTWEB_PIXEL_ARGB8888 = DRM_FORMAT_ARGB8888 ('AR24'): little-endian 32-bit words,
 * i.e. bytes B,G,R,A in memory, alpha premultiplied, origin top-left, row stride in bytes.
 */
#ifndef CRTWEB_SURFACE_WIRE_H
#define CRTWEB_SURFACE_WIRE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRTWEB_WIRE_VERSION 1u
#define CRTWEB_WIRE_MAX_MESSAGE 128u
#define CRTWEB_POOL_SIZE 3u
#define CRTWEB_PIXEL_ARGB8888 0x34325241u /* 'A','R','2','4' */

typedef enum crtweb_wire_type {
  /* consumer -> producer */
  CRTWEB_C2P_HELLO = 1,    /* crtweb_wire_hello: the first message */
  CRTWEB_C2P_RESIZE,       /* crtweb_wire_resize */
  CRTWEB_C2P_FRAME_ACK,    /* crtweb_wire_frame_ack: this frame was composed (presentation acknowledgement) */
  CRTWEB_C2P_BUFFER_RELEASE, /* crtweb_wire_buffer_release: the consumer no longer reads this buffer */
  CRTWEB_C2P_FOCUS,        /* crtweb_wire_focus */
  CRTWEB_C2P_POINTER,      /* crtweb_wire_pointer */
  CRTWEB_C2P_WHEEL,        /* crtweb_wire_wheel */
  CRTWEB_C2P_KEY,          /* crtweb_wire_key */
  CRTWEB_C2P_TEXT,         /* crtweb_wire_text */
  CRTWEB_C2P_BYE,
  /* producer -> consumer */
  CRTWEB_P2C_CONFIG = 0x100, /* crtweb_wire_config: the answer to HELLO */
  CRTWEB_P2C_BUFFER_ADD,     /* crtweb_wire_buffer_add + one fd */
  CRTWEB_P2C_BUFFER_REMOVE,  /* crtweb_wire_buffer_remove */
  CRTWEB_P2C_FRAME,          /* crtweb_wire_frame */
  CRTWEB_P2C_TITLE,          /* crtweb_wire_title: document title changed (UTF-8, truncated) */
  CRTWEB_P2C_BYE
} crtweb_wire_type;

typedef struct crtweb_wire_header {
  uint32_t type;
  uint32_t size; /* of the whole message, header included */
} crtweb_wire_header;

typedef struct crtweb_wire_hello {
  crtweb_wire_header header;
  uint32_t version;       /* CRTWEB_WIRE_VERSION */
  uint32_t width, height; /* logical pixels */
  uint32_t scale_percent; /* 100 = 1.0 */
} crtweb_wire_hello;

typedef struct crtweb_wire_config {
  crtweb_wire_header header;
  uint32_t version;
  uint32_t pool_size;     /* CRTWEB_POOL_SIZE */
  uint32_t pixel_format;  /* CRTWEB_PIXEL_ARGB8888 */
  uint32_t reserved;
} crtweb_wire_config;

typedef struct crtweb_wire_resize {
  crtweb_wire_header header;
  uint32_t width, height;
  uint32_t scale_percent;
  uint32_t reserved;
} crtweb_wire_resize;

typedef struct crtweb_wire_buffer_add {
  crtweb_wire_header header;
  uint32_t buffer_id;
  uint32_t width, height; /* device pixels */
  uint32_t stride;        /* bytes */
  uint32_t pixel_format;
  uint64_t size;          /* bytes mapped from the fd */
} crtweb_wire_buffer_add;

typedef struct crtweb_wire_buffer_remove {
  crtweb_wire_header header;
  uint32_t buffer_id;
  uint32_t reserved;
} crtweb_wire_buffer_remove;

/* A complete, immutable picture is in `buffer_id`. The consumer owns the buffer until it sends
 * BUFFER_RELEASE, and the producer never writes it in between. `damage` is the part that
 * differs from the previous FRAME (device pixels; the whole buffer when `flags` has FULL_DAMAGE);
 * it is advisory: the buffer is always a complete picture. */
#define CRTWEB_FRAME_FULL_DAMAGE 1u
typedef struct crtweb_wire_frame {
  crtweb_wire_header header;
  uint32_t buffer_id;
  uint32_t flags;
  uint64_t serial;        /* strictly increasing, starts at 1 */
  uint64_t damage_serial; /* strictly increasing with every frame that has damage */
  int32_t damage_x, damage_y, damage_width, damage_height;
  uint64_t time_ns;       /* producer CLOCK_MONOTONIC when the frame was committed */
} crtweb_wire_frame;

typedef struct crtweb_wire_frame_ack {
  crtweb_wire_header header;
  uint64_t serial;
} crtweb_wire_frame_ack;

typedef struct crtweb_wire_buffer_release {
  crtweb_wire_header header;
  uint32_t buffer_id;
  uint32_t reserved;
} crtweb_wire_buffer_release;

typedef struct crtweb_wire_title {
  crtweb_wire_header header;
  char utf8[96];
} crtweb_wire_title;

typedef struct crtweb_wire_focus {
  crtweb_wire_header header;
  uint32_t focused;
  uint32_t reserved;
} crtweb_wire_focus;

/* Modifier bits. Independent of any window system; the consumer maps its native flags. */
#define CRTWEB_MOD_CONTROL 1u
#define CRTWEB_MOD_SHIFT 2u
#define CRTWEB_MOD_ALT 4u
#define CRTWEB_MOD_META 8u
#define CRTWEB_MOD_CAPS_LOCK 16u

typedef enum crtweb_pointer_action {
  CRTWEB_POINTER_MOVE = 1,
  CRTWEB_POINTER_DOWN,
  CRTWEB_POINTER_UP,
  CRTWEB_POINTER_ENTER,
  CRTWEB_POINTER_LEAVE
} crtweb_pointer_action;

typedef enum crtweb_pointer_type { CRTWEB_POINTER_MOUSE = 1, CRTWEB_POINTER_PEN, CRTWEB_POINTER_TOUCH } crtweb_pointer_type;

/* x,y: logical pixels, view-local. button: 1 primary, 2 middle, 3 secondary (0 for MOVE/ENTER/LEAVE).
 * press_count: 1 single, 2 double, 3 triple click (DOWN only). pointer_id: 0 for the mouse. */
typedef struct crtweb_wire_pointer {
  crtweb_wire_header header;
  uint32_t action;
  uint32_t pointer_type;
  uint32_t pointer_id;
  uint32_t button;
  uint32_t press_count;
  uint32_t modifiers;
  double x, y;
  uint32_t time_ms;
  uint32_t reserved;
} crtweb_wire_pointer;

typedef struct crtweb_wire_wheel {
  crtweb_wire_header header;
  uint32_t modifiers;
  uint32_t time_ms;
  double x, y;
  double delta_x, delta_y; /* logical pixels, positive = content moves up/left as for a pixel scroll */
} crtweb_wire_wheel;

/* keyval: X11/XKB keysym (platform independent: 0x61 'a', 0xff0d Return, 0x01000000+cp for Unicode).
 * keycode: the hardware key as an XKB keycode, i.e. the Linux evdev code + 8 (KEY_A = 30 -> 38).
 * This is what WPE/WebKit take (they derive DOM `code` from it); a consumer on another platform maps
 * its native scan code to the evdev numbering first. 0 = unknown: WebKit then drops the event.
 * Both fields are carried because WebKit derives `key` from the keysym and `code` from the key code. */
typedef struct crtweb_wire_key {
  crtweb_wire_header header;
  uint32_t down; /* 1 press, 0 release */
  uint32_t repeat;
  uint32_t keyval;
  uint32_t keycode;
  uint32_t modifiers;
  uint32_t time_ms;
} crtweb_wire_key;

/* Committed text (one grapheme, UTF-8, NUL-terminated). */
typedef struct crtweb_wire_text {
  crtweb_wire_header header;
  char utf8[16];
} crtweb_wire_text;

/* ---- transport helpers (header-only) ------------------------------------------------------ */

/* Sends one message, optionally with one file descriptor. Returns 0, or -1 with errno set. */
static inline int crtweb_wire_send(int socket_fd, const void* message, size_t size, int fd) {
  struct iovec iov;
  struct msghdr msg;
  union {
    struct cmsghdr align;
    char bytes[CMSG_SPACE(sizeof(int))];
  } control;
  ssize_t sent;

  memset(&msg, 0, sizeof msg);
  iov.iov_base = (void*)message;
  iov.iov_len = size;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  if (fd >= 0) {
    struct cmsghdr* cmsg;
    memset(&control, 0, sizeof control);
    msg.msg_control = control.bytes;
    msg.msg_controllen = sizeof control.bytes;
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));
  }
  do {
    sent = sendmsg(socket_fd, &msg, MSG_NOSIGNAL);
  } while (sent < 0 && errno == EINTR);
  return sent == (ssize_t)size ? 0 : -1;
}

/* Receives one datagram into `buffer`. *out_fd is the received descriptor or -1. Returns the
 * byte count, 0 at end of stream, -1 on error (EAGAIN when non-blocking and empty). A message
 * whose header size disagrees with the datagram length is a protocol error: -1, EPROTO. */
static inline ssize_t crtweb_wire_recv(int socket_fd, void* buffer, size_t capacity, int* out_fd) {
  struct iovec iov;
  struct msghdr msg;
  union {
    struct cmsghdr align;
    char bytes[CMSG_SPACE(sizeof(int))];
  } control;
  ssize_t got;
  struct cmsghdr* cmsg;

  *out_fd = -1;
  memset(&msg, 0, sizeof msg);
  iov.iov_base = buffer;
  iov.iov_len = capacity;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control.bytes;
  msg.msg_controllen = sizeof control.bytes;
  do {
    got = recvmsg(socket_fd, &msg, MSG_CMSG_CLOEXEC);
  } while (got < 0 && errno == EINTR);
  if (got <= 0) {
    return got;
  }
  for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
      memcpy(out_fd, CMSG_DATA(cmsg), sizeof(int));
    }
  }
  if ((msg.msg_flags & MSG_TRUNC) != 0 || (size_t)got < sizeof(crtweb_wire_header) ||
      ((const crtweb_wire_header*)buffer)->size != (uint32_t)got) {
    errno = EPROTO;
    return -1;
  }
  return got;
}

#ifdef __cplusplus
}
#endif

#endif /* CRTWEB_SURFACE_WIRE_H */
