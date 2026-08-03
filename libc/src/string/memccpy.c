#include <stddef.h>
#include <string.h>

void* memccpy(void* dst, const void* src, int c, size_t n) {
  unsigned char* d = (unsigned char*)dst;
  const unsigned char* s = (const unsigned char*)src;
  unsigned char ch = (unsigned char)c;
  size_t i;

  for (i = 0; i < n; ++i) {
    d[i] = s[i];
    if (s[i] == ch) {
      return d + i + 1;
    }
  }
  return 0;
}
