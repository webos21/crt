#include <float.h>
#include <fenv.h>
#include <limits.h>
#include <math.h>

static const long double crt_ld_ln2 =
    6.93147180559945309417232121458176568075500134360255e-1L;
static const long double crt_ld_inv_ln2 =
    1.4426950408889634073599246810018921374266459541530L;
static const long double crt_ld_ln10 =
    2.3025850929940456840179914546843642076011014886288L;
static const long double crt_ld_pi =
    3.1415926535897932384626433832795028841971693993751L;
static const long double crt_ld_pio2 =
    1.5707963267948966192313216916397514420985846996876L;
static const long double crt_ld_pio4 =
    7.8539816339744830961566084581987572104929234984378e-1L;
static const long double crt_ld_twopi =
    6.2831853071795864769252867665590057683943387987502L;

static long double crt_ld_abs(long double x) {
  return x < 0.0L ? -x : x;
}

static long double crt_ld_integer_threshold(void) {
  long double x = 1.0L;
  int i;

  for (i = 0; i < LDBL_MANT_DIG; ++i) {
    x *= 2.0L;
  }
  return x;
}

static long double crt_ld_trunc_abs(long double x) {
  long double bit = 1.0L;
  long double result = 0.0L;

  while (bit <= x * 0.5L) {
    long double next = bit * 2.0L;
    if (!isfinite(next)) {
      break;
    }
    bit = next;
  }
  while (bit >= 1.0L) {
    if (x >= bit) {
      x -= bit;
      result += bit;
    }
    bit *= 0.5L;
  }
  return result;
}

static long double crt_ld_scalbn_loop(long double x, int n) {
  if (n > 20000) {
    return x < 0.0L ? -(long double)INFINITY : (long double)INFINITY;
  }
  if (n < -20000) {
    return copysignl(0.0L, x);
  }
  while (n > 0) {
    x *= 2.0L;
    --n;
  }
  while (n < 0) {
    x *= 0.5L;
    ++n;
  }
  return x;
}

long double truncl(long double x) {
  long double ax;
  long double result;

  if (!isfinite(x) || x == 0.0L) {
    return x;
  }
  ax = crt_ld_abs(x);
  if (ax >= crt_ld_integer_threshold()) {
    return x;
  }
  result = crt_ld_trunc_abs(ax);
  return x < 0.0L ? -result : result;
}

long double floorl(long double x) {
  long double t;

  if (!isfinite(x) || x == 0.0L) {
    return x;
  }
  t = truncl(x);
  if (t > x) {
    t -= 1.0L;
  }
  return t;
}

long double ceill(long double x) {
  long double t;

  if (!isfinite(x) || x == 0.0L) {
    return x;
  }
  t = truncl(x);
  if (t < x) {
    t += 1.0L;
  }
  return t;
}

long double roundl(long double x) {
  if (!isfinite(x) || x == 0.0L) {
    return x;
  }
  return x < 0.0L ? ceill(x - 0.5L) : floorl(x + 0.5L);
}

long double scalbnl(long double x, int n) {
  if (!isfinite(x) || x == 0.0L || n == 0) {
    return x;
  }
  return crt_ld_scalbn_loop(x, n);
}

long double ldexpl(long double x, int n) {
  return scalbnl(x, n);
}

long double frexpl(long double x, int* exp) {
  long double ax;
  int e = 0;

  if (exp == 0) {
    return x;
  }
  if (x == 0.0L || !isfinite(x)) {
    *exp = 0;
    return x;
  }
  ax = crt_ld_abs(x);
  while (ax >= 1.0L) {
    ax *= 0.5L;
    ++e;
  }
  while (ax < 0.5L) {
    ax *= 2.0L;
    --e;
  }
  *exp = e;
  return x < 0.0L ? -ax : ax;
}

long double modfl(long double x, long double* iptr) {
  long double i = truncl(x);

  if (iptr != 0) {
    *iptr = i;
  }
  return x - i;
}

long double sqrtl(long double x) {
  long double guess;
  int i;

  if (isnan(x) || x == 0.0L || x == (long double)INFINITY) {
    return x;
  }
  if (x < 0.0L) {
    return (long double)NAN;
  }
  guess = x >= 1.0L ? x : 1.0L;
  for (i = 0; i < 80; ++i) {
    guess = 0.5L * (guess + x / guess);
  }
  return guess;
}

