#include <errno.h>
#include <limits.h>
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

int main(void) {
  char* end;

  if (expect_long(atoi("  -42xyz"), -42, "atoi") ||
      expect_long(atol("+123"), 123, "atol")) {
    return 1;
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
