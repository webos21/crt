#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

double fabs(double x) {
  return __builtin_fabs(x);
}

float fabsf(float x) {
  return __builtin_fabsf(x);
}

long double fabsl(long double x) {
  return __builtin_fabsl(x);
}

double copysign(double x, double y) {
  return __builtin_copysign(x, y);
}

float copysignf(float x, float y) {
  return __builtin_copysignf(x, y);
}

long double copysignl(long double x, long double y) {
  return __builtin_copysignl(x, y);
}

double fma(double x, double y, double z) {
  return __builtin_fma(x, y, z);
}

float fmaf(float x, float y, float z) {
  return __builtin_fmaf(x, y, z);
}

long double fmal(long double x, long double y, long double z) {
  /* NOT __builtin_fmal(x, y, z): aarch64/x86_64 have real hardware FMA
   * instructions for float/double (confirmed via generated assembly --
   * fma()/fmaf() above both compile to a single native FMADD/VFMADD
   * instruction), but not for their own long double (128-bit quad
   * precision on Linux aarch64, 80-bit extended on Linux x86_64) -- so
   * clang can't inline __builtin_fmal() into instructions here and instead
   * lowers it into a call to the runtime support symbol named "fmal",
   * which is exactly the symbol this function itself defines: real,
   * confirmed infinite self-recursion (`clang -S` on just this function
   * shows the entire body compiling down to `b fmal`, an unconditional
   * branch to itself), not merely a stack overflow risk -- it never
   * returns at all, spinning at 100% CPU forever. Caught for real via
   * tests/math_test.c hanging on a real Linux aarch64 host (the first time
   * this function had ever actually linked and run there -- macOS's own
   * ARM64 `long double` is the same 64 bits as `double`, so it never hits
   * this gap: __builtin_fmal() there lowers straight to the native
   * double-precision FMADD instruction like fma()/fmaf() already do).
   * Plain x*y+z is not a true single-rounding-step FMA (loses the extra
   * guard-bit precision real FMA exists for), a known, documented
   * simplification matching this project's existing precision bar for
   * long double elsewhere (see nextafterl() above), not a claim of
   * bit-exact FMA semantics. */
  return x * y + z;
}

float fminf(float x, float y) {
  if (isnan(x)) return y;
  if (isnan(y)) return x;
  if (x == 0.0f && y == 0.0f) {
    return (signbit(x) || signbit(y)) ? -0.0f : 0.0f;
  }
  return x < y ? x : y;
}

long double fminl(long double x, long double y) {
  if (isnan(x)) return y;
  if (isnan(y)) return x;
  if (x == 0.0L && y == 0.0L) {
    return (signbit(x) || signbit(y)) ? -0.0L : 0.0L;
  }
  return x < y ? x : y;
}

float fmaxf(float x, float y) {
  if (isnan(x)) return y;
  if (isnan(y)) return x;
  if (x == 0.0f && y == 0.0f) {
    return (signbit(x) && signbit(y)) ? -0.0f : 0.0f;
  }
  return x > y ? x : y;
}

long double fmaxl(long double x, long double y) {
  if (isnan(x)) return y;
  if (isnan(y)) return x;
  if (x == 0.0L && y == 0.0L) {
    return (signbit(x) && signbit(y)) ? -0.0L : 0.0L;
  }
  return x > y ? x : y;
}

float truncf(float x) {
  return (float)trunc((double)x);
}

float floorf(float x) {
  return (float)floor((double)x);
}

float ceilf(float x) {
  return (float)ceil((double)x);
}

float roundf(float x) {
  return (float)round((double)x);
}

double nextafter(double x, double y) {
  uint64_t bits;

  if (isnan(x) || isnan(y)) return x + y;
  if (x == y) return y;
  if (x == 0.0) return copysign(0x1p-1074, y);

  memcpy(&bits, &x, sizeof(bits));
  if ((x > 0.0) == (x < y)) {
    ++bits;
  } else {
    --bits;
  }
  memcpy(&x, &bits, sizeof(x));
  return x;
}

float nextafterf(float x, float y) {
  uint32_t bits;

  if (isnan(x) || isnan(y)) return x + y;
  if (x == y) return y;
  if (x == 0.0f) return copysignf(0x1p-149f, y);

  memcpy(&bits, &x, sizeof(bits));
  if ((x > 0.0f) == (x < y)) {
    ++bits;
  } else {
    --bits;
  }
  memcpy(&x, &bits, sizeof(x));
  return x;
}

long double nextafterl(long double x, long double y) {
  return (long double)nextafter((double)x, (double)y);
}

