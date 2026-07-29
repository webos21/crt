#ifndef CRT_INTTYPES_H
#define CRT_INTTYPES_H

#include <stdint.h>

#define PRId8 "d"
#define PRId16 "d"
#define PRId32 "d"
#define PRId64 "lld"
#define PRIdMAX "lld"
#define PRIdPTR "ld"

#define PRIi8 "i"
#define PRIi16 "i"
#define PRIi32 "i"
#define PRIi64 "lli"
#define PRIiMAX "lli"
#define PRIiPTR "li"

#define PRIu8 "u"
#define PRIu16 "u"
#define PRIu32 "u"
#define PRIu64 "llu"
#define PRIuMAX "llu"
#define PRIuPTR "lu"

#define PRIx8 "x"
#define PRIx16 "x"
#define PRIx32 "x"
#define PRIx64 "llx"
#define PRIxMAX "llx"
#define PRIxPTR "lx"

#define PRIX8 "X"
#define PRIX16 "X"
#define PRIX32 "X"
#define PRIX64 "llX"
#define PRIXMAX "llX"
#define PRIXPTR "lX"

#define SCNd8 "hhd"
#define SCNd16 "hd"
#define SCNd32 "d"
#define SCNd64 "lld"
#define SCNdMAX "lld"
#define SCNdPTR "ld"

#define SCNu8 "hhu"
#define SCNu16 "hu"
#define SCNu32 "u"
#define SCNu64 "llu"
#define SCNuMAX "llu"
#define SCNuPTR "lu"

#define SCNx8 "hhx"
#define SCNx16 "hx"
#define SCNx32 "x"
#define SCNx64 "llx"
#define SCNxMAX "llx"
#define SCNxPTR "lx"

#ifdef __cplusplus
extern "C" {
#endif

intmax_t strtoimax(const char* nptr, char** endptr, int base);
uintmax_t strtoumax(const char* nptr, char** endptr, int base);

#ifdef __cplusplus
}
#endif

#endif
