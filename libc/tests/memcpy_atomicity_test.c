/* Small memcpy()/memmove() copies are single stores, not byte loops. A runtime that patches
 * code another thread is executing (JavaScriptCore/WTF's performJITMemcpy of a 4- or 8-byte
 * displacement or immediate) relies on it, as it does on glibc and Bionic. The reader thread
 * loads the destination with one aligned load while the writer alternates two patterns with
 * memcpy through a function pointer (so the compiler cannot turn it into a mov itself); a byte
 * loop is observed half-written. Found by the JavaScriptCore WebAssembly acceptance: BBQ->OMG
 * call-site patching crashed about 1 run in 100 under CPU contention. */
#define _GNU_SOURCE 1
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef void* (*copy_fn)(void*, const void*, size_t);

static copy_fn volatile copy_function = memcpy;
static copy_fn volatile move_function = memmove;

static union { uint64_t whole[2]; unsigned char bytes[16]; } cell __attribute__((aligned(64)));
static volatile int stop;
static volatile unsigned long torn4, torn8;

static void* reader(void* argument) {
  (void)argument;
  while (!stop) {
    uint32_t four = *(volatile uint32_t*)&cell.bytes[0];
    uint64_t eight = *(volatile uint64_t*)&cell.bytes[8];
    if (four != 0x01010101u && four != 0xfefefefeu) ++torn4;
    if (eight != 0x0101010101010101ull && eight != 0xfefefefefefefefeull) ++torn8;
  }
  return 0;
}

int main(void) {
  static const uint32_t four_a = 0x01010101u, four_b = 0xfefefefeu;
  static const uint64_t eight_a = 0x0101010101010101ull, eight_b = 0xfefefefefefefefeull;
  pthread_t thread;
  long i;
  int failures = 0;

  memcpy(&cell.bytes[0], &four_a, 4);
  memcpy(&cell.bytes[8], &eight_a, 8);
  pthread_create(&thread, 0, reader, 0);
  for (i = 0; i < 3000000; ++i) {
    copy_function(&cell.bytes[0], (i & 1) ? &four_a : &four_b, 4);
    copy_function(&cell.bytes[8], (i & 1) ? &eight_a : &eight_b, 8);
    if ((i & 3) == 0) {
      move_function(&cell.bytes[0], (i & 4) ? &four_a : &four_b, 4);
      move_function(&cell.bytes[8], (i & 4) ? &eight_a : &eight_b, 8);
    }
  }
  stop = 1;
  pthread_join(thread, 0);
  if (torn4 != 0) {
    fprintf(stderr, "memcpy_atomicity_test: %lu torn 4-byte copies\n", torn4);
    ++failures;
  }
  if (torn8 != 0) {
    fprintf(stderr, "memcpy_atomicity_test: %lu torn 8-byte copies\n", torn8);
    ++failures;
  }

  /* Correctness of every small size, overlapping memmove included. */
  {
    unsigned char source[64], destination[64];
    size_t n, offset;
    for (n = 0; n <= 40; ++n) {
      for (offset = 0; offset < 3; ++offset) {
        size_t k;
        for (k = 0; k < sizeof source; ++k) { source[k] = (unsigned char)(k * 7 + 1); destination[k] = 0xee; }
        copy_function(destination + offset, source + 1, n);
        for (k = 0; k < sizeof destination; ++k) {
          unsigned char want = (k >= offset && k < offset + n) ? source[1 + k - offset] : 0xee;
          if (destination[k] != want) { fprintf(stderr, "memcpy_atomicity_test: memcpy n=%zu offset=%zu byte %zu\n", n, offset, k); ++failures; break; }
        }
        for (k = 0; k < sizeof source; ++k) source[k] = (unsigned char)(k + 3);
        move_function(source + 2 + offset, source + 1, n);   /* forward overlap */
        for (k = 0; k < n; ++k) if (source[2 + offset + k] != (unsigned char)(1 + k + 3)) { fprintf(stderr, "memcpy_atomicity_test: memmove forward n=%zu offset=%zu\n", n, offset); ++failures; break; }
        for (k = 0; k < sizeof source; ++k) source[k] = (unsigned char)(k + 3);
        move_function(source + 1, source + 2 + offset, n);   /* backward overlap */
        for (k = 0; k < n; ++k) if (source[1 + k] != (unsigned char)(2 + offset + k + 3)) { fprintf(stderr, "memcpy_atomicity_test: memmove backward n=%zu offset=%zu\n", n, offset); ++failures; break; }
      }
    }
  }
  if (failures != 0) {
    return 1;
  }
  printf("memcpy_atomicity_test: ok\n");
  return 0;
}
