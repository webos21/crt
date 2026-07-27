#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static int fail(const char* message) {
  fprintf(stderr, "stdint_test: %s\n", message);
  return 1;
}

int main(void) {
  bool value = true;
  int8_t i8 = INT8_C(-1);
  uint8_t u8 = UINT8_C(255);
  int16_t i16 = INT16_C(-2);
  uint16_t u16 = UINT16_C(65535);
  int32_t i32 = INT32_C(-3);
  uint32_t u32 = UINT32_C(4294967295);
  int64_t i64 = INT64_C(-4);
  uint64_t u64 = UINT64_C(18446744073709551615);
  intptr_t ip = (intptr_t)(void*)&value;
  uintptr_t up = (uintptr_t)(void*)&value;

  if (!value || false) {
    return fail("stdbool values");
  }
  if (sizeof(int8_t) != 1 || sizeof(uint8_t) != 1 ||
      sizeof(int16_t) != 2 || sizeof(uint16_t) != 2 ||
      sizeof(int32_t) != 4 || sizeof(uint32_t) != 4 ||
      sizeof(int64_t) != 8 || sizeof(uint64_t) != 8) {
    return fail("fixed width sizes");
  }
  if (sizeof(intptr_t) != sizeof(void*) || sizeof(uintptr_t) != sizeof(void*)) {
    return fail("pointer width");
  }
  if (INT8_MIN != -128 || INT8_MAX != 127 || UINT8_MAX != 255) {
    return fail("8-bit limits");
  }
  if (INT16_MIN != -32768 || INT16_MAX != 32767 || UINT16_MAX != 65535) {
    return fail("16-bit limits");
  }
  if (INT32_MIN != (-2147483647 - 1) || INT32_MAX != 2147483647 ||
      UINT32_MAX != 4294967295U) {
    return fail("32-bit limits");
  }
  if (INT64_MIN != (INT64_C(-9223372036854775807) - INT64_C(1)) ||
      INT64_MAX != INT64_C(9223372036854775807) ||
      UINT64_MAX != UINT64_C(18446744073709551615)) {
    return fail("64-bit limits");
  }
  if (INTPTR_MAX <= 0 || UINTPTR_MAX < (uintptr_t)INTPTR_MAX || SIZE_MAX < UINTPTR_MAX) {
    return fail("pointer limits");
  }
  if (i8 != -1 || u8 != 255 || i16 != -2 || u16 != 65535 ||
      i32 != -3 || u32 != UINT32_MAX || i64 != -4 || u64 != UINT64_MAX ||
      ip == 0 || up == 0) {
    return fail("constant values");
  }

  printf("stdint_test: ok\n");
  return 0;
}
