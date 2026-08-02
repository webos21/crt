#include <stddef.h>
#include <string.h>

void* memrchr(const void* s, int c, size_t n) {
  const unsigned char* p = (const unsigned char*)s + n;
  unsigned char ch = (unsigned char)c;

  while (p != (const unsigned char*)s) {
    --p;
    if (*p == ch) {
      return (void*)p;
    }
  }
  return 0;
}
