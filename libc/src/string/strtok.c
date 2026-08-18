#include <string.h>

char* strtok(char* s, const char* delim) {
  static char* saved;

  return strtok_r(s, delim, &saved);
}
