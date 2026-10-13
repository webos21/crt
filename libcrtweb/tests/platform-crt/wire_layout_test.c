/* The frozen version-1 frame-producer/input wire contract (libcrtweb/platform/crtweb_surface_wire.h) must
 * have the same size, field offsets and numbering on every host: both ends of a connection are built
 * separately (the WebKit side with the host compiler, the consumer with the CRT toolchain), and a pointer
 * or a long in a fixed-width struct would make Windows (LLP64) and Linux/macOS (LP64) disagree. No socket
 * is opened here, so it runs wherever the header compiles; the transport itself is Linux-only for now. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "crtweb_surface_control.h"
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

  /* The control extension (Tranche 4): two new message types, one fixed 128-byte chunk layout. */
  CHECK(CRTWEB_C2P_COMMAND == 0x20 && CRTWEB_P2C_EVENT == 0x120 && CRTWEB_BLOB_CHUNK == 96u &&
            CRTWEB_BLOB_MAX_TOTAL == 1048576u,
        "control extension constants");
  CHECK(CRTWEB_COMMAND_LOAD_URL == 1 && CRTWEB_COMMAND_EVALUATE == 7 && CRTWEB_EVENT_LOAD_STARTED == 1 &&
            CRTWEB_EVENT_PROCESS_TERMINATED == 8 && CRTWEB_COMMAND_IME_SET_COMPOSITION == 8 &&
            CRTWEB_COMMAND_CLEAR_DATA == 13 && CRTWEB_EVENT_DOWNLOAD_REQUEST == 9 && CRTWEB_EVENT_DATA_CLEARED == 15,
        "command and event numbers");
  SIZE(crtweb_wire_blob, 128);
  OFFSET(crtweb_wire_blob, code, 8);
  OFFSET(crtweb_wire_blob, total, 20);
  OFFSET(crtweb_wire_blob, length, 28);
  OFFSET(crtweb_wire_blob, data, 32);
  {
    /* Chunking round trip over the assembler, without a socket: a 250-byte argument is 3 chunks. */
    crtweb_blob_assembler assembler = {0};
    crtweb_wire_blob chunk;
    char argument[251];
    size_t offset;
    int completed = 0;
    for (offset = 0; offset < 250; ++offset) argument[offset] = (char)('a' + offset % 26);
    argument[250] = '\0';
    for (offset = 0; offset < 250; offset += CRTWEB_BLOB_CHUNK) {
      size_t length = 250 - offset < CRTWEB_BLOB_CHUNK ? 250 - offset : CRTWEB_BLOB_CHUNK;
      memset(&chunk, 0, sizeof chunk);
      chunk.header.type = CRTWEB_C2P_COMMAND;
      chunk.header.size = sizeof chunk;
      chunk.code = CRTWEB_COMMAND_LOAD_URL;
      chunk.id = 9;
      chunk.total = 250;
      chunk.offset = (uint32_t)offset;
      chunk.length = (uint32_t)length;
      memcpy(chunk.data, argument + offset, length);
      completed = crtweb_blob_assembler_feed(&assembler, &chunk);
      CHECK(completed == (offset + length == 250 ? 1 : 0), "assembler completes on the last chunk");
    }
    CHECK(assembler.length == 250 && memcmp(assembler.data, argument, 251) == 0 && assembler.id == 9,
          "assembled argument");
    chunk.offset = 7; /* does not continue anything */
    CHECK(crtweb_blob_assembler_feed(&assembler, &chunk) == -1, "a stray chunk is rejected");
    free(assembler.data);
  }

  if (failures != 0) {
    fprintf(stderr, "wire_layout_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("wire_layout_test: ok\n");
  return 0;
}
