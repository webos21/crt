// Deterministic, FFmpeg-free coverage for crtmedia/format.h -- set/get
// round trips for every value type, type-mismatch and missing-key error
// handling, and value replacement. No host resource needed.

#include "crtmedia/format.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg)                                                                     \
  do {                                                                                        \
    if (!(cond)) {                                                                            \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);                           \
      ++failures;                                                                             \
    }                                                                                          \
  } while (0)

int main(void) {
  crtmedia_format* format = NULL;
  crtmedia_result r = crtmedia_format_create(&format);
  CHECK(r == CRTMEDIA_OK, "crtmedia_format_create succeeds");
  CHECK(format != NULL, "crtmedia_format_create produces a real format");

  int32_t i32 = 0;
  CHECK(crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_WIDTH, &i32) == CRTMEDIA_ERROR_UNSUPPORTED,
        "getting an unset key fails with CRTMEDIA_ERROR_UNSUPPORTED");

  CHECK(crtmedia_format_set_int32(format, CRTMEDIA_FORMAT_KEY_WIDTH, 64) == CRTMEDIA_OK, "set_int32 succeeds");
  CHECK(crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_WIDTH, &i32) == CRTMEDIA_OK, "get_int32 succeeds");
  CHECK(i32 == 64, "get_int32 round-trips the real value");

  CHECK(crtmedia_format_set_int64(format, CRTMEDIA_FORMAT_KEY_DURATION_US, 1000000) == CRTMEDIA_OK,
        "set_int64 succeeds");
  int64_t i64 = 0;
  CHECK(crtmedia_format_get_int64(format, CRTMEDIA_FORMAT_KEY_DURATION_US, &i64) == CRTMEDIA_OK,
        "get_int64 succeeds");
  CHECK(i64 == 1000000, "get_int64 round-trips the real value");

  CHECK(crtmedia_format_set_string(format, CRTMEDIA_FORMAT_KEY_MIME, "video/avc") == CRTMEDIA_OK,
        "set_string succeeds");
  const char* mime = NULL;
  CHECK(crtmedia_format_get_string(format, CRTMEDIA_FORMAT_KEY_MIME, &mime) == CRTMEDIA_OK, "get_string succeeds");
  CHECK(mime != NULL && strcmp(mime, "video/avc") == 0, "get_string round-trips the real value");

  unsigned char csd[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  CHECK(crtmedia_format_set_buffer(format, CRTMEDIA_FORMAT_KEY_CSD, csd, sizeof(csd)) == CRTMEDIA_OK,
        "set_buffer succeeds");
  const void* csd_out = NULL;
  size_t csd_out_size = 0;
  CHECK(crtmedia_format_get_buffer(format, CRTMEDIA_FORMAT_KEY_CSD, &csd_out, &csd_out_size) == CRTMEDIA_OK,
        "get_buffer succeeds");
  CHECK(csd_out_size == sizeof(csd) && csd_out != NULL && memcmp(csd_out, csd, sizeof(csd)) == 0,
        "get_buffer round-trips the real bytes");

  // Type mismatch: CRTMEDIA_FORMAT_KEY_MIME was set as a string -- asking
  // for it as an int32 must fail cleanly, not read garbage.
  CHECK(crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_MIME, &i32) == CRTMEDIA_ERROR_UNSUPPORTED,
        "type-mismatched get fails with CRTMEDIA_ERROR_UNSUPPORTED");

  // Replacing a key's own value (even with a different type) must work
  // cleanly, matching crtmedia/format.h's own documented contract.
  CHECK(crtmedia_format_set_string(format, CRTMEDIA_FORMAT_KEY_WIDTH, "not-a-number") == CRTMEDIA_OK,
        "replacing an existing key's value succeeds");
  CHECK(crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_WIDTH, &i32) == CRTMEDIA_ERROR_UNSUPPORTED,
        "the replaced key no longer answers as its old type");
  const char* width_str = NULL;
  CHECK(crtmedia_format_get_string(format, CRTMEDIA_FORMAT_KEY_WIDTH, &width_str) == CRTMEDIA_OK &&
            strcmp(width_str, "not-a-number") == 0,
        "the replaced key answers as its new type with the new value");

  // Null-argument handling.
  CHECK(crtmedia_format_create(NULL) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "crtmedia_format_create(NULL) fails cleanly");
  CHECK(crtmedia_format_set_int32(NULL, CRTMEDIA_FORMAT_KEY_WIDTH, 1) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "set_int32 on a null format fails cleanly");
  CHECK(crtmedia_format_get_int32(format, NULL, &i32) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "get_int32 with a null key fails cleanly");

  crtmedia_format_release(format);
  crtmedia_format_release(NULL); // must be a real, safe no-op

  if (failures != 0) {
    fprintf(stderr, "crtmedia_format_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_format_test: ok\n");
  return 0;
}
