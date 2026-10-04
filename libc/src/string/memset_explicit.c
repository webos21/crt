#include <stddef.h>
#include <string.h>

/* memset_explicit(): Bionic's string.h (API 34, C23) -- memset() whose stores the
 * compiler may not remove as dead, for wiping secrets. Returns `dst`, like Bionic.
 * gnulib (gperf, ...) uses it when present instead of its own replacement, and on
 * macOS would otherwise pick the host libSystem's non-Bionic memset_s(). */
void* memset_explicit(void* dst, int ch, size_t n) {
  memset(dst, ch, n);
  /* Make the stores observable: the empty asm reads `dst` and clobbers memory. */
  __asm__ __volatile__("" : : "r"(dst) : "memory");
  return dst;
}
