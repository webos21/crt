#ifndef CRT_GDTOA_MACHINE_IEEE_H
#define CRT_GDTOA_MACHINE_IEEE_H

#include <float.h>
#include <stdint.h>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "CRT gdtoa machine/ieee.h currently supports little-endian IEEE targets only"
#endif

#define DBL_FRACHBITS 20
#define DBL_FRACLBITS 32

struct ieee_double {
  uint32_t dbl_fracl;
  unsigned int dbl_frach : 20;
  unsigned int dbl_exp : 11;
  unsigned int dbl_sign : 1;
};

#if LDBL_MANT_DIG == DBL_MANT_DIG

struct ieee_ext {
  struct ieee_double value;
};

#define EXT_FRACHBITS DBL_FRACHBITS
#define EXT_FRACLBITS DBL_FRACLBITS
#define EXT_TO_ARRAY32(p, a) \
  do { \
    (a)[0] = (p)->value.dbl_fracl; \
    (a)[1] = (uint32_t)(p)->value.dbl_frach; \
  } while (0)

#elif LDBL_MANT_DIG == 64

#define EXT_FRACHBITS 32
#define EXT_FRACLBITS 32

struct ieee_ext {
  uint32_t ext_fracl;
  uint32_t ext_frach;
  unsigned int ext_exp : 15;
  unsigned int ext_sign : 1;
  unsigned int ext_pad : 16;
};

#define EXT_TO_ARRAY32(p, a) \
  do { \
    (a)[0] = (p)->ext_fracl; \
    (a)[1] = (p)->ext_frach; \
  } while (0)

#elif LDBL_MANT_DIG == 113

#define EXT_FRACHBITS 16
#define EXT_FRACHMBITS 32
#define EXT_FRACLMBITS 32
#define EXT_FRACLBITS 32
#define EXT_IMPLICIT_NBIT

struct ieee_ext {
  uint32_t ext_fracl;
  uint32_t ext_fraclm;
  uint32_t ext_frachm;
  unsigned int ext_frach : 16;
  unsigned int ext_exp : 15;
  unsigned int ext_sign : 1;
};

#define EXT_TO_ARRAY32(p, a) \
  do { \
    (a)[0] = (p)->ext_fracl; \
    (a)[1] = (p)->ext_fraclm; \
    (a)[2] = (p)->ext_frachm; \
    (a)[3] = (uint32_t)(p)->ext_frach; \
  } while (0)

#else
#error "Unsupported long double format for CRT gdtoa"
#endif

#endif
