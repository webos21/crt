#include <math.h>
#include <stdio.h>

static int fail(const char* message) {
  fprintf(stderr, "math_test: %s\n", message);
  return 1;
}

static int near_double(double a, double b) {
  double delta = fabs(a - b);
  return delta < 0.000001;
}

int main(void) {
  double nan_value = NAN;
  double inf_value = INFINITY;

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
  if (!near_double(sqrt(4.0), 2.0) ||
      !near_double(sqrt(2.0) * sqrt(2.0), 2.0) ||
      sqrt(0.0) != 0.0 || !isnan(sqrt(-1.0)) ||
      sqrtf(9.0f) != 3.0f || sqrtl(16.0L) != 4.0L) {
    return fail("sqrt");
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

  printf("math_test: ok\n");
  return 0;
}
