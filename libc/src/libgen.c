#include <libgen.h>
#include <string.h>

char* basename(char* path) {
  char* slash;

  if (path == 0 || path[0] == 0) {
    return ".";
  }
  slash = strrchr(path, '/');
  if (slash == 0) {
    return path;
  }
  while (slash > path && slash[1] == 0) {
    *slash = 0;
    slash = strrchr(path, '/');
    if (slash == 0) {
      return path;
    }
  }
  return slash[1] == 0 ? slash : slash + 1;
}

char* dirname(char* path) {
  char* slash;

  if (path == 0 || path[0] == 0) {
    return ".";
  }
  slash = strrchr(path, '/');
  if (slash == 0) {
    return ".";
  }
  while (slash > path && slash[0] == '/') {
    *slash-- = 0;
  }
  if (slash == path && path[0] == '/') {
    path[1] = 0;
    return path;
  }
  return path[0] == 0 ? "/" : path;
}
