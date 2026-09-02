/* crtmedia/format.h -- see that header's own top comment for the design
 * reasoning. A real container/codec's own format never needs more than a
 * handful of keys (mime, width/height or sample-rate/channel-count,
 * duration-us, maybe a couple more later) -- CRTMEDIA_FORMAT_MAX_ENTRIES
 * below is a real, deliberate, small fixed limit rather than a growable
 * allocation, matching this file's own small/simple/deterministic-memory
 * design intent; crtmedia_format_set_*() returns a real, defined error
 * (CRTMEDIA_ERROR_UNSUPPORTED) rather than silently dropping a key if a
 * caller ever needs more than that. */

#include "crtmedia/format.h"

#include <stdlib.h>
#include <string.h>

#define CRTMEDIA_FORMAT_MAX_ENTRIES 32
#define CRTMEDIA_FORMAT_MAX_KEY_LEN 32
#define CRTMEDIA_FORMAT_MAX_STRING_LEN 128
/* Generous for real H.264 SPS/PPS (avcC) or AAC AudioSpecificConfig
 * codec-config data -- typically well under 256 bytes in practice for
 * this pass's own narrow codec set -- not sized for arbitrary payloads;
 * see crtmedia/format.h's own CRTMEDIA_FORMAT_KEY_CSD comment. */
#define CRTMEDIA_FORMAT_MAX_BUFFER_LEN 512

typedef enum crtmedia_format_value_type {
  CRTMEDIA_FORMAT_VALUE_INT32,
  CRTMEDIA_FORMAT_VALUE_INT64,
  CRTMEDIA_FORMAT_VALUE_STRING,
  CRTMEDIA_FORMAT_VALUE_BUFFER,
} crtmedia_format_value_type;

typedef struct crtmedia_format_buffer_value {
  unsigned char data[CRTMEDIA_FORMAT_MAX_BUFFER_LEN];
  size_t size;
} crtmedia_format_buffer_value;

typedef struct crtmedia_format_entry {
  char key[CRTMEDIA_FORMAT_MAX_KEY_LEN];
  crtmedia_format_value_type type;
  union {
    int32_t i32;
    int64_t i64;
    char str[CRTMEDIA_FORMAT_MAX_STRING_LEN];
    crtmedia_format_buffer_value buf;
  } value;
} crtmedia_format_entry;

struct crtmedia_format {
  crtmedia_format_entry entries[CRTMEDIA_FORMAT_MAX_ENTRIES];
  uint32_t entry_count;
};

crtmedia_result crtmedia_format_create(crtmedia_format** out_format) {
  if (out_format == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  crtmedia_format* format = (crtmedia_format*)calloc(1, sizeof(crtmedia_format));
  if (format == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  *out_format = format;
  return CRTMEDIA_OK;
}

void crtmedia_format_release(crtmedia_format* format) {
  free(format);
}

static crtmedia_format_entry* find_entry(crtmedia_format* format, const char* key) {
  for (uint32_t i = 0; i < format->entry_count; ++i) {
    if (strcmp(format->entries[i].key, key) == 0) {
      return &format->entries[i];
    }
  }
  return NULL;
}

static crtmedia_result set_entry(
    crtmedia_format* format, const char* key, crtmedia_format_value_type type, const void* value,
    size_t value_size) {
  if (format == NULL || key == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (strlen(key) >= CRTMEDIA_FORMAT_MAX_KEY_LEN) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  crtmedia_format_entry* entry = find_entry(format, key);
  if (entry == NULL) {
    if (format->entry_count >= CRTMEDIA_FORMAT_MAX_ENTRIES) {
      return CRTMEDIA_ERROR_UNSUPPORTED;
    }
    entry = &format->entries[format->entry_count++];
    strcpy(entry->key, key); /* NOLINT: length already checked above */
  }
  entry->type = type;
  memcpy(&entry->value, value, value_size);
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_format_set_int32(crtmedia_format* format, const char* key, int32_t value) {
  return set_entry(format, key, CRTMEDIA_FORMAT_VALUE_INT32, &value, sizeof(value));
}

crtmedia_result crtmedia_format_set_int64(crtmedia_format* format, const char* key, int64_t value) {
  return set_entry(format, key, CRTMEDIA_FORMAT_VALUE_INT64, &value, sizeof(value));
}

crtmedia_result crtmedia_format_set_string(crtmedia_format* format, const char* key, const char* value) {
  if (value == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (strlen(value) >= CRTMEDIA_FORMAT_MAX_STRING_LEN) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  char buffer[CRTMEDIA_FORMAT_MAX_STRING_LEN];
  strcpy(buffer, value); /* NOLINT: length already checked above */
  return set_entry(format, key, CRTMEDIA_FORMAT_VALUE_STRING, buffer, sizeof(buffer));
}

crtmedia_result crtmedia_format_set_buffer(crtmedia_format* format, const char* key, const void* data, size_t size) {
  if (data == NULL && size > 0) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (size > CRTMEDIA_FORMAT_MAX_BUFFER_LEN) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  crtmedia_format_buffer_value buf;
  buf.size = size;
  if (size > 0) {
    memcpy(buf.data, data, size);
  }
  return set_entry(format, key, CRTMEDIA_FORMAT_VALUE_BUFFER, &buf, sizeof(buf));
}

crtmedia_result crtmedia_format_get_int32(const crtmedia_format* format, const char* key, int32_t* out_value) {
  if (format == NULL || key == NULL || out_value == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  const crtmedia_format_entry* entry = find_entry((crtmedia_format*)format, key);
  if (entry == NULL || entry->type != CRTMEDIA_FORMAT_VALUE_INT32) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  *out_value = entry->value.i32;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_format_get_int64(const crtmedia_format* format, const char* key, int64_t* out_value) {
  if (format == NULL || key == NULL || out_value == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  const crtmedia_format_entry* entry = find_entry((crtmedia_format*)format, key);
  if (entry == NULL || entry->type != CRTMEDIA_FORMAT_VALUE_INT64) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  *out_value = entry->value.i64;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_format_get_string(const crtmedia_format* format, const char* key, const char** out_value) {
  if (format == NULL || key == NULL || out_value == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  const crtmedia_format_entry* entry = find_entry((crtmedia_format*)format, key);
  if (entry == NULL || entry->type != CRTMEDIA_FORMAT_VALUE_STRING) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  *out_value = entry->value.str;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_format_get_buffer(
    const crtmedia_format* format, const char* key, const void** out_data, size_t* out_size) {
  if (format == NULL || key == NULL || out_data == NULL || out_size == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  const crtmedia_format_entry* entry = find_entry((crtmedia_format*)format, key);
  if (entry == NULL || entry->type != CRTMEDIA_FORMAT_VALUE_BUFFER) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  *out_data = entry->value.buf.data;
  *out_size = entry->value.buf.size;
  return CRTMEDIA_OK;
}
