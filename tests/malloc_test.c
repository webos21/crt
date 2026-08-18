#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fail(const char* message) {
  static const char prefix[] = "malloc_test: ";
  static const char suffix[] = "\n";
  write(2, prefix, sizeof(prefix) - 1);
  write(2, message, strlen(message));
  write(2, suffix, sizeof(suffix) - 1);
  return 1;
}

int main(void) {
  unsigned char* bytes;
  unsigned char* grown;
  unsigned char* zeros;
  unsigned char* reused;
  unsigned char* large;
  size_t i;

  bytes = (unsigned char*)malloc(16);
  if (bytes == 0) {
    return fail("malloc");
  }
  for (i = 0; i < 16; ++i) {
    bytes[i] = (unsigned char)(i + 1);
  }
  if (malloc_usable_size(bytes) < 16 || malloc_usable_size(0) != 0) {
    return fail("malloc usable size");
  }

  grown = (unsigned char*)realloc(bytes, 64);
  if (grown == 0) {
    return fail("realloc grow");
  }
  for (i = 0; i < 16; ++i) {
    if (grown[i] != (unsigned char)(i + 1)) {
      return fail("realloc preserve");
    }
  }

  zeros = (unsigned char*)calloc(8, 4);
  if (zeros == 0) {
    return fail("calloc");
  }
  for (i = 0; i < 32; ++i) {
    if (zeros[i] != 0) {
      return fail("calloc zero");
    }
  }

  free(grown);
  reused = (unsigned char*)malloc(16);
  if (reused == 0) {
    return fail("malloc reuse");
  }

  errno = 0;
  if (malloc((size_t)-1) != 0 || errno != ENOMEM) {
    return fail("malloc enomem");
  }

  large = (unsigned char*)malloc(1024u * 1024u + 4096u);
  if (large == 0) {
    return fail("large malloc");
  }
  large[0] = 0x11;
  large[1024u * 1024u + 4095u] = 0x22;
  if (large[0] != 0x11 || large[1024u * 1024u + 4095u] != 0x22) {
    return fail("large malloc contents");
  }

  free(large);
  free(reused);
  free(zeros);
  free(0);

  write(1, "malloc_test: ok\n", 16);
  return 0;
}
