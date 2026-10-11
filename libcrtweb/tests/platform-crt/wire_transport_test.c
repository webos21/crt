/* The frozen frame-producer/input wire contract over its real transport, without a browser, a window or Skia:
 * the header-only helpers crtweb_wire_send()/crtweb_wire_recv() (one datagram per message, SCM_RIGHTS for the
 * buffer descriptor) over an AF_UNIX SOCK_SEQPACKET pair, between two threads' worth of roles in one process
 * and then between two processes. This is the part of Tranche 3A that the consumer (`SurfaceClient`) and the
 * WPE `crt` display share; the Windows replay runs it over the CRT's own SOCK_SEQPACKET layer. */
#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "crtweb_surface_wire.h"

extern char** environ;

static int failures;

#define CHECK(condition, message)                                                                    \
  do {                                                                                               \
    if (!(condition)) {                                                                              \
      fprintf(stderr, "wire_transport_test: %s (line %d, errno %d)\n", message, __LINE__, errno);    \
      ++failures;                                                                                    \
    }                                                                                                \
  } while (0)

#define WIDTH 8u
#define HEIGHT 4u
#define STRIDE (WIDTH * 4u)

/* The producer's side of a short conversation: CONFIG after HELLO, one BUFFER_ADD with a memfd holding a
 * picture, one FRAME naming it, and a TITLE. Returns 0 on success. */
static int produce(int socket_fd) {
  unsigned char message[CRTWEB_WIRE_MAX_MESSAGE];
  int fd = -1;
  ssize_t got;
  crtweb_wire_config config;
  crtweb_wire_buffer_add add;
  crtweb_wire_frame frame;
  crtweb_wire_title title;
  unsigned char* pixels;
  unsigned i;
  int memfd;

  got = crtweb_wire_recv(socket_fd, message, sizeof(message), &fd);
  if (got != (ssize_t)sizeof(crtweb_wire_hello) || ((crtweb_wire_header*)message)->type != CRTWEB_C2P_HELLO) {
    return 1;
  }
  if (((crtweb_wire_hello*)message)->width != WIDTH || ((crtweb_wire_hello*)message)->height != HEIGHT) {
    return 2;
  }
  memset(&config, 0, sizeof(config));
  config.header.type = CRTWEB_P2C_CONFIG;
  config.header.size = sizeof(config);
  config.version = CRTWEB_WIRE_VERSION;
  config.pool_size = CRTWEB_POOL_SIZE;
  config.pixel_format = CRTWEB_PIXEL_ARGB8888;
  if (crtweb_wire_send(socket_fd, &config, sizeof(config), -1) != 0) {
    return 3;
  }

  memfd = memfd_create("crtweb-buffer", 0);
  if (memfd < 0 || ftruncate(memfd, STRIDE * HEIGHT) != 0) {
    return 4;
  }
  pixels = (unsigned char*)mmap(0, STRIDE * HEIGHT, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);
  if (pixels == MAP_FAILED) {
    return 5;
  }
  for (i = 0; i < STRIDE * HEIGHT; ++i) {
    pixels[i] = (unsigned char)(i * 7u + 3u);
  }
  memset(&add, 0, sizeof(add));
  add.header.type = CRTWEB_P2C_BUFFER_ADD;
  add.header.size = sizeof(add);
  add.buffer_id = 1;
  add.width = WIDTH;
  add.height = HEIGHT;
  add.stride = STRIDE;
  add.pixel_format = CRTWEB_PIXEL_ARGB8888;
  add.size = STRIDE * HEIGHT;
  if (crtweb_wire_send(socket_fd, &add, sizeof(add), memfd) != 0) {
    return 6;
  }
  memset(&frame, 0, sizeof(frame));
  frame.header.type = CRTWEB_P2C_FRAME;
  frame.header.size = sizeof(frame);
  frame.buffer_id = 1;
  frame.flags = CRTWEB_FRAME_FULL_DAMAGE;
  frame.serial = 1;
  frame.damage_serial = 1;
  frame.damage_width = (int32_t)WIDTH;
  frame.damage_height = (int32_t)HEIGHT;
  if (crtweb_wire_send(socket_fd, &frame, sizeof(frame), -1) != 0) {
    return 7;
  }
  memset(&title, 0, sizeof(title));
  title.header.type = CRTWEB_P2C_TITLE;
  title.header.size = sizeof(title);
  strcpy(title.utf8, "wire transport");
  if (crtweb_wire_send(socket_fd, &title, sizeof(title), -1) != 0) {
    return 8;
  }

  /* wait for the consumer's acknowledgement and release */
  got = crtweb_wire_recv(socket_fd, message, sizeof(message), &fd);
  if (got != (ssize_t)sizeof(crtweb_wire_frame_ack) || ((crtweb_wire_header*)message)->type != CRTWEB_C2P_FRAME_ACK ||
      ((crtweb_wire_frame_ack*)message)->serial != 1) {
    return 9;
  }
  got = crtweb_wire_recv(socket_fd, message, sizeof(message), &fd);
  if (got != (ssize_t)sizeof(crtweb_wire_buffer_release) ||
      ((crtweb_wire_header*)message)->type != CRTWEB_C2P_BUFFER_RELEASE) {
    return 10;
  }
  munmap(pixels, STRIDE * HEIGHT);
  close(memfd);
  return 0;
}

