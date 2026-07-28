#include <string.h>

char* strtok_r(char* s, const char* delim, char** last) {
  char* token;

  if (last == 0 || delim == 0) {
    return 0;
  }
  if (s == 0) {
    s = *last;
  }
  if (s == 0) {
    return 0;
  }

  s += strspn(s, delim);
  if (*s == 0) {
    *last = s;
    return 0;
  }

  token = s;
  s = strpbrk(token, delim);
  if (s != 0) {
    *s++ = 0;
    *last = s;
  } else {
    *last = token + strlen(token);
  }
  return token;
}
