#include <stddef.h>
#include <string.h>

size_t strxfrm(char* dst, const char* src, size_t n) {
  size_t len = strlen(src);

  if (n != 0) {
    size_t copy_len = len;
    if (copy_len >= n) {
      copy_len = n - 1;
    }
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
  }
  return len;
}