long double expl(long double x) {
  long double term;
  long double sum;
  long double r;
  int k;
  int i;

  if (isnan(x) || x == (long double)INFINITY) {
    return x;
  }
  if (x == -(long double)INFINITY) {
    return 0.0L;
  }
#if LDBL_MANT_DIG == DBL_MANT_DIG
  if (x > 7.09782712893383996732e2L) {
    return (long double)INFINITY;
  }
  if (x < -7.451332191019412076235e2L) {
    return 0.0L;
  }
#else
  if (x > 1.1356e4L) {
    return (long double)INFINITY;
  }
  if (x < -1.14e4L) {
    return 0.0L;
  }
#endif
  k = (int)(x * crt_ld_inv_ln2 + (x >= 0.0L ? 0.5L : -0.5L));
  r = x - (long double)k * crt_ld_ln2;
  term = 1.0L;
  sum = 1.0L;
  for (i = 1; i <= 48; ++i) {
    term *= r / (long double)i;
    sum += term;
    if (term == 0.0L) {
      break;
    }
  }
  return scalbnl(sum, k);
}

long double exp2l(long double x) {
  return expl(x * crt_ld_ln2);
}

long double rintl(long double x) {
#if LDBL_MANT_DIG == DBL_MANT_DIG
  return (long double)rint((double)x);
#else
  switch (fegetround()) {
    case FE_DOWNWARD: return floorl(x);
    case FE_UPWARD: return ceill(x);
    case FE_TOWARDZERO: return truncl(x);
    case FE_TONEAREST:
    default: return roundl(x);
  }
#endif
}

double nexttoward(double x, long double y) {
  return nextafter(x, (double)y);
}

float nexttowardf(float x, long double y) {
  return nextafterf(x, (float)y);
}

long double nexttowardl(long double x, long double y) {
  return nextafterl(x, y);
}

long double nanl(const char* tagp) {
#if LDBL_MANT_DIG == DBL_MANT_DIG
  return (long double)nan(tagp);
#else
  (void)tagp;
  return __builtin_nanl("");
#endif
}

long double logbl(long double x) {
  if (x == 0.0L) return -(long double)INFINITY;
  if (!isfinite(x)) return x * x;
  return (long double)ilogbl(x);
}

long double erfl(long double x) {
  return (long double)erf((double)x);
}

long double erfcl(long double x) {
  return (long double)erfc((double)x);
}

long double hypotl(long double x, long double y) {
  long double ax = crt_ld_abs(x);
  long double ay = crt_ld_abs(y);
  long double scale;

  if (isinf(ax) || isinf(ay)) return (long double)INFINITY;
  if (isnan(ax) || isnan(ay)) return ax + ay;
  scale = ax > ay ? ax : ay;
  if (scale == 0.0L) return 0.0L;
  ax /= scale;
  ay /= scale;
  return scale * sqrtl(ax * ax + ay * ay);
}

int ilogbl(long double x) {
  int exponent;

  if (x == 0.0L) return FP_ILOGB0;
  if (isnan(x)) return FP_ILOGBNAN;
  if (isinf(x)) return INT_MAX;
  (void)frexpl(crt_ld_abs(x), &exponent);
  return exponent - 1;
}

long double lgammal(long double x) {
  return (long double)lgamma((double)x);
}

long double lgammal_r(long double x, int* signgamp) {
  return (long double)lgamma_r((double)x, signgamp);
}

long double tgammal(long double x) {
  return (long double)tgamma((double)x);
}

long double expm1l(long double x) {
  if (crt_ld_abs(x) < 1.0e-8L) {
    long double term = x;
    long double sum = x;
    int i;

    for (i = 2; i <= 32; ++i) {
      term *= x / (long double)i;
      sum += term;
    }
    return sum;
  }
  return expl(x) - 1.0L;
}

long double logl(long double x) {
  long double m;
  long double z;
  long double z2;
  long double term;
  long double sum;
  int e;
  int i;

  if (isnan(x)) {
    return x;
  }
  if (x < 0.0L) {
    return (long double)NAN;
  }
  if (x == 0.0L) {
    return -(long double)INFINITY;
  }
  if (x == (long double)INFINITY) {
    return x;
  }
  m = frexpl(x, &e);
  if (m < 0.70710678118654752440084436210484903928483593768847L) {
    m *= 2.0L;
    --e;
  }
  z = (m - 1.0L) / (m + 1.0L);
  z2 = z * z;
  term = z;
  sum = z;
  for (i = 3; i <= 99; i += 2) {
    term *= z2;
    sum += term / (long double)i;
  }
  return 2.0L * sum + (long double)e * crt_ld_ln2;
}

