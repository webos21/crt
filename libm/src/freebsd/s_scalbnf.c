/* s_scalbnf.c -- float version of s_scalbn.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 */

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#include <math.h>

#include "math_private.h"

static const float two25 = 3.355443200e+07f;
static const float twom25 = 2.9802322388e-08f;
static const float huge = 1.0e+30f;
static const float tiny = 1.0e-30f;

float scalbnf(float x, int n) {
  int32_t k;
  int32_t ix;

  GET_FLOAT_WORD(ix, x);
  k = (ix & 0x7f800000) >> 23;
  if (k == 0) {
    if ((ix & 0x7fffffff) == 0) {
      return x;
    }
    x *= two25;
    GET_FLOAT_WORD(ix, x);
    k = ((ix & 0x7f800000) >> 23) - 25;
    if (n < -50000) {
      return tiny * x;
    }
  }
  if (k == 0xff) {
    return x + x;
  }
  k = k + n;
  if (k > 0xfe) {
    return huge * copysignf(huge, x);
  }
  if (k > 0) {
    SET_FLOAT_WORD(x, (ix & 0x807fffff) | (k << 23));
    return x;
  }
  if (k <= -25) {
    if (n > 50000) {
      return huge * copysignf(huge, x);
    }
    return tiny * copysignf(tiny, x);
  }
  k += 25;
  SET_FLOAT_WORD(x, (ix & 0x807fffff) | (k << 23));
  return x * twom25;
}

float ldexpf(float x, int n) {
  return scalbnf(x, n);
}
