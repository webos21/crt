#ifndef CRT_SYS_ENDIAN_H
#define CRT_SYS_ENDIAN_H

#include <stdint.h>

#define _LITTLE_ENDIAN 1234
#define _BIG_ENDIAN 4321
#define _PDP_ENDIAN 3412

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define _BYTE_ORDER _BIG_ENDIAN
#else
#define _BYTE_ORDER _LITTLE_ENDIAN
#endif

#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#endif
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN _BIG_ENDIAN
#endif
#ifndef __BYTE_ORDER
#define __BYTE_ORDER _BYTE_ORDER
#endif

#define __swap16 __builtin_bswap16
#define __swap32 __builtin_bswap32
#define __swap64 __builtin_bswap64

#if defined(__USE_BSD) || defined(__BIONIC__)
#define LITTLE_ENDIAN _LITTLE_ENDIAN
#define BIG_ENDIAN _BIG_ENDIAN
#define PDP_ENDIAN _PDP_ENDIAN
#define BYTE_ORDER _BYTE_ORDER
#endif

#endif
