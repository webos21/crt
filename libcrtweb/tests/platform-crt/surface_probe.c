/* CRT-built consumer of the web surface wire protocol (Web Tranche 3A acceptance).
 *
 * Plays the part of the CRT application's web-view adapter against the native WPE host: it listens on
 * a Unix socket, receives frames into memfd-backed shared memory, acknowledges and releases them,
 * checks the pixels, and drives the page with pointer, wheel, key, text and resize messages whose
 * effects it observes both in the title messages and in later frames. Plain C99 + POSIX: built with
 * the CRT toolchain, so it also exercises CRT's AF_UNIX/SCM_RIGHTS/memfd/mmap surface.
 */
#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "../../platform/crtweb_surface_wire.h"

typedef struct {
  uint32_t id;
  uint8_t* map;
  size_t size;
  uint32_t width, height, stride;
  int in_use;
} Buffer;

enum { MAX_BUFFERS = 16 };

static int connection = -1;
static Buffer buffers[MAX_BUFFERS];
static uint64_t last_serial;
static uint64_t last_damage_serial;
static const Buffer* current; /* the frame the consumer is showing */
static char title[96];
static unsigned title_changes;
static unsigned frames;
static unsigned ack_count;
static int failures;
static int bye;
static int config_seen;
static int ack_enabled = 1;
static unsigned unacked;

#define CHECK(condition, ...)                                   \
  do {                                                          \
    if (!(condition)) {                                         \
      fprintf(stderr, "surface_probe: FAILED line %d: ", __LINE__); \
      fprintf(stderr, __VA_ARGS__);                             \
      fprintf(stderr, "\n");                                    \
      ++failures;                                               \
    }                                                           \
  } while (0)

