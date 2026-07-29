/*
 * Project-owned public sinf/cosf wrappers over current Bionic FreeBSD/msun
 * float argument reduction and kernel helpers.
 */

#include <float.h>

#include "math.h"
#define INLINE_KERNEL_SINDF
#define INLINE_KERNEL_COSDF
#define INLINE_REM_PIO2F
#include "math_private.h"
#include "e_rem_pio2f.c"
#include "k_sinf.c"
#include "k_cosf.c"

static const double
sc1pio2 = 1*M_PI_2,
sc2pio2 = 2*M_PI_2,
sc3pio2 = 3*M_PI_2,
sc4pio2 = 4*M_PI_2;

float sinf(float x) {
  double y;
  int32_t n;
  int32_t hx;
  int32_t ix;

  GET_FLOAT_WORD(hx, x);
  ix = hx & 0x7fffffff;
  if (ix <= 0x3f490fda) {
    if (ix < 0x39800000) {
      if (((int)x) == 0) {
        return x;
      }
    }
    return __kernel_sindf(x);
  }
  if (ix <= 0x407b53d1) {
    if (ix <= 0x4016cbe3) {
      return hx > 0 ? __kernel_cosdf(x - sc1pio2) : -__kernel_cosdf(x + sc1pio2);
    }
    return __kernel_sindf(hx > 0 ? sc2pio2 - x : -(x + sc2pio2));
  }
  if (ix <= 0x40e231d5) {
    if (ix <= 0x40afeddf) {
      return hx > 0 ? -__kernel_cosdf(x - sc3pio2) : __kernel_cosdf(x + sc3pio2);
    }
    return __kernel_sindf(hx > 0 ? x - sc4pio2 : x + sc4pio2);
  }
  if (ix >= 0x7f800000) {
    return x - x;
  }
  n = __ieee754_rem_pio2f(x, &y);
  switch (n & 3) {
    case 0:
      return __kernel_sindf(y);
    case 1:
      return __kernel_cosdf(y);
    case 2:
      return -__kernel_sindf(y);
    default:
      return -__kernel_cosdf(y);
  }
}

float cosf(float x) {
  double y;
  int32_t n;
  int32_t hx;
  int32_t ix;

  GET_FLOAT_WORD(hx, x);
  ix = hx & 0x7fffffff;
  if (ix <= 0x3f490fda) {
    if (ix < 0x39800000) {
      return 1.0f;
    }
    return __kernel_cosdf(x);
  }
  if (ix <= 0x407b53d1) {
    if (ix <= 0x4016cbe3) {
      return hx > 0 ? __kernel_sindf(sc1pio2 - x) : __kernel_sindf(x + sc1pio2);
    }
    return -__kernel_cosdf(hx > 0 ? x - sc2pio2 : x + sc2pio2);
  }
  if (ix <= 0x40e231d5) {
    if (ix <= 0x40afeddf) {
      return hx > 0 ? __kernel_sindf(x - sc3pio2) : -__kernel_sindf(x + sc3pio2);
    }
    return __kernel_cosdf(hx > 0 ? x - sc4pio2 : x + sc4pio2);
  }
  if (ix >= 0x7f800000) {
    return x - x;
  }
  n = __ieee754_rem_pio2f(x, &y);
  switch (n & 3) {
    case 0:
      return __kernel_cosdf(y);
    case 1:
      return -__kernel_sindf(y);
    case 2:
      return -__kernel_cosdf(y);
    default:
      return __kernel_sindf(y);
  }
}
