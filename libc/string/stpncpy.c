#include <stddef.h>
#include <string.h>

char* stpncpy(char* dst, const char* src, size_t n) {
  size_t i;

  for (i = 0; i < n && src[i] != '\0'; ++i) {
    dst[i] = src[i];
  }
  if (i == n) {
    return dst + n;
  }
  dst[i] = '\0';
  while (++i < n) {
    dst[i] = '\0';
  }
  return dst + strlen(dst);
}
