#ifndef CRT_INTTYPES_H
#define CRT_INTTYPES_H

#include <stdint.h>

/* The 64-bit/MAX-width and pointer-width length modifiers below depend on
 * this target's actual `long` vs `long long` choice for `int64_t`/
 * `intmax_t`/`intptr_t` (see <stdint.h>, `__INT64_TYPE__` et al) -- this is
 * not a fixed choice across every host this project builds for. Linux and
 * macOS are LP64 (`long` is 64-bit, clang's `__INT64_TYPE__` is `long`, so
 * a single `l` length modifier is correct for the 64/MAX-width macros, and
 * also correct for the pointer-width ones since `intptr_t` is the same
 * 64-bit `long`). Windows is LLP64 (`long` stays 32-bit even in a 64-bit
 * build; clang's `*-w64-mingw32` target makes `__INT64_TYPE__` -- and
 * `intptr_t` -- `long long` instead, so both need the `ll` modifier
 * instead. Getting this wrong is not cosmetic: it is a real `-Wformat`
 * mismatch against the real underlying type on whichever host guessed
 * wrong, caught for real via a third-party mbedtls build's own
 * `MBEDTLS_PRINTF_MS_TIME` (`= PRId64`) usage warning against its
 * `int64_t`-typed `mbedtls_ms_time_t` on Linux/macOS. */
#if defined(CRT_TARGET_OS_WINDOWS)
#define CRT_PRI64_PREFIX "ll"
#define CRT_PRIPTR_PREFIX "ll"
#else
#define CRT_PRI64_PREFIX "l"
#define CRT_PRIPTR_PREFIX "l"
#endif

#define PRId8 "d"
#define PRId16 "d"
#define PRId32 "d"
#define PRId64 CRT_PRI64_PREFIX "d"
#define PRIdMAX CRT_PRI64_PREFIX "d"
#define PRIdPTR CRT_PRIPTR_PREFIX "d"

#define PRIi8 "i"
#define PRIi16 "i"
#define PRIi32 "i"
#define PRIi64 CRT_PRI64_PREFIX "i"
#define PRIiMAX CRT_PRI64_PREFIX "i"
#define PRIiPTR CRT_PRIPTR_PREFIX "i"

#define PRIu8 "u"
#define PRIu16 "u"
#define PRIu32 "u"
#define PRIu64 CRT_PRI64_PREFIX "u"
#define PRIuMAX CRT_PRI64_PREFIX "u"
#define PRIuPTR CRT_PRIPTR_PREFIX "u"

#define PRIx8 "x"
#define PRIx16 "x"
#define PRIx32 "x"
#define PRIx64 CRT_PRI64_PREFIX "x"
#define PRIxMAX CRT_PRI64_PREFIX "x"
#define PRIxPTR CRT_PRIPTR_PREFIX "x"

#define PRIX8 "X"
#define PRIX16 "X"
#define PRIX32 "X"
#define PRIX64 CRT_PRI64_PREFIX "X"
#define PRIXMAX CRT_PRI64_PREFIX "X"
#define PRIXPTR CRT_PRIPTR_PREFIX "X"

#define SCNd8 "hhd"
#define SCNd16 "hd"
#define SCNd32 "d"
#define SCNd64 CRT_PRI64_PREFIX "d"
#define SCNdMAX CRT_PRI64_PREFIX "d"
#define SCNdPTR CRT_PRIPTR_PREFIX "d"

#define SCNu8 "hhu"
#define SCNu16 "hu"
#define SCNu32 "u"
#define SCNu64 CRT_PRI64_PREFIX "u"
#define SCNuMAX CRT_PRI64_PREFIX "u"
#define SCNuPTR CRT_PRIPTR_PREFIX "u"

#define SCNx8 "hhx"
#define SCNx16 "hx"
#define SCNx32 "x"
#define SCNx64 CRT_PRI64_PREFIX "x"
#define SCNxMAX CRT_PRI64_PREFIX "x"
#define SCNxPTR CRT_PRIPTR_PREFIX "x"

#ifdef __cplusplus
extern "C" {
#endif

intmax_t strtoimax(const char* nptr, char** endptr, int base);
uintmax_t strtoumax(const char* nptr, char** endptr, int base);

#ifdef __cplusplus
}
#endif

#endif