long double log1pl(long double x) {
  if (crt_ld_abs(x) < 0.25L) {
    long double term = x;
    long double sum = x;
    int i;

    for (i = 2; i <= 80; ++i) {
      term *= -x * (long double)(i - 1) / (long double)i;
      sum += term;
    }
    return sum;
  }
  return logl(1.0L + x);
}

long double log10l(long double x) {
  return logl(x) / crt_ld_ln10;
}

long double log2l(long double x) {
  return logl(x) / crt_ld_ln2;
}

static long double crt_ld_reduce_twopi(long double x) {
  long double q;

  if (!isfinite(x)) {
    return x - x;
  }
  q = truncl(x / crt_ld_twopi);
  x -= q * crt_ld_twopi;
  if (x > crt_ld_pi) {
    x -= crt_ld_twopi;
  } else if (x < -crt_ld_pi) {
    x += crt_ld_twopi;
  }
  return x;
}

long double sinl(long double x) {
  long double term;
  long double sum;
  long double xx;
  int i;

  x = crt_ld_reduce_twopi(x);
  if (isnan(x)) {
    return x;
  }
  if (x > crt_ld_pio2) {
    x = crt_ld_pi - x;
  } else if (x < -crt_ld_pio2) {
    x = -crt_ld_pi - x;
  }
  xx = x * x;
  term = x;
  sum = x;
  for (i = 3; i <= 39; i += 2) {
    term *= -xx / ((long double)(i - 1) * (long double)i);
    sum += term;
  }
  return sum;
}

long double cosl(long double x) {
  long double term;
  long double sum;
  long double xx;
  int i;

  x = crt_ld_reduce_twopi(x);
  if (isnan(x)) {
    return x;
  }
  if (x > crt_ld_pio2) {
    return -cosl(crt_ld_pi - x);
  }
  if (x < -crt_ld_pio2) {
    return -cosl(-crt_ld_pi - x);
  }
  xx = x * x;
  term = 1.0L;
  sum = 1.0L;
  for (i = 2; i <= 40; i += 2) {
    term *= -xx / ((long double)(i - 1) * (long double)i);
    sum += term;
  }
  return sum;
}

long double tanl(long double x) {
  return sinl(x) / cosl(x);
}

/* atan(x) Taylor series, valid for |x| reasonably small -- range reduction
 * (crt_ld_atan_nonneg below) is what keeps the argument small enough for
 * this to converge well, the same division of labor sinl/cosl/logl above
 * already use (a plain truncated series plus simple range reduction, not a
 * bit-exact fdlibm-style implementation). */
static long double crt_ld_atan_series(long double x) {
  long double xx = x * x;
  long double term = x;
  long double sum = x;
  int i;

  for (i = 3; i <= 61; i += 2) {
    term *= -xx;
    sum += term / (long double)i;
  }
  return sum;
}

/* atan(x) for x >= 0. x > 1 reduces via atan(x) = pi/2 - atan(1/x); x in
 * (tan(pi/8), 1] reduces via the half-angle identity
 * atan(x) = pi/4 + atan((x-1)/(x+1)) so the series argument never exceeds
 * tan(pi/8) (~0.4142), where the plain series above already converges
 * well within this file's existing term-count budget. */
static long double crt_ld_atan_nonneg(long double x) {
  if (isinf(x)) {
    return crt_ld_pio2;
  }
  if (x > 1.0L) {
    return crt_ld_pio2 - crt_ld_atan_nonneg(1.0L / x);
  }
  if (x > 0.4142135623730950488016887242096980785696718753769L) {
    return crt_ld_pio4 + crt_ld_atan_series((x - 1.0L) / (x + 1.0L));
  }
  return crt_ld_atan_series(x);
}

long double atanl(long double x) {
  long double result;

  if (isnan(x) || x == 0.0L) {
    return x;
  }
  result = crt_ld_atan_nonneg(crt_ld_abs(x));
  return x < 0.0L ? -result : result;
}

