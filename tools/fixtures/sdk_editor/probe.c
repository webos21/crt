#include <stddef.h>
#include <zlib.h>

#include "editor_probe.h"

#if !defined(CRT_TARGET_OS_LINUX) && !defined(CRT_TARGET_OS_MACOS)
#error The SDK must declare the native target.
#endif

int editor_probe(void) {
  return sizeof(size_t) == 8 && ZLIB_VERNUM > 0 ? EDITOR_PROBE_VALUE : 0;
}
