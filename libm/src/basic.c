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

/*
 * NOT unconditionally __builtin_fma(x, y, z)/__builtin_fmaf(x, y, z): a
 * previous version of this comment claimed aarch64/x86_64 both have real
 * hardware FMA and that these always compile to a single native FMADD/
 * VFMADD instruction -- true for aarch64 (ARMv8's base ISA includes
 * FMADD/FMSUB unconditionally, no separate CPU feature needed), but WRONG
 * for x86_64: FMA3 is an *optional* x86_64 CPU feature (Haswell/2013+)
 * that Clang only assumes when the target explicitly enables it
 * (-mfma, or an -march= that implies it) via the __FMA__ predefine --
 * this project's build passes neither, on any of the three hosts, so
 * __FMA__ is undefined on every x86_64 build here. Without it,
 * __builtin_fma()/__builtin_fmaf() cannot lower to a real instruction and
 * fall back to a libcall -- to a symbol literally named "fma"/"fmaf",
 * exactly the functions defined here: real, stack-growing infinite self-
 * recursion (confirmed via objdump: the compiled body is `call fma`/
 * `call fmaf` targeting itself), not a hang like the long double case
 * below -- a hard STATUS_STACK_OVERFLOW crash instead. Caught for real via
 * tests/math_test.c crashing on this exact machine (Windows x86_64);
 * reasoned to apply identically to Linux/macOS x86_64 (the __FMA__ gap is
 * a compiler/architecture fact, not an OS-specific one -- neither host
 * passes -mfma either), not yet independently re-verified on either.
 *
 * Plain x*y+z is not a true single-rounding-step FMA (loses the extra
 * guard-bit precision real FMA exists for) -- a known, documented
 * simplification, matching this project's existing precision bar for
 * fmal() below and nextafterl() above, not a claim of bit-exact FMA
 * semantics. Real hardware FMA (via __builtin_fma/__builtin_fmaf) is still
 * used whenever the target genuinely has it: unconditionally on aarch64,
 * and on any x86_64 build that does someday enable __FMA__.
 */
#if defined(__FMA__) || defined(__aarch64__) || defined(_M_ARM64)
double fma(double x, double y, double z) {
  return __builtin_fma(x, y, z);
}

float fmaf(float x, float y, float z) {
  return __builtin_fmaf(x, y, z);
}
#else
double fma(double x, double y, double z) {
  return x * y + z;
}

float fmaf(float x, float y, float z) {
  return (float)((double)x * (double)y + (double)z);
}
#endif

long double fmal(long double x, long double y, long double z) {
  /* NOT __builtin_fmal(x, y, z): even on hosts where fma()/fmaf() above
   * get real hardware FMA, long double is a different, wider type
   * (128-bit quad precision on Linux aarch64, 80-bit extended on Linux/
   * macOS/Windows x86_64 -- except Windows, where long double is just
   * double again, LLP64) with no matching hardware instruction, so clang
   * can't inline __builtin_fmal() here either and instead lowers it into a
   * call to the runtime support symbol named "fmal", which is exactly the
   * symbol this function itself defines: real, confirmed infinite self-
   * recursion (`clang -S` on just this function shows the entire body
   * compiling down to `b fmal`, an unconditional branch to itself), not
   * merely a stack overflow risk -- it never returns at all, spinning at
   * 100% CPU forever. Caught for real via tests/math_test.c hanging on a
   * real Linux aarch64 host (the first time this function had ever
   * actually linked and run there -- macOS's own ARM64 `long double` is
   * the same 64 bits as `double`, so it never hits this gap:
   * __builtin_fmal() there lowers straight to the native double-precision
   * FMADD instruction like fma()/fmaf() already do there). Plain x*y+z is
   * not a true single-rounding-step FMA (loses the extra guard-bit
   * precision real FMA exists for), a known, documented simplification
   * matching this project's existing precision bar for long double
   * elsewhere (see nextafterl() above), not a claim of bit-exact FMA
   * semantics. */
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

/* atan2f: this project's own libm otherwise mirrors FreeBSD's own e_*.c/
 * e_*f.c double/float source pairs one-for-one (e_atan2.c is imported,
 * e_atan2f.c never was -- a real, pre-existing gap, unrelated to any
 * specific consumer, first actually hit 2026-08-23 by Skia's own
 * SkPathBuilder::arcTo()/SkComputeRadialSteps() calling it: `ld.lld: error:
 * undefined symbol: atan2f`). A cast-wrapper around the already-present,
 * already-verified double atan2(), matching this same file's own asinf/
 * acosf/coshf precedent just above, rather than porting a dedicated
 * single-precision FreeBSD algorithm this session has no way to verify. */
float atan2f(float y, float x) {
  return (float)atan2((double)y, (double)x);
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
