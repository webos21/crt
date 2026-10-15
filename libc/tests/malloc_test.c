#include <errno.h>
#include <malloc.h>
#include <stdint.h>
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

static int check_adjacent_coalescing(void) {
  unsigned char* blocks[12];
  unsigned char* combined;
  uintptr_t address;
  size_t i;
  for (i = 0; i < 12; ++i) {
    blocks[i] = (unsigned char*)malloc(2048);
    if (blocks[i] == 0) return fail("coalescing allocation");
    memset(blocks[i], (int)i, 2048);
  }
  address = (uintptr_t)blocks[4];
  /* Exercise backward merging on ascending release, then forward merging on
   * descending release. No single block can satisfy the combined request. */
  for (i = 4; i < 8; ++i) {
    free(blocks[i]);
    blocks[i] = 0;
  }
  combined = (unsigned char*)malloc(7000);
  if (combined == 0 || (uintptr_t)combined != address)
    return fail("adjacent coalescing reuse");
  memset(combined, 0x5a, 7000);
  for (i = 0; i < 2048; ++i) {
    if (blocks[3][i] != 3 || blocks[8][i] != 8)
      return fail("coalescing live neighbours");
  }
  combined = (unsigned char*)realloc(combined, 3073);
  if (combined == 0 || combined[3072] != 0x5a)
    return fail("coalescing shrink preservation");
  free(combined);
  for (i = 12; i > 0; --i) free(blocks[i - 1]);
  return 0;
}

int main(void) {
  unsigned char* bytes;
  unsigned char* grown;
  unsigned char* zeros;
  unsigned char* reused;
  unsigned char* large;
  unsigned char* aligned;
  unsigned char* aligned32;
  unsigned char* aligned_page;
  unsigned char* moved;
  unsigned char* moved_grown;
  size_t i;

  if (check_adjacent_coalescing() != 0) return 1;

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
#ifdef CRT_DEBUG_MALLOC
  if (malloc_usable_size(bytes) != 16) {
    return fail("debug malloc usable size");
  }
#endif

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

  aligned = 0;
  if (posix_memalign((void**)&aligned, 64, 37) != 0 || aligned == 0 ||
      ((size_t)aligned & 63u) != 0 || malloc_usable_size(aligned) < 37) {
    return fail("posix memalign");
  }
  memset(aligned, 0x5a, 37);
  aligned = (unsigned char*)realloc(aligned, 80);
  if (aligned == 0 || ((size_t)aligned & 63u) != 0 || aligned[0] != 0x5a ||
      aligned[36] != 0x5a) {
    return fail("aligned realloc");
  }
  free(aligned);
  if (posix_memalign((void**)&aligned, 3, 8) != EINVAL) {
    return fail("posix memalign invalid");
  }

  /* Payload alignment must stay independent of private header size:
   * posix_memalign(32) must not take a merely 16-byte-aligned shortcut.
   * Exercise both a small and page-size request on every host. */
  aligned32 = 0;
  if (posix_memalign((void**)&aligned32, 32, 19) != 0 || aligned32 == 0 ||
      ((size_t)aligned32 & 31u) != 0) {
    return fail("posix memalign 32");
  }
  memset(aligned32, 0x3c, 19);
  free(aligned32);

  aligned_page = 0;
  if (posix_memalign((void**)&aligned_page, 4096, 33) != 0 || aligned_page == 0 ||
      ((size_t)aligned_page & 4095u) != 0) {
    return fail("posix memalign page");
  }
  memset(aligned_page, 0x6d, 33);
  free(aligned_page);

  /* Force realloc's allocate/copy/free path with non-alignment-sized requests.
   * In diagnostic builds the old physical block includes canary slack; only
   * requested bytes may be copied into the freshly canaried replacement. */
  moved = (unsigned char*)malloc(37);
  if (moved == 0) {
    return fail("moved realloc malloc");
  }
  for (i = 0; i < 37; ++i) {
    moved[i] = (unsigned char)(0xa0u + (i & 15u));
  }
  moved_grown = (unsigned char*)realloc(moved, 1024u * 1024u + 37u);
  if (moved_grown == 0) {
    free(moved);
    return fail("moved realloc grow");
  }
  for (i = 0; i < 37; ++i) {
    if (moved_grown[i] != (unsigned char)(0xa0u + (i & 15u))) {
      return fail("moved realloc preserve");
    }
  }
  free(moved_grown);

  free(reused);
  free(zeros);
  free(0);

  write(1, "malloc_test: ok\n", 16);
  return 0;
}
