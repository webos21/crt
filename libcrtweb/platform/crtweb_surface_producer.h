/* CRT web surface protocol: a minimal frame *producer* (Web Tranche 3B, milestone B5).
 *
 * Header-only, plain C, no WebKit type. It is the producer half of the frozen contract in
 * crtweb_surface_wire.h for an embedder that already has its pixels in memory (the CRT port's UI
 * process copies WebKit's backing store out with WKViewCRTCopyPixels): connect to the consumer's
 * socket, answer HELLO/CONFIG, keep a pool of CRTWEB_POOL_SIZE memfd-backed buffers, send complete
 * frames, honour ACK pacing (one frame in flight, bounded wait) and BUFFER_RELEASE ownership.
 * Input messages from the consumer are counted and dropped here; B6 routes them to the page.
 *
 * Not thread-safe: one producer, one thread.
 */
#ifndef CRTWEB_SURFACE_PRODUCER_H
#define CRTWEB_SURFACE_PRODUCER_H

#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "crtweb_surface_wire.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct crtweb_producer_buffer {
  int fd;
  uint8_t* map;
  size_t size;
  uint32_t width, height, stride;
  int added;  /* BUFFER_ADD sent */
  int in_use; /* the consumer holds it (FRAME sent, RELEASE not yet received) */
} crtweb_producer_buffer;

typedef struct crtweb_producer {
  int socket_fd;
  crtweb_producer_buffer buffers[CRTWEB_POOL_SIZE];
  uint64_t serial;
  uint64_t damage_serial;
  uint64_t acked_serial;
  uint64_t last_frame_ns; /* when the last FRAME was sent */
  unsigned input_messages_dropped;
  uint32_t requested_width, requested_height; /* the consumer's last RESIZE */
  int bye;
  /* Called for every consumer message except FRAME_ACK and BUFFER_RELEASE (which the producer
   * handles itself): RESIZE, FOCUS, POINTER, WHEEL, KEY, TEXT, BYE. May be NULL. */
  void (*on_message)(void* user, const void* message);
  void* user;
} crtweb_producer;

static inline uint64_t crtweb_producer_now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* Handles one consumer message. */
static inline void crtweb_producer_handle(crtweb_producer* p, const void* data) {
  const crtweb_wire_header* header = (const crtweb_wire_header*)data;
  if (p->on_message != NULL && header->type != CRTWEB_C2P_FRAME_ACK && header->type != CRTWEB_C2P_BUFFER_RELEASE) {
    p->on_message(p->user, data);
  }
  switch (header->type) {
    case CRTWEB_C2P_FRAME_ACK:
      if (((const crtweb_wire_frame_ack*)data)->serial > p->acked_serial) {
        p->acked_serial = ((const crtweb_wire_frame_ack*)data)->serial;
      }
      break;
    case CRTWEB_C2P_BUFFER_RELEASE: {
      uint32_t id = ((const crtweb_wire_buffer_release*)data)->buffer_id;
      if (id < CRTWEB_POOL_SIZE) {
        p->buffers[id].in_use = 0;
      }
      break;
    }
    case CRTWEB_C2P_RESIZE:
      p->requested_width = ((const crtweb_wire_resize*)data)->width;
      p->requested_height = ((const crtweb_wire_resize*)data)->height;
      break;
    case CRTWEB_C2P_BYE:
      p->bye = 1;
      break;
    default:
      ++p->input_messages_dropped; /* HELLO again, FOCUS, POINTER, WHEEL, KEY, TEXT */
      break;
  }
}

/* Reads and handles consumer messages for up to `timeout_ms` (0 = only what is queued). Returns 0,
 * or -1 when the connection is gone. */
