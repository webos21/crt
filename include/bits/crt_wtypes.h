#ifndef CRT_BITS_CRT_WTYPES_H
#define CRT_BITS_CRT_WTYPES_H

/* The wide-character types, in a leaf header with no includes of its own.
 * <wchar.h>, <wctype.h> and <xlocale.h> all need them, and they include each
 * other (and <locale.h>, <stdio.h>, <stdlib.h>, <time.h>) in a ring; a consumer
 * that interposes a header such as gnulib's <locale.h>/<stdint.h> wrapper can
 * enter that ring at any point, so none of the types may depend on which
 * header is parsed first (found by gnulib in the gperf port). */

#include <stddef.h>

/* <wchar.h> declares fgetwc()/fputws()/... on FILE*, so it needs the type even
 * while <stdio.h> is still being parsed; both headers share this one guard. */
#ifndef __CRT_FILE_DEFINED
#define __CRT_FILE_DEFINED
struct __sFILE;
typedef struct __sFILE FILE;
#endif

typedef __WINT_TYPE__ wint_t;
/* _MBSTATE_T: the same controlled-redefinition sentinel real BSD/Darwin
 * headers use for this exact typedef (Apple's own <sys/_types/_mbstate_t.h>
 * guards itself identically) -- not this project's own invention. Needed
 * for real, not preemptively: a translation unit compiled with real Apple
 * SDK headers reachable (-fcrt-real-apple-sdk, tools/crt-c++/crt-cc's own
 * sentinel -- e.g. libcrtgfx/src/skia_bridge.cc's real CoreFoundation/
 * Metal use) can end up with both this header's own mbstate_t and Apple's
 * real __darwin_mbstate_t-backed one reachable in the same TU; without a
 * shared guard both definitions run, and clang correctly rejects the
 * result as a genuine, incompatible typedef redefinition ('struct
 * mbstate_t' vs '__darwin_mbstate_t') -- confirmed for real, 2026-09-11,
 * building the isolated 04-gfx-media distribution stage on macOS.
 * Whichever definition is reachable first in a given TU wins; every
 * caller in this project only ever treats mbstate_t as opaque storage
 * handed to mbrlen()/mbrtowc()/etc., so which concrete layout wins here
 * is never observable. */
#ifndef _MBSTATE_T
#define _MBSTATE_T
typedef struct {
  unsigned int codepoint;
  unsigned char expected;
  unsigned char seen;
} mbstate_t;
#endif

typedef unsigned long wctype_t;
typedef unsigned long wctrans_t;

#endif
