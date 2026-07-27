#include <stdio.h>

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

#if defined(CRT_TARGET_OS_WINDOWS)
  if (sizeof(long) != 4) {
    return fail("windows long size");
  }
#else
  if (sizeof(long) != 8) {
    return fail("lp64 long size");
  }
#endif

  printf("data_model_test: ok\n");
  return 0;
}
