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

/* NOT __builtin_lrint()/__builtin_lround()/__builtin_llround() (or their
 * f/l variants) anywhere in this file: unlike fma()/fmaf() above (which do
 * compile straight to a single native FMADD instruction, confirmed via
 * generated assembly), clang lowers *every* one of these -- lrint, lrintf,
 * lrintl, lround, lroundf, lroundl, llround, llroundf, llroundl, on both
 * double and float, not just long double -- into a call to a runtime
 * support symbol with the exact same name as the builtin, regardless of
 * precision or `-fno-builtin`/`-ffreestanding` (plausibly because lrint()
 * specifically must honor the *current dynamic* fesetround() mode, which
 * has no single fixed-rounding-mode instruction to lower to at compile
 * time). Since this project defines that exact symbol name as this exact
 * function, every one of them was real, confirmed infinite self-recursion
 * (`clang -S` on each in isolation shows the entire body compiling down to
 * `bl <same name>` -- a real call, not fma()'s tail-call `b`, so this
 * class crashes with a stack overflow rather than spinning forever, but is
 * exactly as broken). Found for real via tests/math_test.c crashing
 * (SIGSEGV, not hanging) on a real Linux aarch64 host immediately after
 * the fmal() self-recursion fix above -- the first time any of these had
 * ever actually linked and run on Linux (macOS never exercises this
 * lowering path the same way). Reimplemented in terms of this project's
 * own already-real floor()/ceil()/trunc()/round() (their *l suffixed
 * variants are real too, see long_double.c/basic.c) plus fegetround(),
 * both already confirmed working (round() and fegetround() are each
 * checked earlier in math_test.c, both pass): lround()/llround() always
 * round half away from zero regardless of the current rounding mode (that
 * is round()'s own real, existing semantics, C99's), and lrint() honors
 * the current fesetround() mode for real. One known, documented precision
 * simplification: lrint()'s FE_TONEAREST case uses round()'s away-from-
 * zero tie-breaking rather than IEEE round-to-nearest-ties-to-even -- only
 * observable exactly on a .5 tie, matching this file's existing precision
 * bar elsewhere (see nextafterl() above), not a claim of bit-exact
 * rint() semantics. */
static long crt_lrint_round_current_mode(double x) {
  switch (fegetround()) {
    case FE_DOWNWARD:
      return (long)floor(x);
    case FE_UPWARD:
      return (long)ceil(x);
    case FE_TOWARDZERO:
      return (long)trunc(x);
    case FE_TONEAREST:
    default:
      return (long)round(x);
  }
}

long lrint(double x) {
  return crt_lrint_round_current_mode(x);
}

long lrintf(float x) {
  return crt_lrint_round_current_mode((double)x);
}

long lrintl(long double x) {
  switch (fegetround()) {
    case FE_DOWNWARD:
      return (long)floorl(x);
    case FE_UPWARD:
      return (long)ceill(x);
    case FE_TOWARDZERO:
      return (long)truncl(x);
    case FE_TONEAREST:
    default:
      return (long)roundl(x);
  }
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

double sqrt(double x) {
  return __builtin_elementwise_sqrt(x);
}

float sqrtf(float x) {
  return __builtin_elementwise_sqrt(x);
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
