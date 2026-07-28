#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* message) {
  fprintf(stderr, "stdlib_string_extra_test: %s\n", message);
  return 1;
}

static int compare_ints(const void* left, const void* right) {
  int a = *(const int*)left;
  int b = *(const int*)right;
  return (a > b) - (a < b);
}

int main(void) {
  char text[] = "alpha,beta,,gamma";
  char* save = 0;
  char* end = 0;
  char errbuf[64];
  int values[] = {5, 1, 4, 3, 2};
  int key = 4;
  int* found;

  if (strcmp(strtok_r(text, ",", &save), "alpha") != 0 ||
      strcmp(strtok_r(0, ",", &save), "beta") != 0 ||
      strcmp(strtok_r(0, ",", &save), "gamma") != 0 ||
      strtok_r(0, ",", &save) != 0) {
    return fail("strtok_r");
  }

  if (strstr(strerror(ENOENT), "file") == 0 ||
      strerror_r(EINVAL, errbuf, sizeof(errbuf)) != 0 ||
      strstr(errbuf, "Invalid") == 0) {
    return fail("strerror");
  }

  errno = 0;
  if (strtoll("-9223372036854775808", &end, 10) != LLONG_MIN ||
      *end != 0 || errno != 0) {
    return fail("strtoll min");
  }
  errno = 0;
  if (strtoull("18446744073709551615", &end, 10) != ULLONG_MAX ||
      *end != 0 || errno != 0) {
    return fail("strtoull max");
  }

  qsort(values, 5, sizeof(values[0]), compare_ints);
  if (values[0] != 1 || values[4] != 5) {
    return fail("qsort");
  }
  found = (int*)bsearch(&key, values, 5, sizeof(values[0]), compare_ints);
  if (found == 0 || *found != 4) {
    return fail("bsearch");
  }

  if (setenv("CRT_TEST_ENV", "first", 1) != 0 ||
      strcmp(getenv("CRT_TEST_ENV"), "first") != 0 ||
      setenv("CRT_TEST_ENV", "second", 0) != 0 ||
      strcmp(getenv("CRT_TEST_ENV"), "first") != 0 ||
      setenv("CRT_TEST_ENV", "second", 1) != 0 ||
      strcmp(getenv("CRT_TEST_ENV"), "second") != 0 ||
      unsetenv("CRT_TEST_ENV") != 0 ||
      getenv("CRT_TEST_ENV") != 0) {
    return fail("environment");
  }

  printf("stdlib_string_extra_test: ok\n");
  return 0;
}
