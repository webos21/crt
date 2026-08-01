#ifndef CRT_BYTESWAP_H
#define CRT_BYTESWAP_H

#include <stdint.h>

static inline uint16_t bswap_16(uint16_t value) {
  return (uint16_t)((value >> 8) | (value << 8));
}

static inline uint32_t bswap_32(uint32_t value) {
  return __builtin_bswap32(value);
}

static inline uint64_t bswap_64(uint64_t value) {
  return __builtin_bswap64(value);
}

#endif
