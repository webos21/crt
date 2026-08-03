#include <stddef.h>
#include <string.h>

size_t strnlen(const char* s, size_t n) {
  const char* end = (const char*)memchr(s, '\0', n);

  if (end == 0) {
    return n;
  }
  return (size_t)(end - s);
}