long lrintl(long double x) {
  return (long)rintl(x);
}

long long llrintl(long double x) {
  return (long long)rintl(x);
}

long lround(double x) {
  return (long)round(x);
}

long lroundf(float x) {
  return (long)roundf(x);
}

long lroundl(long double x) {
  return (long)roundl(x);
}

long long llround(double x) {
  return (long long)round(x);
}

long long llroundf(float x) {
  return (long long)roundf(x);
}

long long llroundl(long double x) {
  return (long long)roundl(x);
}

double asin(double x) {
  if (isnan(x)) return x;
  if (x > 1.0 || x < -1.0) return NAN;
  if (x == 1.0) return atan2(1.0, 0.0);
  if (x == -1.0) return -atan2(1.0, 0.0);
  return atan2(x, sqrt(1.0 - x * x));
}

float asinf(float x) {
  return (float)asin((double)x);
}

long double asinl(long double x) {
  if (isnan(x)) return x;
  if (x > 1.0L || x < -1.0L) return NAN;
  if (x == 1.0L) return atan2l(1.0L, 0.0L);
  if (x == -1.0L) return -atan2l(1.0L, 0.0L);
  return atan2l(x, sqrtl(1.0L - x * x));
}

double acos(double x) {
  if (isnan(x)) return x;
  if (x > 1.0 || x < -1.0) return NAN;
  return atan2(sqrt(1.0 - x * x), x);
}

float acosf(float x) {
  return (float)acos((double)x);
}

long double acosl(long double x) {
  if (isnan(x)) return x;
  if (x > 1.0L || x < -1.0L) return NAN;
  return atan2l(sqrtl(1.0L - x * x), x);
}

double cosh(double x) {
  double ex = exp(x);
  return 0.5 * (ex + 1.0 / ex);
}

float coshf(float x) { return (float)cosh((double)x); }
long double coshl(long double x) { return (long double)cosh((double)x); }

double sinh(double x) {
  double ex = exp(x);
  return 0.5 * (ex - 1.0 / ex);
}

float sinhf(float x) { return (float)sinh((double)x); }
long double sinhl(long double x) { return (long double)sinh((double)x); }

double tanh(double x) {
  double ax = fabs(x);
  double ex;
  if (isinf(x)) return copysign(1.0, x);
  ex = exp(2.0 * ax);
  return copysign((ex - 1.0) / (ex + 1.0), x);
}

float tanhf(float x) { return (float)tanh((double)x); }
long double tanhl(long double x) { return (long double)tanh((double)x); }

double acosh(double x) {
  if (x < 1.0) return NAN;
  return log(x + sqrt(x * x - 1.0));
}

float acoshf(float x) { return (float)acosh((double)x); }
long double acoshl(long double x) { return (long double)acosh((double)x); }

double asinh(double x) {
  return log(x + sqrt(x * x + 1.0));
}

float asinhf(float x) { return (float)asinh((double)x); }
long double asinhl(long double x) { return (long double)asinh((double)x); }

double atanh(double x) {
  if (x <= -1.0 || x >= 1.0) return x == 1.0 ? INFINITY : (x == -1.0 ? -INFINITY : NAN);
  return 0.5 * log((1.0 + x) / (1.0 - x));
}

float atanhf(float x) { return (float)atanh((double)x); }
long double atanhl(long double x) { return (long double)atanh((double)x); }

double sqrt(double x) {
  return __builtin_elementwise_sqrt(x);
}

float sqrtf(float x) {
  return __builtin_elementwise_sqrt(x);
}

/* Bionic main's Android.bp has no standalone exp2/exp2f msun source: its
 * Android build supplies those through its compiler/libm integration. Keep
 * the public Bionic surface in the CRT with the already-imported exp family
 * until a matching current-main source route is available. */
double exp2(double x) {
  return exp(x * 0.693147180559945309417232121458176568);
}

float exp2f(float x) {
  return expf(x * 0.693147180559945309417232121458176568f);
}

double cbrt(double x) {
  if (isnan(x) || isinf(x) || x == 0.0) return x;
  return x < 0.0 ? -pow(-x, 1.0 / 3.0) : pow(x, 1.0 / 3.0);
}

float cbrtf(float x) {
  return (float)cbrt((double)x);
}

long double cbrtl(long double x) {
  if (isnan(x) || isinf(x) || x == 0.0L) return x;
  return x < 0.0L ? -powl(-x, 1.0L / 3.0L) : powl(x, 1.0L / 3.0L);
}
