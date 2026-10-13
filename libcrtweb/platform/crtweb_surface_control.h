/* CRT web surface protocol: the control extension (Web Tranche 4).
 *
 * The frame/input contract of version 1 (crtweb_surface_wire.h) is frozen and unchanged. This header
 * adds the *page control* a WebView needs beside it -- navigation commands to the producer and load,
 * URL, history and script-result events back -- as two new message types. A v1 peer that does not know
 * them ignores them (both ends already skip unknown types), so the extension is additive and negotiated
 * implicitly: a consumer only sends COMMAND to a producer it launched knowing it is a crtweb host.
 *
 * Both new types carry the same fixed 128-byte message, a *blob chunk*: a code, a request id, flags and
 * one chunk of a UTF-8 argument of up to CRTWEB_BLOB_MAX_TOTAL bytes. A longer argument is sent as
 * consecutive chunks (offset, length, total) of the same code and id; the receiver appends in order.
 * An argument that is two strings (an HTML document and its base URL; a URL and an error message) is
 * their concatenation with one NUL between them. Plain C, fixed-width, no pointers.
 */
#ifndef CRTWEB_SURFACE_CONTROL_H
#define CRTWEB_SURFACE_CONTROL_H

#include <stdlib.h>

#include "crtweb_surface_wire.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CRTWEB_BLOB_CHUNK 96u
#define CRTWEB_BLOB_MAX_TOTAL (1024u * 1024u)

enum {
  CRTWEB_C2P_COMMAND = 0x20, /* crtweb_wire_blob, code = crtweb_command */
  CRTWEB_P2C_EVENT = 0x120   /* crtweb_wire_blob, code = crtweb_event */
};

typedef enum crtweb_command {
  CRTWEB_COMMAND_LOAD_URL = 1,  /* arg: url */
  CRTWEB_COMMAND_LOAD_HTML,     /* arg: html NUL base_url (the base may be empty) */
  CRTWEB_COMMAND_STOP,
  CRTWEB_COMMAND_RELOAD,
  CRTWEB_COMMAND_GO_BACK,
  CRTWEB_COMMAND_GO_FORWARD,
  CRTWEB_COMMAND_EVALUATE       /* arg: script; id: the request id the result event repeats */
} crtweb_command;

typedef enum crtweb_event {
  CRTWEB_EVENT_LOAD_STARTED = 1,   /* arg: url */
  CRTWEB_EVENT_LOAD_COMMITTED,     /* arg: url */
  CRTWEB_EVENT_LOAD_FINISHED,      /* arg: url */
  CRTWEB_EVENT_LOAD_FAILED,        /* arg: url NUL message */
  CRTWEB_EVENT_URL_CHANGED,        /* arg: url */
  CRTWEB_EVENT_HISTORY,            /* flags: bit 0 can go back, bit 1 can go forward */
  CRTWEB_EVENT_SCRIPT_RESULT,      /* id: the request id; flags bit 0: error; arg: the result string */
  CRTWEB_EVENT_PROCESS_TERMINATED  /* the page's Web process is gone; the page is blank until a load */
} crtweb_event;

typedef struct crtweb_wire_blob {
  crtweb_wire_header header;
  uint32_t code;
  uint32_t id;
  uint32_t flags;
  uint32_t total;  /* bytes of the whole argument */
  uint32_t offset; /* of this chunk */
  uint32_t length; /* of this chunk, <= CRTWEB_BLOB_CHUNK */
  char data[CRTWEB_BLOB_CHUNK];
} crtweb_wire_blob;

/* Sends one command/event with its argument (arg may be NULL with length 0), chunked as needed.
 * Returns 0, or -1 with errno set (EMSGSIZE for an argument over CRTWEB_BLOB_MAX_TOTAL). */
static inline int crtweb_blob_send(int socket_fd, uint32_t message_type, uint32_t code, uint32_t id, uint32_t flags,
                                   const char* arg, size_t length) {
  crtweb_wire_blob blob;
  size_t offset = 0;
  if (length > CRTWEB_BLOB_MAX_TOTAL) {
    errno = EMSGSIZE;
    return -1;
  }
  do {
    size_t chunk = length - offset;
    if (chunk > CRTWEB_BLOB_CHUNK) chunk = CRTWEB_BLOB_CHUNK;
    memset(&blob, 0, sizeof blob);
    blob.header.type = message_type;
    blob.header.size = (uint32_t)sizeof blob;
    blob.code = code;
    blob.id = id;
    blob.flags = flags;
    blob.total = (uint32_t)length;
    blob.offset = (uint32_t)offset;
    blob.length = (uint32_t)chunk;
    if (chunk) memcpy(blob.data, arg + offset, chunk);
    if (crtweb_wire_send(socket_fd, &blob, sizeof blob, -1) != 0) return -1;
    offset += chunk;
  } while (offset < length);
  return 0;
}

/* Receiver side: feed every blob chunk of one direction into one assembler. `complete` becomes
 * non-zero when the chunk finished an argument; code, id, flags and the bytes (not NUL-terminated by
 * the sender; a NUL is appended here) are then valid until the next feed. A chunk that does not
 * continue the argument in progress restarts it (offset 0) or is dropped. */
typedef struct crtweb_blob_assembler {
  char* data;
  size_t capacity;
  size_t length;
  uint32_t code;
  uint32_t id;
  uint32_t flags;
  uint32_t total;
  int active;
} crtweb_blob_assembler;

static inline void crtweb_blob_assembler_reset(crtweb_blob_assembler* a) {
  a->length = 0;
  a->total = 0;
  a->active = 0;
}

/* Returns 1 when an argument completed, 0 when more is needed, -1 for an invalid chunk. */
static inline int crtweb_blob_assembler_feed(crtweb_blob_assembler* a, const crtweb_wire_blob* blob) {
  if (blob->header.size != sizeof *blob || blob->length > CRTWEB_BLOB_CHUNK || blob->total > CRTWEB_BLOB_MAX_TOTAL ||
      blob->offset > blob->total || blob->length > blob->total - blob->offset)
    return -1;
  if (blob->offset == 0) {
    crtweb_blob_assembler_reset(a);
    a->active = 1;
    a->code = blob->code;
    a->id = blob->id;
    a->flags = blob->flags;
    a->total = blob->total;
  } else if (!a->active || blob->offset != a->length || blob->code != a->code || blob->id != a->id) {
    crtweb_blob_assembler_reset(a);
    return -1;
  }
  if (a->capacity < (size_t)blob->total + 1) {
    size_t wanted = (size_t)blob->total + 1;
    char* grown = (char*)realloc(a->data, wanted);
    if (!grown) {
      crtweb_blob_assembler_reset(a);
      return -1;
    }
    a->data = grown;
    a->capacity = wanted;
  }
  memcpy(a->data + blob->offset, blob->data, blob->length);
  a->length = (size_t)blob->offset + blob->length;
  if (a->length < a->total) return 0;
  a->data[a->length] = '\0';
  a->active = 0;
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* CRTWEB_SURFACE_CONTROL_H */