static inline int crtweb_producer_pump(crtweb_producer* p, int timeout_ms) {
  for (;;) {
    struct pollfd pfd;
    uint8_t message[CRTWEB_WIRE_MAX_MESSAGE];
    int fd = -1;
    ssize_t got;
    int ready;

    pfd.fd = p->socket_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    ready = poll(&pfd, 1, timeout_ms);
    if (ready < 0 && errno == EINTR) {
      continue;
    }
    if (ready <= 0) {
      return 0;
    }
    got = crtweb_wire_recv(p->socket_fd, message, sizeof message, &fd);
    if (fd >= 0) {
      close(fd);
    }
    if (got <= 0) {
      p->bye = 1;
      return -1;
    }
    crtweb_producer_handle(p, message);
    timeout_ms = 0; /* drain what is queued, then return */
  }
}

/* Connects to the consumer listening on `path` and completes HELLO -> CONFIG. 0, or -1.
 * `*p` must be zero-initialised by the caller (`crtweb_producer p = {0};`); `on_message` and `user`
 * may be set before the call and are kept. */
static inline int crtweb_producer_connect(crtweb_producer* p, const char* path, uint32_t width, uint32_t height,
                                          uint32_t scale_percent) {
  struct sockaddr_un address;
  crtweb_wire_hello hello;
  uint8_t message[CRTWEB_WIRE_MAX_MESSAGE];
  struct pollfd pfd;
  int fd = -1;
  unsigned i;

  {
    void (*on_message)(void*, const void*) = p->on_message;
    void* user = p->user;
    memset(p, 0, sizeof *p);
    p->on_message = on_message;
    p->user = user;
  }
  for (i = 0; i < CRTWEB_POOL_SIZE; ++i) {
    p->buffers[i].fd = -1;
  }
  if (strlen(path) >= sizeof address.sun_path) {
    return -1;
  }
  p->socket_fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
  if (p->socket_fd < 0) {
    return -1;
  }
  memset(&address, 0, sizeof address);
  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, path);
  if (connect(p->socket_fd, (struct sockaddr*)&address, sizeof address) != 0) {
    close(p->socket_fd);
    p->socket_fd = -1;
    return -1;
  }

  memset(&hello, 0, sizeof hello);
  hello.header.type = CRTWEB_C2P_HELLO;
  hello.header.size = sizeof hello;
  hello.version = CRTWEB_WIRE_VERSION;
  hello.width = width;
  hello.height = height;
  hello.scale_percent = scale_percent;
  /* The consumer speaks first in the contract: HELLO is consumer -> producer. A producer that
   * connects therefore waits for it, then answers CONFIG. */
  pfd.fd = p->socket_fd;
  pfd.events = POLLIN;
  pfd.revents = 0;
  if (poll(&pfd, 1, 5000) <= 0 || crtweb_wire_recv(p->socket_fd, message, sizeof message, &fd) <= 0) {
    return -1;
  }
  if (fd >= 0) {
    close(fd);
  }
  if (((crtweb_wire_header*)message)->type != CRTWEB_C2P_HELLO ||
      ((crtweb_wire_hello*)message)->version != CRTWEB_WIRE_VERSION) {
    return -1;
  }
  p->requested_width = ((crtweb_wire_hello*)message)->width;
  p->requested_height = ((crtweb_wire_hello*)message)->height;
  {
    crtweb_wire_config config;
    memset(&config, 0, sizeof config);
    config.header.type = CRTWEB_P2C_CONFIG;
    config.header.size = sizeof config;
    config.version = CRTWEB_WIRE_VERSION;
    config.pool_size = CRTWEB_POOL_SIZE;
    config.pixel_format = CRTWEB_PIXEL_ARGB8888;
    if (crtweb_wire_send(p->socket_fd, &config, sizeof config, -1) != 0) {
      return -1;
    }
  }
  (void)hello;
  return 0;
}

