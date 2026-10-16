#ifndef CRTWEB_GPU_WIRE_H
#define CRTWEB_GPU_WIRE_H

/* Separate GPU resource channel, version 2. NEVER a reinterpretation of the
 * v1 CPU memfd protocol. Fixed little-endian packets, current CRT LE targets.
 * A successful GPU CONFIG negotiates five reference-owned native resource
 * descriptors per immutable BUFFER_ADD. ACK is pacing; RELEASE authorizes
 * reuse only after the producer waits for the imported native release fence.
 * Only a validated FRAME epoch may consume its corresponding ready fence.
 */
#include "crtgfx/shared_image.h"
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CRTWEB_GPU_MAGIC 0x32475043u
#define CRTWEB_GPU_VERSION 2u
#define CRTWEB_GPU_MAX_BUFFERS 6u
#define CRTWEB_GPU_POOL_SIZE 3u
#define CRTWEB_GPU_MODE_SHARED 1u
#define CRTWEB_GPU_MODE_CPU 2u
#define CRTWEB_GPU_REQUIRE_SHARED 1u

enum crtweb_gpu_message_type {
  CRTWEB_GPU_HELLO = 1, CRTWEB_GPU_CONFIG, CRTWEB_GPU_BUFFER_ADD,
  CRTWEB_GPU_FRAME, CRTWEB_GPU_ACK, CRTWEB_GPU_RELEASE,
  CRTWEB_GPU_BUFFER_REMOVE, CRTWEB_GPU_COMMAND, CRTWEB_GPU_BYE, CRTWEB_GPU_ERROR
};
enum crtweb_gpu_command_type {
  CRTWEB_GPU_EVALUATE = 1, CRTWEB_GPU_RESIZE = 2
};
typedef struct crtweb_gpu_packet {
  uint32_t magic, version, type, size;
  uint64_t serial;
  uint32_t generation, buffer_id, flags, reserved;
  union {
    crtgfx_shared_image_desc image;
    struct { uint32_t code, length; char text[192]; } command;
    uint8_t bytes[216];
  } payload;
} crtweb_gpu_packet;

static inline crtweb_gpu_packet crtweb_gpu_message(uint32_t type) {
  crtweb_gpu_packet packet;
  memset(&packet, 0, sizeof packet);
  packet.magic = CRTWEB_GPU_MAGIC; packet.version = CRTWEB_GPU_VERSION;
  packet.type = type; packet.size = sizeof packet;
  return packet;
}

static inline int crtweb_gpu_packet_valid(const crtweb_gpu_packet* p, size_t fd_count) {
  size_t used = 0, i;
  if (!p || p->magic != CRTWEB_GPU_MAGIC || p->version != CRTWEB_GPU_VERSION
      || p->size != sizeof(*p) || p->reserved) return 0;
  if (p->type == CRTWEB_GPU_BUFFER_ADD) {
    /* The descriptor says how many native references this backend sends: five for
     * Vulkan, three for D3D12. A count that differs is a malformed packet. */
    if (!crtgfx_shared_image_descriptor_valid(&p->payload.image)
        || fd_count != crtgfx_shared_image_handle_count(&p->payload.image)
        || !p->generation || !p->buffer_id
        || p->buffer_id > CRTWEB_GPU_MAX_BUFFERS || p->serial || p->flags) return 0;
    used = sizeof p->payload.image;
  } else {
    if (fd_count) return 0;
    switch (p->type) {
      case CRTWEB_GPU_HELLO:
        if (p->serial || p->generation || p->buffer_id || p->flags > CRTWEB_GPU_REQUIRE_SHARED
            || !p->payload.image.width || !p->payload.image.height
            || p->payload.image.width > CRTGFX_SHARED_IMAGE_MAX_EXTENT
            || p->payload.image.height > CRTGFX_SHARED_IMAGE_MAX_EXTENT
            || p->payload.image.reserved || p->payload.image.allocation_size || p->payload.image.memory_type) return 0;
        used = sizeof p->payload.image;
        break;
      case CRTWEB_GPU_CONFIG:
        if (p->serial || p->generation || p->buffer_id
            || (p->flags != CRTWEB_GPU_MODE_SHARED && p->flags != CRTWEB_GPU_MODE_CPU)
            || p->payload.image.width || p->payload.image.height || p->payload.image.memory_type
            || p->payload.image.reserved || p->payload.image.allocation_size) return 0;
        used = sizeof p->payload.image;
        break;
      case CRTWEB_GPU_FRAME: case CRTWEB_GPU_ACK: case CRTWEB_GPU_RELEASE:
        if (!p->serial || !p->generation || !p->buffer_id || p->buffer_id > CRTWEB_GPU_MAX_BUFFERS || p->flags) return 0;
        break;
      case CRTWEB_GPU_BUFFER_REMOVE:
        if (!p->generation || !p->buffer_id || p->buffer_id > CRTWEB_GPU_MAX_BUFFERS || p->serial || p->flags) return 0;
        break;
      case CRTWEB_GPU_COMMAND:
        if (p->generation || p->buffer_id || p->flags || p->serial
            || (p->payload.command.code != CRTWEB_GPU_EVALUATE && p->payload.command.code != CRTWEB_GPU_RESIZE)
            || p->payload.command.length >= sizeof p->payload.command.text
            || p->payload.command.text[p->payload.command.length]
            || memchr(p->payload.command.text, 0, p->payload.command.length)) return 0;
        used = offsetof(crtweb_gpu_packet, payload.command.text) - offsetof(crtweb_gpu_packet, payload)
            + p->payload.command.length + 1;
        break;
      case CRTWEB_GPU_BYE: case CRTWEB_GPU_ERROR:
        if (p->serial || p->generation || p->buffer_id || p->flags) return 0;
        break;
      default: return 0;
    }
  }
  for (i = used; i < sizeof p->payload; ++i) if (p->payload.bytes[i]) return 0;
  return 1;
}

