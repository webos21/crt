#include <errno.h>
#include <stdlib.h>

// Bionic-compatible reallocarray(): like realloc(ptr, nmemb * size), but
// fails safely (returning NULL with errno set to ENOMEM, leaving `ptr`
// untouched) instead of silently wrapping when nmemb * size overflows
// size_t. Added specifically so the ported NetBSD/Bionic regex engine
// (libc/src/regex/) can grow its internal strip/cset arrays without
// implementing its own overflow-checked multiply.
void* reallocarray(void* ptr, size_t nmemb, size_t size) {
  size_t total;

  if (__builtin_mul_overflow(nmemb, size, &total)) {
    errno = ENOMEM;
    return 0;
  }
  return realloc(ptr, total);
}
