#include <ctype.h>
#include <stddef.h>
#include <string.h>

int strcasecmp(const char* s1, const char* s2) {
  const unsigned char* p1 = (const unsigned char*)s1;
  const unsigned char* p2 = (const unsigned char*)s2;

  while (*p1 != '\0' && *p2 != '\0') {
    int c1 = tolower(*p1);
    int c2 = tolower(*p2);
    if (c1 != c2) {
      return c1 - c2;
    }
    ++p1;
    ++p2;
  }
  return tolower(*p1) - tolower(*p2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
  const unsigned char* p1 = (const unsigned char*)s1;
  const unsigned char* p2 = (const unsigned char*)s2;
  size_t i;

  for (i = 0; i < n; ++i) {
    int c1 = tolower(p1[i]);
    int c2 = tolower(p2[i]);
    if (c1 != c2 || c1 == '\0') {
      return c1 - c2;
    }
  }
  return 0;
}
