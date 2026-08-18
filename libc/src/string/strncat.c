#include <string.h>

char* strncat(char* dst, const char* src, size_t n) {
  char* result = dst;

  while (*dst != '\0') {
    ++dst;
  }
  while (n != 0 && *src != '\0') {
    *dst++ = *src++;
    --n;
  }
  *dst = '\0';
  return result;
}
