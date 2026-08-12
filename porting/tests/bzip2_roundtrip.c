#include <stdio.h>
#include <string.h>

#include <bzlib.h>

static char input[128 * 1024];
static char compressed[256 * 1024];
static char decoded[128 * 1024 + 1024];

int main(void) {
  for (unsigned int i = 0; i < sizeof(input); ++i) {
    input[i] = (char)((i * 17u + (i >> 2) + "crt-bzip2"[i % 9]) & 0x7f);
  }

  unsigned int compressed_len = sizeof(compressed);
  int rc = BZ2_bzBuffToBuffCompress(compressed, &compressed_len,
                                    input, sizeof(input),
                                    9, 0, 30);
  if (rc != BZ_OK) {
    printf("bzip2_roundtrip_test: compress failed (%d)\n", rc);
    return 1;
  }
  if (compressed_len == 0 || compressed_len >= sizeof(compressed)) {
    printf("bzip2_roundtrip_test: invalid compressed size\n");
    return 1;
  }

  unsigned int decoded_len = sizeof(decoded);
  rc = BZ2_bzBuffToBuffDecompress(decoded, &decoded_len,
                                  compressed, compressed_len,
                                  0, 0);
  if (rc != BZ_OK) {
    printf("bzip2_roundtrip_test: decompress failed (%d)\n", rc);
    return 1;
  }
  if (decoded_len != sizeof(input) || memcmp(input, decoded, sizeof(input)) != 0) {
    printf("bzip2_roundtrip_test: data mismatch\n");
    return 1;
  }

  printf("bzip2_roundtrip_test: ok compressed=%u decoded=%u\n",
         compressed_len, decoded_len);
  return 0;
}
