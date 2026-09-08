#include <math.h>
#include <stdio.h>
#include <fenv.h>
#include <float.h>

static int fail(const char* message) {
  fprintf(stderr, "math_test: %s\n", message);
  return 1;
}

static int near_double(double a, double b) {
  double delta = fabs(a - b);
  return delta < 0.000001;
}

static int remquo_low_bits_match(int quotient, int expected) {
  return (quotient & 0x7) == (expected & 0x7);
}

int main(void) {
  double nan_value = NAN;
  double inf_value = INFINITY;
  double integral;
  float integralf;
  long double integrall;
  int exponent;
  int quotient;
  int gamma_sign;
  fexcept_t except_flag;
  fenv_t env;

  if (!isnan(nan_value) || !isinf(inf_value) || !isfinite(1.0) ||
      fpclassify(0.0) != FP_ZERO || fpclassify(inf_value) != FP_INFINITE) {
    return fail("classify");
  }
  if (fabs(-3.5) != 3.5 || fabsf(-2.0f) != 2.0f || fabsl(-4.0L) != 4.0L) {
    return fail("fabs");
  }
  if (copysign(2.0, -1.0) != -2.0 || copysign(-2.0, 1.0) != 2.0 ||
      !signbit(copysign(0.0, -1.0))) {
    return fail("copysign");
  }
  if (!near_double(fma(2.0, 3.0, 0.5), 6.5) ||
      fmaf(2.0f, 3.0f, 0.5f) != 6.5f ||
      (double)fmal(2.0L, 3.0L, 0.5L) != 6.5) {
    return fail("fma");
  }
  if (fmin(2.0, -1.0) != -1.0 || fmax(2.0, -1.0) != 2.0 ||
      fmin(NAN, 7.0) != 7.0 || fmax(7.0, NAN) != 7.0) {
    return fail("fmin/fmax");
  }
  if (!signbit(fmin(0.0, -0.0)) || signbit(fmax(0.0, -0.0))) {
    return fail("fmin/fmax signed zero");
  }
  if (floor(2.9) != 2.0 || floor(-2.1) != -3.0 ||
      ceil(2.1) != 3.0 || ceil(-2.9) != -2.0 ||
      trunc(2.9) != 2.0 || trunc(-2.9) != -2.0) {
    return fail("rounding primitives");
  }
  if (round(2.5) != 3.0 || round(-2.5) != -3.0 ||
      roundf(1.4f) != 1.0f || roundl(1.6L) != 2.0L) {
    return fail("round");
  }
  if (!(nextafter(1.0, 2.0) > 1.0) || !(nextafterf(1.0f, 2.0f) > 1.0f) ||
      !signbit(nextafter(0.0, -1.0)) || !signbit(nextafterf(0.0f, -1.0f)) ||
      nextafterl(1.0L, 1.0L) != 1.0L) {
    return fail("nextafter");
  }
  if (lrint(2.0) != 2 || lrintf(-2.0f) != -2 || lrintl(3.0L) != 3) {
    return fail("lrint");
  }
  if (lround(2.5) != 3 || lroundf(-2.5f) != -3 || lroundl(1.5L) != 2 ||
      llround(2.5) != 3 || llroundf(-2.5f) != -3 || llroundl(1.5L) != 2) {
    return fail("lround/llround");
  }
  if (!near_double(sqrt(4.0), 2.0) ||
      !near_double(sqrt(2.0) * sqrt(2.0), 2.0) ||
      sqrt(0.0) != 0.0 || !isnan(sqrt(-1.0)) ||
      sqrtf(9.0f) != 3.0f || sqrtl(16.0L) != 4.0L) {
    return fail("sqrt");
  }
  if (!near_double(cbrt(27.0), 3.0) || !near_double(cbrt(-8.0), -2.0) ||
      !near_double((double)cbrtf(27.0f), 3.0) ||
      !near_double((double)cbrtl(64.0L), 4.0) || cbrt(INFINITY) != INFINITY ||
      !isnan(cbrt(NAN))) {
    return fail("cbrt");
  }
  if (!signbit(sqrt(-0.0)) || sqrt(INFINITY) != INFINITY) {
    return fail("sqrt signed zero/inf");
  }
  if (!near_double(exp(0.0), 1.0) || !near_double(exp(1.0), 2.718281828459045) ||
      !near_double(exp(-1.0), 0.36787944117144233) ||
      exp(INFINITY) != INFINITY || exp(-INFINITY) != 0.0 ||
      !isnan(exp(NAN)) || expf(0.0f) != 1.0f || expl(0.0L) != 1.0L) {
    return fail("exp");
  }
  if (exp2(10.0) != 1024.0 || exp2f(3.0f) != 8.0f ||
      exp2l(4.0L) != 16.0L || exp2(-INFINITY) != 0.0 ||
      exp2(INFINITY) != INFINITY || !isnan(exp2(NAN))) {
    return fail("exp2");
  }
  if (!near_double(log(1.0), 0.0) || !near_double(log(2.718281828459045), 1.0) ||
      !near_double(log(exp(2.0)), 2.0) || log(INFINITY) != INFINITY ||
      !isinf(log(0.0)) || !signbit(log(0.0)) || !isnan(log(-1.0)) ||
      !isnan(log(NAN)) || logf(1.0f) != 0.0f || logl(1.0L) != 0.0L) {
    return fail("log");
  }
  if (!near_double(log10(1000.0), 3.0) || !near_double(log2(1024.0), 10.0) ||
      !isinf(log10(0.0)) || !signbit(log10(0.0)) || !isnan(log2(-1.0)) ||
      !near_double(log10f(100.0f), 2.0) || !near_double((double)log2l(8.0L), 3.0)) {
    return fail("log10/log2");
  }
  if (!near_double(expm1(1.0), exp(1.0) - 1.0) || expm1(0.0) != 0.0 ||
      !signbit(expm1(-0.0)) || expm1(INFINITY) != INFINITY ||
      !isnan(expm1(NAN)) || !near_double(log1p(1.0), log(2.0)) ||
      log1p(0.0) != 0.0 || !signbit(log1p(-0.0)) ||
      !isinf(log1p(-1.0)) || !signbit(log1p(-1.0)) || !isnan(log1p(-2.0)) ||
      !near_double(expm1f(1.0f), (float)(exp(1.0) - 1.0)) ||
      !near_double((double)log1pl(1.0L), log(2.0))) {
    return fail("expm1/log1p");
  }
  if (scalbn(1.5, 3) != 12.0 || scalbn(-2.0, -1) != -1.0 ||
      scalbn(0.0, 100) != 0.0 || !signbit(scalbn(-0.0, 100)) ||
      scalbn(INFINITY, -100) != INFINITY || !isnan(scalbn(NAN, 10)) ||
      scalbnf(1.25f, 2) != 5.0f || scalbnl(1.0L, 4) != 16.0L ||
      ldexp(1.5, 2) != 6.0 || ldexpf(0.5f, 3) != 4.0f ||
      ldexpl(2.0L, -1) != 1.0L) {
    return fail("scalbn/ldexp");
  }
  if (!near_double(pow(2.0, 10.0), 1024.0) ||
      !near_double(pow(9.0, 0.5), 3.0) ||
      !near_double(pow(2.0, -3.0), 0.125) ||
      pow(-2.0, 3.0) != -8.0 || pow(-2.0, 4.0) != 16.0 ||
      !isnan(pow(-2.0, 0.5)) || pow(0.0, 3.0) != 0.0 ||
      !isinf(pow(0.0, -1.0)) || pow(INFINITY, 2.0) != INFINITY ||
      powf(2.0f, 3.0f) != 8.0f || !near_double((double)powl(4.0L, 0.5L), 2.0)) {
    return fail("pow");
  }
  if (!near_double(sin(0.0), 0.0) || !signbit(sin(-0.0)) ||
      !near_double(sin(1.5707963267948966), 1.0) ||
      !near_double(sin(-1.5707963267948966), -1.0) ||
      !near_double(sinf(0.5f), (float)sin(0.5)) ||
      !near_double((double)sinl(0.5L), sin(0.5)) || !isnan(sin(INFINITY)) ||
      !isnan(sin(NAN))) {
    return fail("sin");
  }
  if (!near_double(cos(0.0), 1.0) ||
      !near_double(cos(3.141592653589793), -1.0) ||
      !near_double(cos(1.5707963267948966), 0.0) ||
      !near_double(cosf(0.5f), (float)cos(0.5)) ||
      !near_double((double)cosl(0.5L), cos(0.5)) || !isnan(cos(INFINITY)) ||
      !isnan(cos(NAN))) {
    return fail("cos");
  }
  if (!near_double(tan(0.0), 0.0) || !signbit(tan(-0.0)) ||
      !near_double(tan(0.7853981633974483), 1.0) ||
      !near_double(tan(-0.7853981633974483), -1.0) ||
      !near_double(tanf(0.5f), (float)tan(0.5)) ||
      !near_double((double)tanl(0.5L), tan(0.5)) || !isnan(tan(INFINITY)) ||
      !isnan(tan(NAN))) {
    return fail("tan");
  }
  if (!near_double(asin(0.5), 0.5235987755982989) ||
      !near_double((double)asinf(0.5f), 0.5235987901687622) ||
      !near_double((double)asinl(0.5L), 0.5235987755982989) ||
      !near_double(acos(0.5), 1.0471975511965979) ||
      !near_double((double)acosf(0.5f), 1.0471975803375244) ||
      !near_double((double)acosl(0.5L), 1.0471975511965979) ||
      !isnan(asin(2.0)) || !isnan(acos(2.0))) {
    return fail("asin/acos");
  }
  if (erf(0.0) != 0.0 || erfc(0.0) != 1.0 ||
      !near_double(erf(1.0), 0.8427007929497149) ||
      !near_double((double)erff(1.0f), 0.8427008) ||
      !near_double((double)erfl(1.0L), 0.8427007929497149) ||
      !near_double((double)erfcl(1.0L), 0.1572992070502851)) {
    return fail("erf/erfc");
  }
  if (fdim(5.0, 2.0) != 3.0 || fdim(2.0, 5.0) != 0.0 ||
      fdimf(5.0f, 2.0f) != 3.0f || fdiml(2.0L, 5.0L) != 0.0L ||
      hypot(3.0, 4.0) != 5.0 || hypotf(3.0f, 4.0f) != 5.0f ||
      hypotl(3.0L, 4.0L) != 5.0L || hypot(INFINITY, NAN) != INFINITY) {
    return fail("fdim/hypot");
  }
  if (ilogb(8.0) != 3 || ilogbf(0.5f) != -1 || ilogbl(16.0L) != 4 ||
      ilogb(0.0) != FP_ILOGB0 || ilogb(INFINITY) != INT_MAX ||
      ilogb(NAN) != FP_ILOGBNAN) {
    return fail("ilogb");
  }
  gamma_sign = 0;
  if (lgamma(1.0) != 0.0 || lgammaf(1.0f) != 0.0f ||
      lgammal(1.0L) != 0.0L || lgamma_r(1.0, &gamma_sign) != 0.0 ||
      gamma_sign != 1) {
    return fail("lgamma");
  }
  if (frexp(12.0, &exponent) != 0.75 || exponent != 4 ||
      frexp(-0.0, &exponent) != 0.0 || !signbit(frexp(-0.0, &exponent)) ||
      exponent != 0 || frexpf(8.0f, &exponent) != 0.5f || exponent != 4 ||
      frexpl(4.0L, &exponent) != 0.5L || exponent != 3) {
    return fail("frexp");
  }
  if (!near_double(modf(3.75, &integral), 0.75) || integral != 3.0 ||
      !near_double(modf(-3.75, &integral), -0.75) || integral != -3.0 ||
      !signbit(modf(-2.0, &integral)) || integral != -2.0 ||
      !isnan(modf(NAN, &integral)) || modf(INFINITY, &integral) != 0.0 ||
      integral != INFINITY || !near_double(modff(2.25f, &integralf), 0.25) ||
      integralf != 2.0f || !near_double((double)modfl(2.5L, &integrall), 0.5) ||
      integrall != 2.0L) {
    return fail("modf");
  }
  if (!near_double(fmod(7.0, 2.5), 2.0) || !near_double(fmod(-7.0, 2.5), -2.0) ||
      !signbit(fmod(-4.0, 2.0)) || fmod(1.0, INFINITY) != 1.0 ||
      !isnan(fmod(1.0, 0.0)) || !isnan(fmod(INFINITY, 1.0)) ||
      !near_double(fmodf(7.0f, 2.5f), 2.0) ||
      !near_double((double)fmodl(7.0L, 2.5L), 2.0)) {
    return fail("fmod");
  }
  if (!near_double(remainder(7.0, 2.5), -0.5) ||
      !near_double(remainder(6.0, 4.0), -2.0) ||
      !near_double(remainder(-7.0, 2.5), 0.5) || !signbit(remainder(-4.0, 2.0)) ||
      remainder(1.0, INFINITY) != 1.0 || !isnan(remainder(1.0, 0.0)) ||
      !isnan(remainder(INFINITY, 1.0)) || !near_double(remainderf(7.0f, 2.5f), -0.5) ||
      !near_double((double)remainderl(7.0L, 2.5L), -0.5)) {
    return fail("remainder");
  }
  if (!near_double(remquo(7.0, 2.5, &quotient), -0.5) ||
      !remquo_low_bits_match(quotient, 3) ||
      !near_double(remquo(-7.0, 2.5, &quotient), 0.5) ||
      !remquo_low_bits_match(quotient, -3) ||
      !near_double(remquof(7.0f, 2.5f, &quotient), -0.5) ||
      !remquo_low_bits_match(quotient, 3) ||
      !near_double((double)remquol(7.0L, 2.5L, &quotient), -0.5) ||
      !remquo_low_bits_match(quotient, 3) ||
      !isnan(remquo(1.0, 0.0, &quotient))) {
    return fail("remquo");
  }
#if LDBL_MANT_DIG > DBL_MANT_DIG
  if (expm1l(4.0L * LDBL_EPSILON) <= 0.0L ||
      log1pl(4.0L * LDBL_EPSILON) <= 0.0L ||
      modfl(9007199254740992.5L, &integrall) != 0.5L ||
      integrall != 9007199254740992.0L) {
    return fail("long double precision");
  }
#endif
  if (math_errhandling != 0 || fegetround() != FE_TONEAREST ||
      fesetround(99) == 0 || fesetround(FE_DOWNWARD) != 0 ||
      fegetround() != FE_DOWNWARD || fesetround(FE_UPWARD) != 0 ||
      fegetround() != FE_UPWARD || fesetround(FE_TOWARDZERO) != 0 ||
      fegetround() != FE_TOWARDZERO || fesetround(FE_TONEAREST) != 0 ||
      feclearexcept(FE_ALL_EXCEPT) != 0 || fetestexcept(FE_ALL_EXCEPT) != 0 ||
      feraiseexcept(FE_INVALID | FE_OVERFLOW) != 0 ||
      (fetestexcept(FE_ALL_EXCEPT) & (FE_INVALID | FE_OVERFLOW)) !=
          (FE_INVALID | FE_OVERFLOW) ||
      fegetexceptflag(&except_flag, FE_OVERFLOW) != 0 ||
      (except_flag & FE_OVERFLOW) == 0 ||
      feclearexcept(FE_INVALID | FE_OVERFLOW) != 0 ||
      fetestexcept(FE_INVALID | FE_OVERFLOW) != 0 ||
      fesetexceptflag(&except_flag, FE_OVERFLOW) != 0 ||
      (fetestexcept(FE_OVERFLOW) & FE_OVERFLOW) == 0 ||
      fegetenv(&env) != 0 || feholdexcept(&env) != 0 ||
      fetestexcept(FE_ALL_EXCEPT) != 0 || fesetenv(&env) != 0 ||
      fesetenv(FE_DFL_ENV) != 0 || fegetround() != FE_TONEAREST ||
      feupdateenv(FE_DFL_ENV) != 0) {
    return fail("fenv policy");
  }

  printf("math_test: ok\n");
  return 0;
}
