#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int fail(const char* message) {
  fprintf(stderr, "stdlib_test: %s\n", message);
  return 1;
}

static int expect_long(long actual, long expected, const char* message) {
  if (actual != expected) {
    return fail(message);
  }
  return 0;
}

static int expect_ulong(unsigned long actual, unsigned long expected, const char* message) {
  if (actual != expected) {
    return fail(message);
  }
  return 0;
}

static int expect_double_near(double actual, double expected, const char* message) {
  double diff = actual > expected ? actual - expected : expected - actual;
  if (diff > 0.000001) {
    return fail(message);
  }
  return 0;
}

int main(void) {
  char* end;

  if (expect_long(atoi("  -42xyz"), -42, "atoi") ||
      expect_long(atol("+123"), 123, "atol")) {
    return 1;
  }

  if (expect_long(abs(-7), 7, "abs") ||
      expect_long(labs(-70000L), 70000L, "labs") ||
      expect_double_near(atof("-12.25"), -12.25, "atof")) {
    return 1;
  }
  if (llabs(-9000000000LL) != 9000000000LL) {
    return fail("llabs");
  }

  if (div(7, 3).quot != 2 || div(7, 3).rem != 1 ||
      ldiv(-7L, 3L).quot != -2 || ldiv(-7L, 3L).rem != -1 ||
      lldiv(9LL, -4LL).quot != -2 || lldiv(9LL, -4LL).rem != 1) {
    return fail("div family");
  }

  if (expect_double_near(strtod("1.5e2tail", &end), 150.0, "strtod") ||
      *end != 't') {
    return fail("strtod end");
  }

  if (expect_double_near(strtod("0x1.8p2z", &end), 6.0, "strtod hex") ||
      *end != 'z') {
    return fail("strtod hex end");
  }

  if (!isnan(strtod("nan(payload)!", &end)) || *end != '!') {
    return fail("strtod nan");
  }

  if (strtod("-inf!", &end) != -INFINITY || *end != '!') {
    return fail("strtod inf");
  }

  if (strtod("9007199254740993", &end) != 9007199254740992.0 ||
      *end != '\0') {
    return fail("strtod round to even");
  }

  if (strtod("2.2250738585072014e-308", &end) != DBL_MIN ||
      *end != '\0') {
    return fail("strtod dbl_min");
  }

  if (strtod("0x1p-1074", &end) != 0x1p-1074 || *end != '\0') {
    return fail("strtod subnormal hex");
  }

  errno = 0;
  if (strtod("1e9999", &end) != INFINITY || errno != ERANGE) {
    return fail("strtod overflow");
  }

  errno = 0;
  if (strtod("1e-9999", &end) != 0.0 || errno != ERANGE) {
    return fail("strtod underflow");
  }

  if (strtof("3.5", &end) != 3.5f || *end != '\0') {
    return fail("strtof");
  }

  if (strtof("16777217", &end) != 16777216.0f || *end != '\0') {
    return fail("strtof round to even");
  }

  if (strtof("1.17549435e-38", &end) != FLT_MIN || *end != '\0') {
    return fail("strtof flt_min");
  }

  if (strtold("2.25", &end) != 2.25L || *end != '\0') {
    return fail("strtold");
  }

  errno = 0;
  if (expect_long(strtol("  -123tail", &end, 10), -123, "strtol decimal") ||
      *end != 't' || errno != 0) {
    return fail("strtol decimal end");
  }

  if (expect_long(strtol("077", &end, 0), 63, "strtol octal") ||
      *end != '\0') {
    return fail("strtol octal end");
  }

  if (expect_long(strtol("0x1fZ", &end, 0), 31, "strtol hex") ||
      *end != 'Z') {
    return fail("strtol hex end");
  }

  if (expect_long(strtol("z", &end, 36), 35, "strtol base36") ||
      *end != '\0') {
    return fail("strtol base36 end");
  }

  if (strtol("no digits", &end, 10) != 0 || end == 0 || *end != 'n') {
    return fail("strtol no digits");
  }

  errno = 0;
  if (strtol("999999999999999999999999999999", &end, 10) != LONG_MAX ||
      errno != ERANGE) {
    return fail("strtol overflow");
  }

  errno = 0;
  if (strtol("-999999999999999999999999999999", &end, 10) != LONG_MIN ||
      errno != ERANGE) {
    return fail("strtol underflow");
  }

  errno = 0;
  if (strtol("123", &end, 1) != 0 || end == 0 || *end != '1' || errno != EINVAL) {
    return fail("strtol invalid base");
  }

  errno = 0;
  if (expect_ulong(strtoul("429", &end, 10), 429UL, "strtoul decimal") ||
      *end != '\0' || errno != 0) {
    return fail("strtoul decimal end");
  }

  if (expect_ulong(strtoul("0xff", &end, 0), 255UL, "strtoul hex") ||
      *end != '\0') {
    return fail("strtoul hex end");
  }

  if (expect_ulong(strtoul("-1", &end, 10), ULONG_MAX, "strtoul negative") ||
      *end != '\0') {
    return fail("strtoul negative end");
  }

  errno = 0;
  if (strtoul("999999999999999999999999999999", &end, 10) != ULONG_MAX ||
      errno != ERANGE) {
    return fail("strtoul overflow");
  }

  printf("stdlib_test: ok\n");
  return 0;
}