static double now(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void send_simple(uint32_t type, const void* body, size_t size) {
  uint8_t message[CRTWEB_WIRE_MAX_MESSAGE];
  crtweb_wire_header* header = (crtweb_wire_header*)message;
  memset(message, 0, sizeof message);
  memcpy(message, body, size);
  header->type = type;
  header->size = (uint32_t)size;
  if (crtweb_wire_send(connection, message, size, -1) != 0) {
    fprintf(stderr, "surface_probe: send type %u failed: %s\n", type, strerror(errno));
    ++failures;
  }
}

static Buffer* find_buffer(uint32_t id) {
  int i;
  for (i = 0; i < MAX_BUFFERS; ++i) {
    if (buffers[i].map && buffers[i].id == id) return &buffers[i];
  }
  return NULL;
}

static void release_buffer(Buffer* buffer) {
  crtweb_wire_buffer_release release;
  memset(&release, 0, sizeof release);
  release.buffer_id = buffer->id;
  send_simple(CRTWEB_C2P_BUFFER_RELEASE, &release, sizeof release);
  buffer->in_use = 0;
}

/* The consumer composes the frame (here: just remembers it), acknowledges it, and releases the
 * buffer it was showing before. */
static void show_frame(const crtweb_wire_frame* frame) {
  Buffer* buffer = find_buffer(frame->buffer_id);
  crtweb_wire_frame_ack ack;

  CHECK(buffer != NULL, "FRAME names unknown buffer %u", frame->buffer_id);
  if (buffer == NULL) return;
  CHECK(frame->serial > last_serial, "frame serial %llu not increasing (last %llu)",
        (unsigned long long)frame->serial, (unsigned long long)last_serial);
  CHECK(frame->damage_serial > last_damage_serial, "damage serial not increasing");
  last_serial = frame->serial;
  last_damage_serial = frame->damage_serial;
  if (current != NULL && current != buffer && ack_enabled) {
    release_buffer((Buffer*)current);
  }
  buffer->in_use = 1;
  current = buffer;
  ++frames;
  if (!ack_enabled) {
    ++unacked;
    return;
  }
  memset(&ack, 0, sizeof ack);
  ack.serial = frame->serial;
  send_simple(CRTWEB_C2P_FRAME_ACK, &ack, sizeof ack);
  ++ack_count;
}

static void handle(const void* data, int fd) {
  const crtweb_wire_header* header = (const crtweb_wire_header*)data;
  switch (header->type) {
    case CRTWEB_P2C_CONFIG: {
      const crtweb_wire_config* config = (const crtweb_wire_config*)data;
      CHECK(config->version == CRTWEB_WIRE_VERSION, "version");
      CHECK(config->pool_size == CRTWEB_POOL_SIZE, "pool size %u", config->pool_size);
      CHECK(config->pixel_format == CRTWEB_PIXEL_ARGB8888, "pixel format");
      config_seen = 1;
      break;
    }
    case CRTWEB_P2C_BUFFER_ADD: {
      const crtweb_wire_buffer_add* add = (const crtweb_wire_buffer_add*)data;
      int i;
      CHECK(fd >= 0, "BUFFER_ADD carried no descriptor");
      CHECK(add->pixel_format == CRTWEB_PIXEL_ARGB8888, "buffer format");
      CHECK(add->stride >= add->width * 4u && add->size >= (uint64_t)add->stride * add->height, "buffer geometry");
      for (i = 0; i < MAX_BUFFERS && fd >= 0; ++i) {
        if (buffers[i].map == NULL) {
          void* map = mmap(NULL, (size_t)add->size, PROT_READ, MAP_SHARED, fd, 0);
          CHECK(map != MAP_FAILED, "mmap of the shared buffer failed: %s", strerror(errno));
          if (map != MAP_FAILED) {
            buffers[i].map = (uint8_t*)map;
            buffers[i].size = (size_t)add->size;
            buffers[i].id = add->buffer_id;
            buffers[i].width = add->width;
            buffers[i].height = add->height;
            buffers[i].stride = add->stride;
          }
          break;
        }
      }
      if (fd >= 0) close(fd);
      return;
    }
    case CRTWEB_P2C_BUFFER_REMOVE: {
      const crtweb_wire_buffer_remove* remove = (const crtweb_wire_buffer_remove*)data;
      Buffer* buffer = find_buffer(remove->buffer_id);
      if (buffer != NULL) {
        if (current == buffer) current = NULL;
        munmap(buffer->map, buffer->size);
        memset(buffer, 0, sizeof *buffer);
      }
      break;
    }
    case CRTWEB_P2C_FRAME:
      show_frame((const crtweb_wire_frame*)data);
      break;
    case CRTWEB_P2C_TITLE: {
      const crtweb_wire_title* message = (const crtweb_wire_title*)data;
      strncpy(title, message->utf8, sizeof title - 1);
      title[sizeof title - 1] = '\0';
      ++title_changes;
      break;
    }
    case CRTWEB_P2C_BYE:
      bye = 1;
      break;
    default:
      CHECK(0, "unknown message type %u", header->type);
  }
  if (fd >= 0) close(fd);
}

/* Reads messages until `until` (a predicate) is true or the time runs out. */
typedef int (*predicate)(void);

static int pump(predicate until, double seconds) {
  double deadline = now() + seconds;
  for (;;) {
    struct pollfd pfd;
    double remaining;
    if (until != NULL && until()) return 1;
    remaining = deadline - now();
    if (remaining <= 0) return 0;
    pfd.fd = connection;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, (int)(remaining * 1000) + 1) > 0) {
      uint8_t buffer[CRTWEB_WIRE_MAX_MESSAGE];
      int fd = -1;
      ssize_t got = crtweb_wire_recv(connection, buffer, sizeof buffer, &fd);
      if (got <= 0) {
        if (got < 0 && errno == EAGAIN) continue;
        fprintf(stderr, "surface_probe: connection ended (%zd, %s)\n", got, got < 0 ? strerror(errno) : "eof");
        bye = 1;
        return until != NULL && until();
      }
      handle(buffer, fd);
    }
  }
}

/* ---- pixel helpers ------------------------------------------------------------------------ */

static void pixel(const Buffer* buffer, uint32_t x, uint32_t y, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) {
  const uint8_t* p = buffer->map + (size_t)y * buffer->stride + (size_t)x * 4u;
  *b = p[0]; *g = p[1]; *r = p[2]; *a = p[3];
}

static int is_color(const Buffer* buffer, uint32_t x, uint32_t y, int r, int g, int b) {
  uint8_t pr, pg, pb, pa;
  pixel(buffer, x, y, &pr, &pg, &pb, &pa);
  return abs(pr - r) <= 2 && abs(pg - g) <= 2 && abs(pb - b) <= 2 && pa == 255;
}

/* The fixture's quadrants, sampled at their centres whatever the size. */
static int quadrants_are(const Buffer* buffer, int ar, int ag, int ab) {
  uint32_t w = buffer->width, h = buffer->height;
  return is_color(buffer, w / 4, h / 4, ar, ag, ab) && is_color(buffer, 3 * w / 4, h / 4, 0, 255, 0) &&
         is_color(buffer, w / 4, 3 * h / 4, 0, 0, 255) && is_color(buffer, 3 * w / 4, 3 * h / 4, 255, 255, 0);
}

static int want_frames_target;
static int have_frames(void) { return (int)frames >= want_frames_target; }
static const char* want_title_prefix;
static int title_matches(void) { return strncmp(title, want_title_prefix, strlen(want_title_prefix)) == 0; }
static uint32_t want_width, want_height;
static int current_has_size(void) { return current != NULL && current->width == want_width && current->height == want_height; }
static int current_is_white_a(void) { return current != NULL && is_color(current, current->width / 4, current->height / 4, 255, 255, 255); }
static int got_bye(void) { return bye; }

