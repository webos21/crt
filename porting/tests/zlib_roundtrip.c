#include <stdio.h>
#include <string.h>

#include <zlib.h>

static unsigned char input[128 * 1024];
static unsigned char compressed[256 * 1024];
static unsigned char decoded[128 * 1024 + 1024];

int main(void) {
  for (uLong i = 0; i < sizeof(input); ++i) {
    input[i] = (unsigned char)((i * 13u + (i >> 4) + "crt-zlib"[i % 8]) & 0xffu);
  }

  uLongf compressed_len = sizeof(compressed);
  int rc = compress2(compressed, &compressed_len, input, sizeof(input), Z_BEST_COMPRESSION);
  if (rc != Z_OK) {
    printf("zlib_roundtrip_test: compress failed (%d)\n", rc);
    return 1;
  }
  if (compressed_len == 0 || compressed_len >= sizeof(compressed)) {
    printf("zlib_roundtrip_test: invalid compressed size\n");
    return 1;
  }

  uLongf decoded_len = sizeof(decoded);
  rc = uncompress(decoded, &decoded_len, compressed, compressed_len);
  if (rc != Z_OK) {
    printf("zlib_roundtrip_test: uncompress failed (%d)\n", rc);
    return 1;
  }
  if (decoded_len != sizeof(input) || memcmp(input, decoded, sizeof(input)) != 0) {
    printf("zlib_roundtrip_test: data mismatch\n");
    return 1;
  }

  printf("zlib_roundtrip_test: ok compressed=%lu decoded=%lu version=%s\n",
         (unsigned long)compressed_len, (unsigned long)decoded_len, zlibVersion());
  return 0;
}
