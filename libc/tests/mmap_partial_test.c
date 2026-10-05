/* POSIX partial munmap and PROT_NONE reservations. WTF::OSAllocator (JavaScriptCore) reserves
 * `bytes + alignment` with mmap(PROT_NONE), then munmap()s the unaligned head and tail to keep
 * an aligned region, and it reserves multi-GiB regions it never fully uses. On Windows munmap()
 * released the whole VirtualAlloc allocation (or failed for an interior address) and PROT_NONE was
 * charged against the commit limit, so JavaScriptCore aborted in tryReserveUncommittedAligned. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define UNIT (64UL * 1024UL)

static int failures;

#define CHECK(condition, message)                                   \
  do {                                                              \
    if (!(condition)) {                                             \
      fprintf(stderr, "mmap_partial_test: %s (line %d)\n", message, __LINE__); \
      ++failures;                                                   \
    }                                                               \
  } while (0)

static void fill(unsigned char* p, size_t n, unsigned char seed) {
  size_t i;
  for (i = 0; i < n; ++i) p[i] = (unsigned char)(seed + i * 7);
}

static int check(const unsigned char* p, size_t n, unsigned char seed) {
  size_t i;
  for (i = 0; i < n; ++i) if (p[i] != (unsigned char)(seed + i * 7)) return 0;
  return 1;
}

int main(void) {
  /* A very large PROT_NONE reservation costs no memory. */
  {
    size_t huge = (size_t)16 << 30;
    void* r = mmap(0, huge, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    CHECK(r != MAP_FAILED, "a 16 GiB PROT_NONE reservation succeeds");
    if (r != MAP_FAILED) {
      CHECK(mprotect(r, UNIT, PROT_READ | PROT_WRITE) == 0, "the first unit of the reservation can be committed");
      fill(r, UNIT, 3);
      CHECK(check(r, UNIT, 3), "the committed unit holds data");
      CHECK(munmap(r, huge) == 0, "the whole reservation is released");
    }
  }

  /* Reserve more than needed and trim the head and the tail: what is kept stays usable. */
  {
    size_t total = 8 * UNIT;
    unsigned char* m = mmap(0, total, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    unsigned char* aligned;

    CHECK(m != MAP_FAILED, "8-unit PROT_NONE reservation");
    if (m != MAP_FAILED) {
      aligned = (unsigned char*)(((uintptr_t)m + UNIT - 1) & ~(uintptr_t)(UNIT - 1));
      if (aligned != m) {
        CHECK(munmap(m, (size_t)(aligned - m)) == 0, "the unaligned head is unmapped");
      }
      CHECK(munmap(aligned + 6 * UNIT, (size_t)((m + total) - (aligned + 6 * UNIT))) == 0, "the tail is unmapped");
      CHECK(mprotect(aligned, 6 * UNIT, PROT_READ | PROT_WRITE) == 0, "the kept middle is made accessible");
      fill(aligned, 6 * UNIT, 11);
      CHECK(check(aligned, 6 * UNIT, 11), "the kept middle holds data");

      /* An interior hole in committed memory: the pieces around it keep their data. */
      CHECK(munmap(aligned + 2 * UNIT, UNIT) == 0, "an interior unit is unmapped");
      CHECK(check(aligned, 2 * UNIT, 11), "the head piece is intact after the hole");
      CHECK(check(aligned + 3 * UNIT, 3 * UNIT, (unsigned char)(11 + 3 * UNIT * 7)),
            "the tail piece is intact after the hole");
      CHECK(munmap(aligned, 2 * UNIT) == 0, "the head piece is unmapped");
      CHECK(munmap(aligned + 3 * UNIT, 3 * UNIT) == 0, "the tail piece is unmapped");
    }
  }

  if (failures != 0) return 1;
  printf("mmap_partial_test: ok\n");
  return 0;
}
