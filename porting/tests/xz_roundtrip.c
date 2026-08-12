#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <lzma.h>

static unsigned char input[256 * 1024];
static unsigned char compressed[2 * 1024 * 1024];
static unsigned char decoded[256 * 1024 + 1024];

static int fail(const char* stage, lzma_ret ret) {
  printf("xz_roundtrip_test: %s failed (%d)\n", stage, (int)ret);
  return 1;
}

int main(void) {
  for (size_t i = 0; i < sizeof(input); ++i) {
    input[i] = (unsigned char)((i * 31u + (i >> 3) + "crt-xz-liblzma"[i % 14]) & 0xffu);
  }

  size_t compressed_pos = 0;
  lzma_ret ret = lzma_easy_buffer_encode(9 | LZMA_PRESET_EXTREME,
                                         LZMA_CHECK_CRC64, NULL,
                                         input, sizeof(input),
                                         compressed, &compressed_pos,
                                         sizeof(compressed));
  if (ret != LZMA_OK) {
    return fail("encode", ret);
  }
  if (compressed_pos == 0 || compressed_pos >= sizeof(compressed)) {
    printf("xz_roundtrip_test: invalid compressed size\n");
    return 1;
  }

  uint64_t memlimit = UINT64_MAX;
  size_t compressed_read = 0;
  size_t decoded_pos = 0;
  ret = lzma_stream_buffer_decode(&memlimit, 0, NULL,
                                  compressed, &compressed_read, compressed_pos,
                                  decoded, &decoded_pos, sizeof(decoded));
  if (ret != LZMA_OK) {
    return fail("decode", ret);
  }
  if (compressed_read != compressed_pos || decoded_pos != sizeof(input)) {
    printf("xz_roundtrip_test: size mismatch\n");
    return 1;
  }
  if (memcmp(input, decoded, sizeof(input)) != 0) {
    printf("xz_roundtrip_test: data mismatch\n");
    return 1;
  }

  printf("xz_roundtrip_test: ok compressed=%zu decoded=%zu version=%s\n",
         compressed_pos, decoded_pos, lzma_version_string());
  return 0;
}