static inline int crtweb_producer_buffer_prepare(crtweb_producer* p, unsigned index, uint32_t width, uint32_t height,
                                                 uint32_t stride) {
  crtweb_producer_buffer* buffer = &p->buffers[index];
  size_t size = (size_t)stride * height;

  if (buffer->added && buffer->width == width && buffer->height == height && buffer->stride == stride) {
    return 0;
  }
  if (buffer->added) {
    crtweb_wire_buffer_remove remove;
    memset(&remove, 0, sizeof remove);
    remove.header.type = CRTWEB_P2C_BUFFER_REMOVE;
    remove.header.size = sizeof remove;
    remove.buffer_id = index;
    (void)crtweb_wire_send(p->socket_fd, &remove, sizeof remove, -1);
    munmap(buffer->map, buffer->size);
    close(buffer->fd);
    buffer->added = 0;
  }
  buffer->fd = memfd_create("crtweb-frame", MFD_CLOEXEC);
  if (buffer->fd < 0 || ftruncate(buffer->fd, (off_t)size) != 0) {
    return -1;
  }
  buffer->map = (uint8_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, buffer->fd, 0);
  if (buffer->map == MAP_FAILED) {
    buffer->map = NULL;
    return -1;
  }
  buffer->size = size;
  buffer->width = width;
  buffer->height = height;
  buffer->stride = stride;
  {
    crtweb_wire_buffer_add add;
    memset(&add, 0, sizeof add);
    add.header.type = CRTWEB_P2C_BUFFER_ADD;
    add.header.size = sizeof add;
    add.buffer_id = index;
    add.width = width;
    add.height = height;
    add.stride = stride;
    add.pixel_format = CRTWEB_PIXEL_ARGB8888;
    add.size = size;
    if (crtweb_wire_send(p->socket_fd, &add, sizeof add, buffer->fd) != 0) {
      return -1;
    }
  }
  buffer->added = 1;
  return 0;
}

/* Sends one complete picture (B, G, R, A premultiplied, `stride` bytes per row) as a FRAME with full
 * damage, preceded by the document title when it is non-NULL, without waiting.
 * Returns 0 when sent, 1 when it cannot be sent now (the previous frame has not been acknowledged
 * within the contract's 250 ms window, or the consumer holds every buffer of the pool: the caller keeps
 * the newest picture and retries -- latest wins), -1 on a connection or allocation failure. */
static inline int crtweb_producer_try_present(crtweb_producer* p, const void* pixels, uint32_t width, uint32_t height,
                                              uint32_t stride, const char* title) {
  unsigned index = CRTWEB_POOL_SIZE;
  unsigned i;
  crtweb_wire_frame frame;
  uint64_t now = crtweb_producer_now_ns();

  if (p->serial > p->acked_serial && now - p->last_frame_ns < 250ull * 1000000ull) {
    return 1;
  }
  for (i = 0; i < CRTWEB_POOL_SIZE; ++i) {
    if (!p->buffers[i].in_use) {
      index = i;
      break;
    }
  }
  if (index >= CRTWEB_POOL_SIZE) {
    return 1;
  }
  if (crtweb_producer_buffer_prepare(p, index, width, height, width * 4u) != 0) {
    return -1;
  }
  {
    const uint8_t* source = (const uint8_t*)pixels;
    uint8_t* target = p->buffers[index].map;
    uint32_t row;
    for (row = 0; row < height; ++row) {
      memcpy(target + (size_t)row * p->buffers[index].stride, source + (size_t)row * stride, (size_t)width * 4u);
    }
  }
  if (title != NULL) {
    crtweb_wire_title message;
    memset(&message, 0, sizeof message);
    message.header.type = CRTWEB_P2C_TITLE;
    message.header.size = sizeof message;
    strncpy(message.utf8, title, sizeof message.utf8 - 1);
    if (crtweb_wire_send(p->socket_fd, &message, sizeof message, -1) != 0) {
      return -1;
    }
  }
  memset(&frame, 0, sizeof frame);
  frame.header.type = CRTWEB_P2C_FRAME;
  frame.header.size = sizeof frame;
  frame.buffer_id = index;
  frame.flags = CRTWEB_FRAME_FULL_DAMAGE;
  frame.serial = ++p->serial;
  frame.damage_serial = ++p->damage_serial;
  frame.damage_x = 0;
  frame.damage_y = 0;
  frame.damage_width = (int32_t)width;
  frame.damage_height = (int32_t)height;
  frame.time_ns = now;
  p->last_frame_ns = now;
  p->buffers[index].in_use = 1;
  if (crtweb_wire_send(p->socket_fd, &frame, sizeof frame, -1) != 0) {
    p->buffers[index].in_use = 0;
    return -1;
  }
  return 0;
}

