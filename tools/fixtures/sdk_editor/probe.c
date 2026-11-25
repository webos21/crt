#include <stddef.h>
#include <zlib.h>

#include "editor_probe.h"

#ifndef CRT_TARGET_OS_LINUX
#error The SDK must declare the native target.
#endif

int editor_probe(void) {
  return sizeof(size_t) == 8 && ZLIB_VERNUM > 0 ? EDITOR_PROBE_VALUE : 0;
}
