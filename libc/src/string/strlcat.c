#include <string.h>

size_t strlcat(char* dst, const char* src, size_t size) {
  size_t dst_len = strnlen(dst, size);
  size_t src_len = strlen(src);

  if (dst_len == size) {
    return size + src_len;
  }
  if (src_len < size - dst_len) {
    memcpy(dst + dst_len, src, src_len + 1);
  } else {
    memcpy(dst + dst_len, src, size - dst_len - 1);
    dst[size - 1] = 0;
  }
  return dst_len + src_len;
}
