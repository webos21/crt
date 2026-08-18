#ifndef CRT_LIBM_FREEBSD_COMPAT_H
#define CRT_LIBM_FREEBSD_COMPAT_H

#include <ctype.h>

/* Bionic supplies this private BSD helper to imported msun sources. */
static inline int digittoint(char ch) {
  if (!isxdigit((unsigned char)ch)) return -1;
  if (isdigit((unsigned char)ch)) return ch - '0';
  return tolower((unsigned char)ch) - 'a' + 10;
}

#endif
