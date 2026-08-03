#include <stddef.h>
#include <stdlib.h>
#include <string.h>

char* strndup(const char* str, size_t n) {
  size_t len = strnlen(str, n);
  char* copy = (char*)malloc(len + 1);

  if (copy == 0) {
    return 0;
  }
  memcpy(copy, str, len);
  copy[len] = '\0';
  return copy;
}
