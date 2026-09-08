#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

struct sample {
  char c;
  int i;
  char tail;
};

static int fail(const char* message) {
  fprintf(stderr, "stddef_stdarg_test: %s\n", message);
  return 1;
}

static int sum_ints(int count, ...) {
  int i;
  int total = 0;
  va_list ap;

  va_start(ap, count);
  for (i = 0; i < count; ++i) {
    total += va_arg(ap, int);
  }
  va_end(ap);
  return total;
}

static int sum_ints_twice(int count, ...) {
  int i;
  int total = 0;
  va_list ap;
  va_list copy;

  va_start(ap, count);
  va_copy(copy, ap);
  for (i = 0; i < count; ++i) {
    total += va_arg(ap, int);
  }
  for (i = 0; i < count; ++i) {
    total += va_arg(copy, int);
  }
  va_end(copy);
  va_end(ap);
  return total;
}

int main(void) {
  char bytes[8];
  ptrdiff_t diff = &bytes[7] - &bytes[2];
  size_t size = sizeof(bytes);
  wchar_t wc = (wchar_t)'A';
  max_align_t aligned;

  if (NULL != (void*)0) {
    return fail("NULL");
  }
  if (diff != 5) {
    return fail("ptrdiff_t");
  }
  if (size != 8) {
    return fail("size_t");
  }
  if (wc != (wchar_t)'A') {
    return fail("wchar_t");
  }
  if (sizeof(aligned) < sizeof(long double)) {
    return fail("max_align_t");
  }
  if (offsetof(struct sample, i) <= offsetof(struct sample, c) ||
      offsetof(struct sample, tail) <= offsetof(struct sample, i)) {
    return fail("offsetof");
  }
  if (sum_ints(4, 1, 2, 3, 4) != 10) {
    return fail("va_list");
  }
  if (sum_ints_twice(3, 5, 6, 7) != 36) {
    return fail("va_copy");
  }

  printf("stddef_stdarg_test: ok\n");
  return 0;
}