/* The consumer's side: HELLO, then read the whole conversation and verify the picture. */
static int consume(int socket_fd) {
  crtweb_wire_hello hello;
  unsigned char message[CRTWEB_WIRE_MAX_MESSAGE];
  int fd = -1;
  ssize_t got;
  unsigned char* pixels = 0;
  int buffer_fd = -1;
  int frames = 0;
  int titles = 0;
  int configs = 0;
  unsigned i;

  memset(&hello, 0, sizeof(hello));
  hello.header.type = CRTWEB_C2P_HELLO;
  hello.header.size = sizeof(hello);
  hello.version = CRTWEB_WIRE_VERSION;
  hello.width = WIDTH;
  hello.height = HEIGHT;
  hello.scale_percent = 100;
  CHECK(crtweb_wire_send(socket_fd, &hello, sizeof(hello), -1) == 0, "send HELLO");

  while (frames == 0 || titles == 0 || configs == 0) {
    const crtweb_wire_header* header;

    got = crtweb_wire_recv(socket_fd, message, sizeof(message), &fd);
    CHECK(got > 0, "receive a message");
    if (got <= 0) {
      return 1;
    }
    header = (const crtweb_wire_header*)message;
    if (header->type == CRTWEB_P2C_CONFIG) {
      const crtweb_wire_config* config = (const crtweb_wire_config*)message;

      ++configs;
      CHECK(got == (ssize_t)sizeof(*config) && config->version == CRTWEB_WIRE_VERSION &&
                config->pool_size == CRTWEB_POOL_SIZE && config->pixel_format == CRTWEB_PIXEL_ARGB8888,
            "CONFIG fields");
    } else if (header->type == CRTWEB_P2C_BUFFER_ADD) {
      const crtweb_wire_buffer_add* add = (const crtweb_wire_buffer_add*)message;

      CHECK(fd >= 0, "BUFFER_ADD carries a descriptor");
      CHECK(add->width == WIDTH && add->height == HEIGHT && add->stride == STRIDE && add->size == STRIDE * HEIGHT,
            "BUFFER_ADD geometry");
      if (fd >= 0) {
        buffer_fd = fd;
        pixels = (unsigned char*)mmap(0, (size_t)add->size, PROT_READ, MAP_SHARED, fd, 0);
        CHECK(pixels != MAP_FAILED, "map the received buffer");
        if (pixels == MAP_FAILED) {
          pixels = 0;
        }
      }
    } else if (header->type == CRTWEB_P2C_FRAME) {
      const crtweb_wire_frame* frame = (const crtweb_wire_frame*)message;

      ++frames;
      CHECK(frame->serial == 1 && frame->buffer_id == 1 && (frame->flags & CRTWEB_FRAME_FULL_DAMAGE) != 0,
            "FRAME fields");
      CHECK(pixels != 0, "the buffer arrived before the frame that names it");
      if (pixels != 0) {
        int intact = 1;

        for (i = 0; i < STRIDE * HEIGHT; ++i) {
          if (pixels[i] != (unsigned char)(i * 7u + 3u)) {
            intact = 0;
          }
        }
        CHECK(intact, "the mapped buffer holds the producer's picture");
      }
    } else if (header->type == CRTWEB_P2C_TITLE) {
      ++titles;
      CHECK(strcmp(((const crtweb_wire_title*)message)->utf8, "wire transport") == 0, "TITLE text");
    } else {
      CHECK(0, "unexpected message type");
      return 1;
    }
  }
  {
    crtweb_wire_frame_ack ack;
    crtweb_wire_buffer_release release;

    memset(&ack, 0, sizeof(ack));
    ack.header.type = CRTWEB_C2P_FRAME_ACK;
    ack.header.size = sizeof(ack);
    ack.serial = 1;
    CHECK(crtweb_wire_send(socket_fd, &ack, sizeof(ack), -1) == 0, "send FRAME_ACK");
    memset(&release, 0, sizeof(release));
    release.header.type = CRTWEB_C2P_BUFFER_RELEASE;
    release.header.size = sizeof(release);
    release.buffer_id = 1;
    CHECK(crtweb_wire_send(socket_fd, &release, sizeof(release), -1) == 0, "send BUFFER_RELEASE");
  }
  if (pixels != 0) {
    munmap(pixels, STRIDE * HEIGHT);
  }
  if (buffer_fd >= 0) {
    close(buffer_fd);
  }
  return failures != 0;
}

