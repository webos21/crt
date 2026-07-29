#include <stdio.h>
#include <float.h>

static int fail(const char* message) {
  fprintf(stderr, "data_model_test: %s\n", message);
  return 1;
}

int main(void) {
  if (sizeof(int) != 4) {
    return fail("int size");
  }
  if (sizeof(long long) != 8) {
    return fail("long long size");
  }
  if (sizeof(void*) != 8) {
    return fail("pointer size");
  }

  if (sizeof(long double) != __SIZEOF_LONG_DOUBLE__) {
    return fail("long double compiler size");
  }
  if (LDBL_MANT_DIG != __LDBL_MANT_DIG__) {
    return fail("long double compiler mantissa");
  }

#if defined(CRT_TARGET_OS_WINDOWS)
  if (sizeof(long) != 4) {
    return fail("windows long size");
  }
#else
  if (sizeof(long) != 8) {
    return fail("lp64 long size");
  }
#endif

#if defined(CRT_TARGET_OS_LINUX) && defined(__aarch64__)
  if (sizeof(long double) != 16 || LDBL_MANT_DIG != 113) {
    return fail("linux aarch64 long double ABI");
  }
#elif defined(CRT_TARGET_OS_WINDOWS)
  if (sizeof(long double) != 8 || LDBL_MANT_DIG != 53) {
    return fail("windows long double ABI");
  }
#elif defined(CRT_TARGET_OS_MACOS) && defined(__aarch64__)
  if (sizeof(long double) != 8 || LDBL_MANT_DIG != 53) {
    return fail("macos arm64 long double ABI");
  }
#elif defined(__x86_64__)
  if (sizeof(long double) != 16 ||
      (LDBL_MANT_DIG != 64 && LDBL_MANT_DIG != 113)) {
    return fail("x86_64 long double ABI");
  }
#endif

  printf("data_model_test: ok\n");
  return 0;
}