/* The document title alone (it changes without a new picture). 0, or -1. */
static inline int crtweb_producer_send_title(crtweb_producer* p, const char* title) {
  crtweb_wire_title message;
  memset(&message, 0, sizeof message);
  message.header.type = CRTWEB_P2C_TITLE;
  message.header.size = sizeof message;
  strncpy(message.utf8, title, sizeof message.utf8 - 1);
  return crtweb_wire_send(p->socket_fd, &message, sizeof message, -1);
}

/* Like crtweb_producer_try_present, but waits at most 250 ms for the previous frame's ACK and for a
 * free buffer (the contract's bounded wait). 0, or -1. */
static inline int crtweb_producer_present(crtweb_producer* p, const void* pixels, uint32_t width, uint32_t height,
                                          uint32_t stride, const char* title) {
  uint64_t deadline = crtweb_producer_now_ns() + 250ull * 1000000ull;
  for (;;) {
    int result = crtweb_producer_try_present(p, pixels, width, height, stride, title);
    if (result <= 0) {
      return result;
    }
    if (crtweb_producer_now_ns() >= deadline) {
      return -1;
    }
    if (crtweb_producer_pump(p, 10) != 0) {
      return -1;
    }
  }
}

/* Waits until the last frame sent is acknowledged. 0, or -1 on timeout or a lost connection. */
static inline int crtweb_producer_wait_ack(crtweb_producer* p, int timeout_ms) {
  uint64_t deadline = crtweb_producer_now_ns() + (uint64_t)timeout_ms * 1000000ull;
  while (p->acked_serial < p->serial) {
    if (crtweb_producer_now_ns() >= deadline || crtweb_producer_pump(p, 20) != 0) {
      return p->acked_serial >= p->serial ? 0 : -1;
    }
  }
  return 0;
}

/* Says BYE and keeps reading for `linger_ms` before closing, so a consumer that is still releasing
 * the buffers of frames it received (or acknowledging them) does not hit a closed socket. */
static inline void crtweb_producer_goodbye(crtweb_producer* p, int linger_ms);

static inline void crtweb_producer_close(crtweb_producer* p) {
  unsigned i;
  if (p->socket_fd >= 0) {
    crtweb_wire_header bye;
    memset(&bye, 0, sizeof bye);
    bye.type = CRTWEB_P2C_BYE;
    bye.size = sizeof bye;
    (void)crtweb_wire_send(p->socket_fd, &bye, sizeof bye, -1);
    close(p->socket_fd);
    p->socket_fd = -1;
  }
  for (i = 0; i < CRTWEB_POOL_SIZE; ++i) {
    if (p->buffers[i].map) {
      munmap(p->buffers[i].map, p->buffers[i].size);
    }
    if (p->buffers[i].fd >= 0) {
      close(p->buffers[i].fd);
    }
    p->buffers[i].map = NULL;
    p->buffers[i].fd = -1;
  }
}

static inline void crtweb_producer_goodbye(crtweb_producer* p, int linger_ms) {
  if (p->socket_fd >= 0) {
    crtweb_wire_header bye;
    uint64_t deadline = crtweb_producer_now_ns() + (uint64_t)linger_ms * 1000000ull;
    memset(&bye, 0, sizeof bye);
    bye.type = CRTWEB_P2C_BYE;
    bye.size = sizeof bye;
    (void)crtweb_wire_send(p->socket_fd, &bye, sizeof bye, -1);
    while (crtweb_producer_now_ns() < deadline) {
      if (crtweb_producer_pump(p, 20) != 0) {
        break;
      }
    }
  }
  crtweb_producer_close(p);
}

#ifdef __cplusplus
}
#endif

#endif /* CRTWEB_SURFACE_PRODUCER_H */
