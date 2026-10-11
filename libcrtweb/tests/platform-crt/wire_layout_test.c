/* The frozen version-1 frame-producer/input wire contract (libcrtweb/platform/crtweb_surface_wire.h) must
 * have the same size, field offsets and numbering on every host: both ends of a connection are built
 * separately (the WebKit side with the host compiler, the consumer with the CRT toolchain), and a pointer
 * or a long in a fixed-width struct would make Windows (LLP64) and Linux/macOS (LP64) disagree. No socket
 * is opened here, so it runs wherever the header compiles; the transport itself is Linux-only for now. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "crtweb_surface_wire.h"

static int failures;

#define CHECK(condition, message)                                                          \
  do {                                                                                     \
    if (!(condition)) {                                                                    \
      fprintf(stderr, "wire_layout_test: %s (line %d)\n", message, __LINE__);              \
      ++failures;                                                                          \
    }                                                                                      \
  } while (0)

#define SIZE(type, expected) CHECK(sizeof(type) == (expected), #type " size")
#define OFFSET(type, field, expected) CHECK(offsetof(type, field) == (expected), #type "." #field " offset")

int main(void) {
  CHECK(CRTWEB_WIRE_VERSION == 1u && CRTWEB_POOL_SIZE == 3u && CRTWEB_WIRE_MAX_MESSAGE == 128u,
        "contract constants");
  CHECK(CRTWEB_PIXEL_ARGB8888 == 0x34325241u, "pixel format is 'AR24'");

  CHECK(CRTWEB_C2P_HELLO == 1 && CRTWEB_C2P_RESIZE == 2 && CRTWEB_C2P_FRAME_ACK == 3 &&
            CRTWEB_C2P_BUFFER_RELEASE == 4 && CRTWEB_C2P_FOCUS == 5 && CRTWEB_C2P_POINTER == 6 &&
            CRTWEB_C2P_WHEEL == 7 && CRTWEB_C2P_KEY == 8 && CRTWEB_C2P_TEXT == 9 && CRTWEB_C2P_BYE == 10,
        "consumer-to-producer message numbers");
  CHECK(CRTWEB_P2C_CONFIG == 0x100 && CRTWEB_P2C_BUFFER_ADD == 0x101 && CRTWEB_P2C_BUFFER_REMOVE == 0x102 &&
            CRTWEB_P2C_FRAME == 0x103 && CRTWEB_P2C_TITLE == 0x104 && CRTWEB_P2C_BYE == 0x105,
        "producer-to-consumer message numbers");
  CHECK(CRTWEB_POINTER_MOVE == 1 && CRTWEB_POINTER_LEAVE == 5 && CRTWEB_POINTER_MOUSE == 1 &&
            CRTWEB_POINTER_TOUCH == 3,
        "pointer enumerations");
  CHECK(CRTWEB_MOD_CONTROL == 1u && CRTWEB_MOD_SHIFT == 2u && CRTWEB_MOD_ALT == 4u && CRTWEB_MOD_META == 8u &&
            CRTWEB_MOD_CAPS_LOCK == 16u && CRTWEB_FRAME_FULL_DAMAGE == 1u,
        "modifier and flag bits");

  SIZE(crtweb_wire_header, 8);
  SIZE(crtweb_wire_hello, 24);
  OFFSET(crtweb_wire_hello, version, 8);
  OFFSET(crtweb_wire_hello, scale_percent, 20);
  SIZE(crtweb_wire_config, 24);
  SIZE(crtweb_wire_resize, 24);
  SIZE(crtweb_wire_buffer_add, 40);
  OFFSET(crtweb_wire_buffer_add, stride, 20);
  OFFSET(crtweb_wire_buffer_add, size, 32);
  SIZE(crtweb_wire_buffer_remove, 16);
  SIZE(crtweb_wire_frame, 56);
  OFFSET(crtweb_wire_frame, serial, 16);
  OFFSET(crtweb_wire_frame, damage_serial, 24);
  OFFSET(crtweb_wire_frame, damage_x, 32);
  OFFSET(crtweb_wire_frame, time_ns, 48);
  SIZE(crtweb_wire_frame_ack, 16);
  OFFSET(crtweb_wire_frame_ack, serial, 8);
  SIZE(crtweb_wire_buffer_release, 16);
  SIZE(crtweb_wire_title, 104);
  SIZE(crtweb_wire_focus, 16);
  SIZE(crtweb_wire_pointer, 56);
  OFFSET(crtweb_wire_pointer, x, 32);
  OFFSET(crtweb_wire_pointer, time_ms, 48);
  SIZE(crtweb_wire_wheel, 48);
  OFFSET(crtweb_wire_wheel, delta_y, 40);
  SIZE(crtweb_wire_key, 32);
  OFFSET(crtweb_wire_key, keycode, 20);
  SIZE(crtweb_wire_text, 24);

  CHECK(sizeof(crtweb_wire_title) <= CRTWEB_WIRE_MAX_MESSAGE && sizeof(crtweb_wire_pointer) <= CRTWEB_WIRE_MAX_MESSAGE,
        "every message fits CRTWEB_WIRE_MAX_MESSAGE");

  if (failures != 0) {
    fprintf(stderr, "wire_layout_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("wire_layout_test: ok\n");
  return 0;
}