static int wait_title(const char* prefix, double seconds) {
  want_title_prefix = prefix;
  return pump(title_matches, seconds);
}

static void pointer(uint32_t action, uint32_t button, double x, double y, uint32_t presses) {
  crtweb_wire_pointer p;
  memset(&p, 0, sizeof p);
  p.action = action;
  p.pointer_type = CRTWEB_POINTER_MOUSE;
  p.button = button;
  p.press_count = presses;
  p.x = x;
  p.y = y;
  send_simple(CRTWEB_C2P_POINTER, &p, sizeof p);
}

static void key(uint32_t down, uint32_t keyval, uint32_t keycode, uint32_t modifiers) {
  crtweb_wire_key k;
  memset(&k, 0, sizeof k);
  k.down = down;
  k.keyval = keyval;
  k.keycode = keycode;
  k.modifiers = modifiers;
  send_simple(CRTWEB_C2P_KEY, &k, sizeof k);
}

int main(int argc, char** argv) {
  struct sockaddr_un address;
  int listener;
  struct pollfd pfd;
  crtweb_wire_hello hello;
  double started = now();

  if (argc != 2 || strlen(argv[1]) >= sizeof address.sun_path) {
    fprintf(stderr, "usage: %s SOCKET_PATH\n", argv[0]);
    return 2;
  }
  unlink(argv[1]);
  listener = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
  memset(&address, 0, sizeof address);
  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, argv[1]);
  if (listener < 0 || bind(listener, (struct sockaddr*)&address, sizeof address) != 0 || listen(listener, 1) != 0) {
    fprintf(stderr, "surface_probe: cannot listen on %s: %s\n", argv[1], strerror(errno));
    return 1;
  }
  printf("SURFACE_PROBE_LISTENING %s\n", argv[1]);
  fflush(stdout);
  pfd.fd = listener;
  pfd.events = POLLIN;
  if (poll(&pfd, 1, 90 * 1000) <= 0) {
    fprintf(stderr, "surface_probe: no producer connected within 90 s\n");
    return 1;
  }
  connection = accept(listener, NULL, NULL);
  if (connection < 0) return 1;
  memset(&hello, 0, sizeof hello);
  hello.version = CRTWEB_WIRE_VERSION;
  hello.width = 640;
  hello.height = 480;
  hello.scale_percent = 100;
  send_simple(CRTWEB_C2P_HELLO, &hello, sizeof hello);

  /* 1. first frame: the page's four quadrants at the requested size. */
  want_frames_target = 1;
  CHECK(pump(have_frames, 60.0), "no frame within 60 s");
  CHECK(config_seen, "no CONFIG before the first frame");
  if (current != NULL) {
    CHECK(current->width == 640 && current->height == 480, "first frame is %ux%u", current->width, current->height);
    if (current->width == 640 && current->height == 480) {
      CHECK(quadrants_are(current, 255, 0, 0), "quadrant colours of the first frame are wrong");
    }
  }
  CHECK(pump(NULL, 1.0) == 0 || 1, "settle");
  printf("SURFACE_PROBE_FRAME1 serial=%llu\n", (unsigned long long)last_serial);

  /* 2. pointer: move, press, release -> the page reports a click and turns quadrant A white. */
  pointer(CRTWEB_POINTER_ENTER, 0, 100, 100, 0);
  pointer(CRTWEB_POINTER_MOVE, 0, 100, 100, 0);
  pointer(CRTWEB_POINTER_DOWN, 1, 100, 100, 1);
  CHECK(wait_title("crt|down|0|100,100", 15.0), "no 'down' report, last title '%s'", title);
  pointer(CRTWEB_POINTER_UP, 1, 100, 100, 0);
  CHECK(wait_title("crt|click|1|100,100", 15.0), "no 'click' report, last title '%s'", title);
  CHECK(pump(current_is_white_a, 15.0), "quadrant A did not turn white after the click");

  /* 3. keyboard: 'a' (keysym 0x61, XKB keycode 38 = evdev 30 + 8) and Shift+Tab-style modifiers reach the page. */
  key(1, 0x61, 38, 0);
  CHECK(wait_title("crt|key|a|KeyA", 15.0), "no 'a' key report, last title '%s'", title);
  key(0, 0x61, 38, 0);
  key(1, 0x62, 56, CRTWEB_MOD_SHIFT | CRTWEB_MOD_CONTROL);
  CHECK(wait_title("crt|key|b|KeyB|SC", 15.0), "no ctrl+shift 'b' report, last title '%s'", title);
  key(0, 0x62, 56, 0);

  /* 4. wheel. */
  {
    crtweb_wire_wheel wheel;
    memset(&wheel, 0, sizeof wheel);
    wheel.x = 200;
    wheel.y = 200;
    wheel.delta_y = 40;
    send_simple(CRTWEB_C2P_WHEEL, &wheel, sizeof wheel);
  }
  CHECK(wait_title("crt|wheel|", 15.0), "no wheel report, last title '%s'", title);

  /* 5. committed text: first into the page (a keydown report), then into a focused field (the
   * `input` report follows the keydown at once, so only the final one is waited for). */
  {
    crtweb_wire_text text;
    memset(&text, 0, sizeof text);
    strcpy(text.utf8, "q");
    send_simple(CRTWEB_C2P_TEXT, &text, sizeof text);
    CHECK(wait_title("crt|key|q|KeyQ", 15.0), "no keydown from committed text, last title '%s'", title);
    pointer(CRTWEB_POINTER_DOWN, 1, 100, 460, 1);
    pointer(CRTWEB_POINTER_UP, 1, 100, 460, 0);
    CHECK(wait_title("crt|click|1|100,460", 15.0), "no click on the field, last title '%s'", title);
    strcpy(text.utf8, "Z");
    send_simple(CRTWEB_C2P_TEXT, &text, sizeof text);
    CHECK(wait_title("crt|text|Z", 15.0), "the field received no text, last title '%s'", title);
  }

  /* 6. resize: new buffers of the new size, same page, quadrants proportional. */
  {
    crtweb_wire_resize resize;
    memset(&resize, 0, sizeof resize);
    resize.width = 320;
    resize.height = 240;
    resize.scale_percent = 100;
    send_simple(CRTWEB_C2P_RESIZE, &resize, sizeof resize);
  }
  want_width = 320;
  want_height = 240;
  CHECK(pump(current_has_size, 20.0), "no 320x240 frame after the resize");
  if (current != NULL && current->width == 320 && current->height == 240) {
    CHECK(quadrants_are(current, 255, 255, 255), "quadrant colours after the resize are wrong");
  }

  /* 7. a stalled consumer: no acknowledgement and no release, so the producer's pool of three runs
   * dry. Every press repaints a corner. The page must keep running (the producer stops waiting for
   * an acknowledgement after its bounded wait, and holds only the newest commit), and after the
   * consumer recovers the newest picture arrives and the old buffers are reusable. */
  {
    unsigned before = frames, i, held = 0;
    ack_enabled = 0;
    for (i = 0; i < 6; ++i) {
      char want[40];
      pointer(CRTWEB_POINTER_DOWN, 1, 50 + i, 50, 1);
      snprintf(want, sizeof want, "crt|down|0|%u,50", 50 + i);
      CHECK(wait_title(want, 15.0), "page frozen with the pool exhausted (press %u): last title '%s'", i, title);
      pointer(CRTWEB_POINTER_UP, 1, 50 + i, 50, 0);
      pump(NULL, 0.4); /* longer than the producer's acknowledgement wait */
    }
    for (i = 0; i < MAX_BUFFERS; ++i) {
      if (buffers[i].map && buffers[i].in_use) ++held;
    }
    printf("SURFACE_PROBE_STALLED frames=%u held_buffers=%u\n", frames - before, held);
    CHECK(held <= CRTWEB_POOL_SIZE, "the consumer holds %u buffers, more than the pool", held);
    ack_enabled = 1;
    for (i = 0; i < MAX_BUFFERS; ++i) {
      if (buffers[i].map && buffers[i].in_use && &buffers[i] != current) release_buffer(&buffers[i]);
    }
    {
      crtweb_wire_frame_ack ack;
      memset(&ack, 0, sizeof ack);
      ack.serial = last_serial;
      send_simple(CRTWEB_C2P_FRAME_ACK, &ack, sizeof ack);
    }
    before = frames;
    pointer(CRTWEB_POINTER_DOWN, 1, 70, 70, 1);
    CHECK(wait_title("crt|down|0|70,70", 15.0), "page does not recover after the stall");
    want_frames_target = (int)before + 1;
    CHECK(pump(have_frames, 15.0), "no new frame after the consumer recovered");
  }

  { crtweb_wire_header bye_message; memset(&bye_message, 0, sizeof bye_message); send_simple(CRTWEB_C2P_BYE, &bye_message, sizeof bye_message); }
  pump(got_bye, 5.0);
  printf("SURFACE_PROBE_SUMMARY frames=%u acks=%u titles=%u elapsed=%.1fs\n", frames, ack_count, title_changes, now() - started);
  if (failures != 0) {
    printf("surface_probe: %d check(s) failed\n", failures);
    return 1;
  }
  printf("surface_probe: ok\n");
  return 0;
}
