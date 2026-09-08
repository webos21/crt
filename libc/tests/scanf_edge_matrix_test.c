#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* message) {
  fprintf(stderr, "scanf_edge_matrix_test: %s\n", message);
  return 1;
}

static int expect_match_failure(void) {
  int value = 1234;
  int consumed = -1;

  if (sscanf("x", "%d%n", &value, &consumed) != 0 || value != 1234 || consumed != -1) {
    return fail("decimal match failure");
  }
  if (sscanf("+", "%d%n", &value, &consumed) != 0 || value != 1234 || consumed != -1) {
    return fail("sign-only match failure");
  }
  if (sscanf("x", "y%n", &consumed) != 0 || consumed != -1) {
    return fail("literal match failure");
  }
  return 0;
}

static int expect_input_failure(void) {
  int value = 1234;
  char word[8];

  if (sscanf("", "%d", &value) != EOF || value != 1234) {
    return fail("empty integer input failure");
  }
  if (sscanf("   ", "%7s", word) != EOF) {
    return fail("space-only string input failure");
  }
  if (sscanf("", "%1c", word) != EOF) {
    return fail("empty char input failure");
  }
  return 0;
}

static int expect_partial_conversions(void) {
  int first = 0;
  int second = 77;
  char chars[4] = {0, 0, 0, 0};
  int consumed = -1;

  if (sscanf("123x", "%d%d", &first, &second) != 1 || first != 123 || second != 77) {
    return fail("partial integer conversion");
  }
  if (sscanf("ab", "%3c%n", chars, &consumed) != 1 ||
      memcmp(chars, "ab", 2) != 0 || consumed != 2) {
    return fail("partial char conversion");
  }
  return 0;
}

static int expect_prefix_edges(void) {
  int value = -1;
  int consumed = -1;
  int next = 0;

  if (sscanf("0x", "%i%n", &value, &consumed) != 1 || value != 0 || consumed != 1) {
    return fail("hex prefix without digits");
  }
  value = -1;
  consumed = -1;
  if (sscanf("0b", "%i%n", &value, &consumed) != 1 || value != 0 || consumed != 1) {
    return fail("binary prefix without digits");
  }
  value = -1;
  next = 0;
  if (sscanf("0b2", "%i%d", &value, &next) != 1 || value != 0 || next != 0) {
    return fail("binary prefix pushback");
  }
  return 0;
}

static int expect_scanset_edges(void) {
  char set[16];
  int consumed = -1;

  memset(set, 0, sizeof(set));
  if (sscanf("]", "%[]]%n", set, &consumed) != 1 ||
      strcmp(set, "]") != 0 || consumed != 1) {
    return fail("scanset leading bracket");
  }
  memset(set, 0, sizeof(set));
  if (sscanf("]-", "%[]-]%n", set, &consumed) != 1 ||
      strcmp(set, "]-") != 0 || consumed != 2) {
    return fail("scanset leading bracket dash");
  }
  memset(set, 0, sizeof(set));
  if (sscanf("abc1", "%[^0-9]%n", set, &consumed) != 1 ||
      strcmp(set, "abc") != 0 || consumed != 3) {
    return fail("scanset negated range");
  }
  memset(set, 0, sizeof(set));
  if (sscanf("bdf", "%[a-c-e]%n", set, &consumed) != 1 ||
      strcmp(set, "bd") != 0 || consumed != 2) {
    return fail("scanset chained range");
  }
  return 0;
}

static int expect_bionic_modifiers(void) {
  signed char i8 = 0;
  short i16 = 0;
  int i32 = 0;
  long long i64 = 0;
  unsigned int binary = 0;
  void* pointer = 0;

  if (sscanf("-8 -16 -32 -64", "%w8d %w16d %w32d %w64d", &i8, &i16, &i32, &i64) != 4 ||
      i8 != -8 || i16 != -16 || i32 != -32 || i64 != -64) {
    return fail("bionic w modifiers");
  }
  if (sscanf("0b1010 0x1234", "%b %p", &binary, &pointer) != 2 ||
      binary != 10 || pointer != (void*)(uintptr_t)0x1234) {
    return fail("bionic binary pointer");
  }
  return 0;
}

static int expect_float_edges(void) {
  double d = 0.0;
  long double ld = 0.0L;
  int consumed = -1;

  if (sscanf("x", "%la%n", &d, &consumed) != 0 || consumed != -1) {
    return fail("float match failure");
  }
  if (sscanf("", "%la", &d) != EOF) {
    return fail("float input failure");
  }
  if (sscanf("0x1.8p2z", "%la%n", &d, &consumed) != 1 ||
      d < 5.99 || d > 6.01 || consumed != 7) {
    return fail("hex float consumed");
  }
  if (sscanf("nan(payload)!", "%La%n", &ld, &consumed) != 1 || consumed != 12) {
    return fail("nan payload consumed");
  }
  return 0;
}

int main(void) {
  if (expect_match_failure() != 0 ||
      expect_input_failure() != 0 ||
      expect_partial_conversions() != 0 ||
      expect_prefix_edges() != 0 ||
      expect_scanset_edges() != 0 ||
      expect_bionic_modifiers() != 0 ||
      expect_float_edges() != 0) {
    return 1;
  }

  printf("scanf_edge_matrix_test: ok\n");
  return 0;
}