static inline void crtweb_gpu_close_handles(int fds[CRTGFX_SHARED_IMAGE_HANDLE_COUNT]) {
  size_t i;
  for (i = 0; i < CRTGFX_SHARED_IMAGE_HANDLE_COUNT; ++i) {
    if (fds[i] >= 0) close(fds[i]); fds[i] = -1;
  }
}

/* Handles: file descriptors; on Windows the descriptors of adopted NT handles (the CRT's
 * SCM_RIGHTS duplicates them into the peer). Send borrows handles. Receive owns its returned handles, setting CLOEXEC.
 * Any invalid/truncated packet closes ALL received handles before returning. */
static inline int crtweb_gpu_send(int fd, const crtweb_gpu_packet* p, const int* fds, size_t count) {
  struct msghdr msg;
  struct iovec iov;
  union { struct cmsghdr align; uint8_t bytes[CMSG_SPACE(CRTGFX_SHARED_IMAGE_HANDLE_COUNT * sizeof(int))]; } control;
  if (!crtweb_gpu_packet_valid(p, count) || (count && !fds)) { errno = EINVAL; return -1; }
  memset(&msg, 0, sizeof msg); memset(&control, 0, sizeof control);
  iov.iov_base = (void*)p; iov.iov_len = sizeof *p; msg.msg_iov = &iov; msg.msg_iovlen = 1;
  if (count) {
    struct cmsghdr* c;
    msg.msg_control = control.bytes; msg.msg_controllen = CMSG_SPACE(count * sizeof(int));
    c = CMSG_FIRSTHDR(&msg); c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS;
    c->cmsg_len = CMSG_LEN(count * sizeof(int)); memcpy(CMSG_DATA(c), fds, count * sizeof(int));
  }
  return sendmsg(fd, &msg, MSG_NOSIGNAL) == (ssize_t)sizeof *p ? 0 : -1;
}

static inline int crtweb_gpu_recv(int fd, crtweb_gpu_packet* p, int out_fds[CRTGFX_SHARED_IMAGE_HANDLE_COUNT]) {
  struct msghdr msg;
  struct iovec iov;
  struct cmsghdr* c;
  /* One extra handle lets us detect surplus descriptors without losing the
   * delivered ones; MSG_CTRUNC rejects still larger ancillary payloads. */
  union { struct cmsghdr align; uint8_t bytes[CMSG_SPACE((CRTGFX_SHARED_IMAGE_HANDLE_COUNT + 1) * sizeof(int))]; } control;
  int received[CRTGFX_SHARED_IMAGE_HANDLE_COUNT + 1];
  size_t count = 0, i;
  int valid = 1;
  ssize_t n;
  for (i = 0; i < CRTGFX_SHARED_IMAGE_HANDLE_COUNT; ++i) out_fds[i] = -1;
  memset(&msg, 0, sizeof msg); memset(&control, 0, sizeof control); memset(p, 0, sizeof *p);
  iov.iov_base = p; iov.iov_len = sizeof *p; msg.msg_iov = &iov; msg.msg_iovlen = 1;
  msg.msg_control = control.bytes; msg.msg_controllen = sizeof control.bytes;
  do { n = recvmsg(fd, &msg, 0); } while (n < 0 && errno == EINTR);
  if (n < 0) return (int)n;
  if (!n && !msg.msg_controllen) return 0;
  for (c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
    if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS || c->cmsg_len < CMSG_LEN(0)) { valid = 0; continue; }
    size_t bytes = c->cmsg_len - CMSG_LEN(0), handles = bytes / sizeof(int), k;
    if (bytes % sizeof(int)) valid = 0;
    for (k = 0; k < handles; ++k) {
      int handle; memcpy(&handle, (const uint8_t*)CMSG_DATA(c) + k * sizeof(int), sizeof handle);
      if (count < CRTGFX_SHARED_IMAGE_HANDLE_COUNT + 1) received[count++] = handle;
      else { close(handle); valid = 0; }
    }
  }
  valid = valid && n == (ssize_t)sizeof *p && !(msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC))
      && crtweb_gpu_packet_valid(p, count);
  for (i = 0; valid && i < count; ++i) if (fcntl(received[i], F_SETFD, FD_CLOEXEC) < 0) valid = 0;
  if (!valid) {
    for (i = 0; i < count; ++i) close(received[i]);
    errno = EPROTO; return -1;
  }
  for (i = 0; i < count; ++i) out_fds[i] = received[i];
  return 1;
}
#endif
