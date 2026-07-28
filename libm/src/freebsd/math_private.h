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

#ifndef CRT_LIBM_FREEBSD_MATH_PRIVATE_H
#define CRT_LIBM_FREEBSD_MATH_PRIVATE_H

#include <stdint.h>

typedef uint32_t u_int32_t;

#define __weak_reference(sym, alias)
#define __strong_reference(sym, alias)

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define CRT_IEEE_WORD_ORDER_BIG 1
#else
#define CRT_IEEE_WORD_ORDER_BIG 0
#endif

#if CRT_IEEE_WORD_ORDER_BIG
typedef union {
  double value;
  struct {
    uint32_t msw;
    uint32_t lsw;
  } parts;
  struct {
    uint64_t w;
  } xparts;
} ieee_double_shape_type;
#else
typedef union {
  double value;
  struct {
    uint32_t lsw;
    uint32_t msw;
  } parts;
  struct {
    uint64_t w;
  } xparts;
} ieee_double_shape_type;
#endif

typedef union {
  float value;
  uint32_t word;
} ieee_float_shape_type;

#define EXTRACT_WORDS(ix0, ix1, d) \
  do { \
    ieee_double_shape_type ew_u; \
    ew_u.value = (d); \
    (ix0) = (int32_t)ew_u.parts.msw; \
    (ix1) = (int32_t)ew_u.parts.lsw; \
  } while (0)

#define GET_HIGH_WORD(i, d) \
  do { \
    ieee_double_shape_type gh_u; \
    gh_u.value = (d); \
    (i) = (int32_t)gh_u.parts.msw; \
  } while (0)

#define GET_LOW_WORD(i, d) \
  do { \
    ieee_double_shape_type gl_u; \
    gl_u.value = (d); \
    (i) = (int32_t)gl_u.parts.lsw; \
  } while (0)

#define SET_HIGH_WORD(d, v) \
  do { \
    ieee_double_shape_type sh_u; \
    sh_u.value = (d); \
    sh_u.parts.msw = (uint32_t)(v); \
    (d) = sh_u.value; \
  } while (0)

#define SET_LOW_WORD(d, v) \
  do { \
    ieee_double_shape_type sl_u; \
    sl_u.value = (d); \
    sl_u.parts.lsw = (uint32_t)(v); \
    (d) = sl_u.value; \
  } while (0)

#define INSERT_WORDS(d, ix0, ix1) \
  do { \
    ieee_double_shape_type iw_u; \
    iw_u.parts.msw = (uint32_t)(ix0); \
    iw_u.parts.lsw = (uint32_t)(ix1); \
    (d) = iw_u.value; \
  } while (0)

#define GET_FLOAT_WORD(i, f) \
  do { \
    ieee_float_shape_type gfw_u; \
    gfw_u.value = (f); \
    (i) = (int32_t)gfw_u.word; \
  } while (0)

#define SET_FLOAT_WORD(f, i) \
  do { \
    ieee_float_shape_type sfw_u; \
    sfw_u.word = (uint32_t)(i); \
    (f) = sfw_u.value; \
  } while (0)

#endif
