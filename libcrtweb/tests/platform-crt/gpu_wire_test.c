#include "crtweb_gpu_wire.h"
#include <stdio.h>
#include <stdlib.h>

typedef char packet_size_is_256[sizeof(crtweb_gpu_packet) == 256 ? 1 : -1];
typedef char image_size_is_72[sizeof(crtgfx_shared_image_desc) == 72 ? 1 : -1];
typedef char serial_offset_is_16[offsetof(crtweb_gpu_packet, serial) == 16 ? 1 : -1];
typedef char payload_offset_is_40[offsetof(crtweb_gpu_packet, payload) == 40 ? 1 : -1];
static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "gpu_wire_test: FAILED line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

static crtweb_gpu_packet buffer_packet(void) {
  crtweb_gpu_packet p = crtweb_gpu_message(CRTWEB_GPU_BUFFER_ADD);
  p.generation = 1; p.buffer_id = 1;
  p.payload.image.version = CRTGFX_SHARED_IMAGE_VERSION;
  p.payload.image.backend = CRTGFX_GPU_BACKEND_VULKAN;
  p.payload.image.handle_kind = CRTGFX_SHARED_IMAGE_OPAQUE_FD;
  p.payload.image.format = CRTGFX_SHARED_IMAGE_BGRA8;
  p.payload.image.width = 64; p.payload.image.height = 32; p.payload.image.allocation_size = 8192;
  p.payload.image.device_uuid[0] = 1; p.payload.image.driver_uuid[0] = 2;
  return p;
}

/* Deliberately bypass send validation to exercise receiving malicious shapes. */
static void raw_send(int fd, const void* bytes, size_t size, const int* fds, size_t count) {
  struct msghdr msg; struct iovec iov;
  union { struct cmsghdr align; uint8_t bytes[CMSG_SPACE(12 * sizeof(int))]; } control;
  memset(&msg, 0, sizeof msg); memset(&control, 0, sizeof control);
  iov.iov_base = (void*)bytes; iov.iov_len = size; msg.msg_iov = &iov; msg.msg_iovlen = 1;
  if (count) {
    struct cmsghdr* c;
    msg.msg_control = control.bytes; msg.msg_controllen = CMSG_SPACE(count * sizeof(int));
    c = CMSG_FIRSTHDR(&msg); c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS;
    c->cmsg_len = CMSG_LEN(count * sizeof(int)); memcpy(CMSG_DATA(c), fds, count * sizeof(int));
  }
  CHECK(sendmsg(fd, &msg, MSG_NOSIGNAL) == (ssize_t)size);
}

static int descriptors(void) {
  int count = 0, fd;
  for (fd = 0; fd < 4096; ++fd) if (fcntl(fd, F_GETFD) >= 0) ++count;
  return count;
}

int main(void) {
  int sockets[2], original[12], out[CRTGFX_SHARED_IMAGE_HANDLE_COUNT];
  unsigned i;
  crtweb_gpu_packet p = buffer_packet(), received;
  CHECK(crtweb_gpu_packet_valid(&p, 5));
  CHECK(!crtweb_gpu_packet_valid(&p, 4) && !crtweb_gpu_packet_valid(&p, 6));
  for (i = 0; i < 14; ++i) {
    crtweb_gpu_packet bad = p;
    switch (i) {
      case 0: ++bad.version; break; case 1: bad.magic = 0; break;
      case 2: bad.payload.image.width = 0; break; case 3: bad.payload.image.height = 8193; break;
      case 4: bad.payload.image.reserved = 1; break; case 5: bad.buffer_id = 7; break;
      case 6: bad.generation = 0; break; case 7: bad.payload.image.memory_type = 32; break;
      case 8: bad.payload.image.allocation_size = 1; break; case 9: bad.payload.bytes[215] = 1; break;
      case 10: bad.payload.image.backend = CRTGFX_GPU_BACKEND_METAL; break;
      case 11: ++bad.payload.image.handle_kind; break;
      case 12: ++bad.payload.image.format; break;
      case 13: ++bad.payload.image.version; break;
    }
    CHECK(!crtweb_gpu_packet_valid(&bad, 5));
  }
  CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets) == 0);
  for (i = 0; i < 12; ++i) { original[i] = open("/dev/null", O_RDONLY); CHECK(original[i] >= 0); }
  CHECK(crtweb_gpu_send(sockets[0], &p, original, 5) == 0);
  CHECK(crtweb_gpu_recv(sockets[1], &received, out) == 1);
  CHECK(!memcmp(&p, &received, sizeof p));
  for (i = 0; i < 5; ++i) CHECK(out[i] >= 0 && (fcntl(out[i], F_GETFD) & FD_CLOEXEC));
  crtweb_gpu_close_handles(out);
  {
    int before = descriptors();
    for (i = 0; i < 8; ++i) {
      crtweb_gpu_packet bad = p;
      size_t count = 5, length = sizeof p;
      if (i == 0) ++bad.version;
      if (i == 1) count = 4;
      if (i == 2) count = 6;
      if (i == 3) count = 12;
      if (i == 4) --length;
      if (i == 5) bad.reserved = 1;
      if (i == 6) bad.type = CRTWEB_GPU_FRAME;
      if (i == 7) length = 0;
      raw_send(sockets[0], &bad, length, original, count);
      CHECK(crtweb_gpu_recv(sockets[1], &received, out) == -1 && errno == EPROTO);
      CHECK(descriptors() == before);
      { unsigned j; for (j = 0; j < 5; ++j) CHECK(out[j] == -1); }
    }
  }
  p = crtweb_gpu_message(CRTWEB_GPU_FRAME); p.serial = 7; p.generation = 2; p.buffer_id = 3;
  CHECK(crtweb_gpu_send(sockets[0], &p, NULL, 0) == 0);
  CHECK(crtweb_gpu_recv(sockets[1], &received, out) == 1 && received.serial == 7);
  for (i = 0; i < 12; ++i) close(original[i]);
  close(sockets[0]); close(sockets[1]);
  if (failures) return 1;
  puts("gpu_wire_test: ok layout=pass shape=pass five-fd=pass truncation=pass ownership=pass");
  return 0;
}