/* A datagram whose header size disagrees with its length is a protocol error, not data. */
static void test_protocol_error(void) {
  int sv[2];
  unsigned char message[CRTWEB_WIRE_MAX_MESSAGE];
  int fd = -1;
  crtweb_wire_header bogus;

  CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) == 0, "pair");
  if (sv[0] < 0) {
    return;
  }
  bogus.type = CRTWEB_C2P_BYE;
  bogus.size = 99; /* says 99, is 8 */
  CHECK(send(sv[0], &bogus, sizeof(bogus), 0) == (ssize_t)sizeof(bogus), "send a lying header");
  CHECK(crtweb_wire_recv(sv[1], message, sizeof(message), &fd) == -1 && errno == EPROTO,
        "a header whose size disagrees with the datagram is EPROTO");
  close(sv[0]);
  CHECK(crtweb_wire_recv(sv[1], message, sizeof(message), &fd) == 0, "end of stream is 0");
  close(sv[1]);
}

static int child_main(const char* path) {
  struct sockaddr_un address;
  int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);

  memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  strncpy(address.sun_path, path, sizeof(address.sun_path) - 1);
  if (sock < 0 || connect(sock, (const struct sockaddr*)&address, sizeof(address)) != 0) {
    return 20;
  }
  return produce(sock);
}

int main(int argc, char** argv) {
  int sv[2];

  if (argc > 2 && strcmp(argv[1], "producer") == 0) {
    return child_main(argv[2]);
  }
  test_protocol_error();

  /* Same process: the producer role runs on a thread-free schedule by interleaving: the consumer sends HELLO
   * first, so a single process can play both with the producer answering after each step. Easier and still
   * exact: fork-free role split over two processes below; here only the message codec is exercised. */
  CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) == 0, "pair");
  if (sv[0] >= 0) {
    crtweb_wire_hello hello;
    unsigned char message[CRTWEB_WIRE_MAX_MESSAGE];
    int fd = -1;

    memset(&hello, 0, sizeof(hello));
    hello.header.type = CRTWEB_C2P_HELLO;
    hello.header.size = sizeof(hello);
    hello.version = CRTWEB_WIRE_VERSION;
    CHECK(crtweb_wire_send(sv[0], &hello, sizeof(hello), -1) == 0, "send in one process");
    CHECK(crtweb_wire_recv(sv[1], message, sizeof(message), &fd) == (ssize_t)sizeof(hello) && fd == -1 &&
              ((crtweb_wire_header*)message)->type == CRTWEB_C2P_HELLO,
          "receive in the same process");
    close(sv[0]);
    close(sv[1]);
  }

  /* Two processes: this one listens (the consumer listens), a child connects and produces. */
  {
    char path[256];
    struct sockaddr_un address;
    int server;
    int accepted;
    pid_t pid;
    int status = 0;
    const char* base = getenv("TMPDIR") != 0 ? getenv("TMPDIR") : (getenv("TEMP") != 0 ? getenv("TEMP") : ".");
    char* const child_argv[] = {argv[0], (char*)"producer", path, 0};

    snprintf(path, sizeof(path), "%s/crtweb_wire_%d.sock", base, (int)getpid());
    unlink(path);
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, path, sizeof(address.sun_path) - 1);
    server = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    CHECK(server >= 0 && bind(server, (const struct sockaddr*)&address, sizeof(address)) == 0 &&
              listen(server, 1) == 0,
          "the consumer listens");
    if (server >= 0) {
      CHECK(posix_spawn(&pid, argv[0], 0, 0, child_argv, environ) == 0, "start the producer");
      accepted = accept(server, 0, 0);
      CHECK(accepted >= 0, "accept the producer");
      if (accepted >= 0) {
        CHECK(consume(accepted) == 0, "the conversation");
        close(accepted);
      }
      waitpid(pid, &status, 0);
      CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "the producer finished cleanly");
      close(server);
      unlink(path);
    }
  }

  if (failures != 0) {
    fprintf(stderr, "wire_transport_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("wire_transport_test: ok\n");
  return 0;
}
