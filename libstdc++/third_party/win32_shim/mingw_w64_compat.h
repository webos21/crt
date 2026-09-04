/* Project-owned, force-included compatibility header for mingw-w64-
 * headers-consuming (D3D12 Ganesh) Windows compiles only.
 *
 * Unlike every other file in this directory, this one is never reached
 * via a real `#include <...>` -- it is force-included (`-include`,
 * added only to the mingw-w64-headers-consuming extra_cflags in
 * tools/build_skia.py and libcrtgfx/CMakeLists.txt; see each one's own
 * comment) so its content lands before *any* other header in the
 * translation unit, including mingw-w64's own real windows.h chain.
 *
 * _WCHAR_T_DEFINED / _WCTYPE_T_DEFINED (2026-09-04, real, confirmed
 * necessary): mingw-w64's own real corecrt.h is the canonical place
 * these two definitional guards normally get set (gating both a
 * fallback `typedef unsigned short wchar_t;` in rpcndr.h -- illegal in
 * C++ regardless, wchar_t is always a builtin keyword here -- and
 * corecrt.h's own `typedef unsigned short wctype_t;`). This project
 * deliberately never vendors mingw-w64's own corecrt.h at all (see
 * win32_shim/malloc.h's own top comment for the real reason: a real,
 * confirmed `unsigned short` vs. this project's own libc `unsigned
 * long` wctype_t conflict). Pre-defining both guards here means
 * whichever real mingw-w64 header would otherwise have defined them
 * (rpcndr.h, or corecrt.h had it been vendored) sees its own `#ifndef`
 * already satisfied and skips its own typedef entirely -- confirmed for
 * real: without this, `error: 'short wchar_t' is invalid` (rpcndr.h,
 * illegal even to attempt in C++) and (when corecrt.h was still
 * vendored, since reverted) `typedef redefinition with different
 * types ('unsigned short' vs 'unsigned long')` for wctype_t against
 * this project's own sysroot/include/wctype.h. Confirmed safe: neither
 * this project's own sysroot/include/wchar.h nor wctype.h reference
 * either guard macro at all (grep'd directly) -- their own real
 * typedefs are unconditional, so nothing here ever suppresses this
 * project's own libc content, only mingw-w64's.
 *
 * _wcsicmp (2026-09-04, real, confirmed necessary): mingw-w64's own real
 * <stralign.h> (reached transitively via windows.h -> ... -> winscard.h
 * -> wtypes.h -> stralign.h, an unavoidable part of windows.h's own
 * wholesale include cascade, unrelated to D3D12/Ganesh itself) calls
 * this real, stable mingw-w64/MSVCRT CRT extension (case-insensitive
 * wide-string compare, learn.microsoft.com/en-us/cpp/c-runtime-library/
 * reference/wcsicmp-wcsicmp-l-mbsicmp-mbsicmp-l-mbsicmp-l) without
 * declaring it anywhere reachable in this project's own real libc --
 * confirmed for real: `use of undeclared identifier '_wcsicmp'`. Never
 * actually called by this project's own code; declared here purely so
 * stralign.h's own real, unavoidable call site parses and links against
 * *some* real declaration.
 */
#ifndef CRT_WIN32_SHIM_MINGW_W64_COMPAT_H
#define CRT_WIN32_SHIM_MINGW_W64_COMPAT_H

#define _WCHAR_T_DEFINED 1
#define _WCTYPE_T_DEFINED 1

/* Force-included before anything else in the TU (see this file's own
 * top comment) -- wchar_t is always a builtin keyword in C++, but in a
 * plain C compile (if this ever lands on one) it is only a real type
 * once <stddef.h> has defined it; this project's own real libc stddef.h
 * is what actually ends up providing it, reached normally here since
 * nothing about this force-include changes the real include search
 * path itself. */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int _wcsicmp(const wchar_t* _String1, const wchar_t* _String2);

#ifdef __cplusplus
} /* extern "C" */
#endif

/* _countof (2026-09-04, real, confirmed necessary): Skia's own real
 * src/gpu/ganesh/d3d/GrD3DCaps.cpp calls this real, stable, publicly
 * documented Microsoft/mingw-w64 macro (learn.microsoft.com/en-us/cpp/
 * c-runtime-library/reference/countof) -- confirmed for real: `use of
 * undeclared identifier '_countof'`. mingw-w64's own real definition
 * (crt/stdlib.h, deliberately not vendored -- a real C-standard-library
 * name, see win32_shim/malloc.h's own top comment for why those stay
 * excluded) has a separate, bounds-checked C++ overload using a
 * template plus the `UNALIGNED` macro (only defined later, by
 * rpcndr.h's own real content, itself only reached after this force-
 * included file) -- this project's own real usage is always a plain,
 * ordinary local array, so the simpler, plain C-style expansion (valid
 * in both C and C++, and correct for every real call site in this
 * project's own D3D12-touching compiles) is used here instead. */
#ifndef _countof
#define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#endif

#endif /* CRT_WIN32_SHIM_MINGW_W64_COMPAT_H */
