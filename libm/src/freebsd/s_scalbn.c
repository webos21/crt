/* @(#)s_scalbn.c 5.1 93/09/24 */
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

/*
 * scalbn(double x, int n)
 * scalbn(x,n) returns x*2**n computed by exponent manipulation rather than by
 * actually performing an exponentiation or a multiplication.
 */

#include <float.h>
#include <math.h>

#include "math_private.h"

static const double two54 = 1.80143985094819840000e+16;
static const double twom54 = 5.55111512312578270212e-17;
static const double huge = 1.0e+300;
static const double tiny = 1.0e-300;

double scalbn(double x, int n) {
  int32_t k;
  int32_t hx;
  int32_t lx;

  EXTRACT_WORDS(hx, lx, x);
  k = (hx & 0x7ff00000) >> 20;
  if (k == 0) {
    if ((lx | (hx & 0x7fffffff)) == 0) {
      return x;
    }
    x *= two54;
    GET_HIGH_WORD(hx, x);
    k = ((hx & 0x7ff00000) >> 20) - 54;
    if (n < -50000) {
      return tiny * x;
    }
  }
  if (k == 0x7ff) {
    return x + x;
  }
  k = k + n;
  if (k > 0x7fe) {
    return huge * copysign(huge, x);
  }
  if (k > 0) {
    SET_HIGH_WORD(x, (hx & 0x800fffff) | (k << 20));
    return x;
  }
  if (k <= -54) {
    if (n > 50000) {
      return huge * copysign(huge, x);
    }
    return tiny * copysign(tiny, x);
  }
  k += 54;
  SET_HIGH_WORD(x, (hx & 0x800fffff) | (k << 20));
  return x * twom54;
}

double ldexp(double x, int n) {
  return scalbn(x, n);
}