long double atan2l(long double y, long double x) {
  long double result;

  /* This project's real Linux/aarch64/x86_64 long double is never the same
   * precision as double (LDBL_MANT_DIG != DBL_MANT_DIG on either target),
   * so the FreeBSD-derived double-precision atan2() in
   * libm/src/freebsd/e_atan2.c can't be reused via its own
   * __weak_reference(atan2, atan2l) fallback (only wired up for
   * LDBL_MANT_DIG == 53 hosts) -- confirmed via a real undefined-reference
   * link failure from basic.c's asinl()/acosl(), both of which already
   * called atan2l() assuming a real implementation existed. This is that
   * real implementation, following the same quadrant logic as the
   * double-precision one but built on atanl() above instead of bit-level
   * tricks. */
  if (isnan(x) || isnan(y)) {
    return x + y;
  }
  if (isinf(x) && isinf(y)) {
    result = x > 0.0L ? crt_ld_pio4 : (crt_ld_pio2 + crt_ld_pio4);
    return y < 0.0L ? -result : result;
  }
  if (isinf(y)) {
    return y < 0.0L ? -crt_ld_pio2 : crt_ld_pio2;
  }
  if (isinf(x)) {
    if (x > 0.0L) {
      return copysignl(0.0L, y);
    }
    return y < 0.0L ? -crt_ld_pi : crt_ld_pi;
  }
  if (x == 0.0L) {
    if (y == 0.0L) {
      if (signbit(x)) {
        return signbit(y) ? -crt_ld_pi : crt_ld_pi;
      }
      return y;
    }
    return y < 0.0L ? -crt_ld_pio2 : crt_ld_pio2;
  }
  if (y == 0.0L) {
    if (x > 0.0L) {
      return y;
    }
    return signbit(y) ? -crt_ld_pi : crt_ld_pi;
  }
  result = crt_ld_atan_nonneg(crt_ld_abs(y / x));
  if (x > 0.0L) {
    return y < 0.0L ? -result : result;
  }
  return y < 0.0L ? result - crt_ld_pi : crt_ld_pi - result;
}

static int crt_ld_is_integer(long double x, long double* iptr) {
  long double i = truncl(x);

  if (iptr != 0) {
    *iptr = i;
  }
  return i == x;
}

long double fmodl(long double x, long double y) {
  long double q;

  if (isnan(x) || isnan(y) || y == 0.0L || isinf(x)) {
    return (long double)NAN;
  }
  if (isinf(y)) {
    return x;
  }
  q = truncl(x / y);
  return x - q * y;
}

long double powl(long double x, long double y) {
  long double iy;
  long double result;
  int negative = 0;

  if (y == 0.0L) {
    return 1.0L;
  }
  if (x == 1.0L) {
    return 1.0L;
  }
  if (x < 0.0L) {
    if (!crt_ld_is_integer(y, &iy)) {
      return (long double)NAN;
    }
    negative = fmodl(crt_ld_abs(iy), 2.0L) == 1.0L;
    x = -x;
  }
  result = expl(y * logl(x));
  return negative ? -result : result;
}

long double remainderl(long double x, long double y) {
  long double q;
  long double n;

  if (isnan(x) || isnan(y) || y == 0.0L || isinf(x)) {
    return (long double)NAN;
  }
  if (isinf(y)) {
    return x;
  }
  q = x / y;
  n = q < 0.0L ? ceill(q - 0.5L) : floorl(q + 0.5L);
  if (crt_ld_abs(q - n) == 0.5L && fmodl(crt_ld_abs(n), 2.0L) == 1.0L) {
    n += n < q ? 1.0L : -1.0L;
  }
  return x - n * y;
}

long double remquol(long double x, long double y, int* quo) {
  long double q;
  long double r;

  if (quo != 0) {
    *quo = 0;
  }
  if (isnan(x) || isnan(y) || y == 0.0L || isinf(x)) {
    return (long double)NAN;
  }
  q = x / y;
  q = q < 0.0L ? ceill(q - 0.5L) : floorl(q + 0.5L);
  if (quo != 0) {
    long double aq = crt_ld_abs(q);
    int bits = 0;
    int bit = 1;

    while (aq >= 1.0L && bit <= 4) {
      if (fmodl(aq, 2.0L) >= 1.0L) {
        bits |= bit;
      }
      aq = truncl(aq * 0.5L);
      bit <<= 1;
    }
    *quo = q < 0.0L ? -bits : bits;
  }
  r = x - q * y;
  return r;
}
