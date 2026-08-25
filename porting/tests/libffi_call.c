#include <stdio.h>

#include <ffi.h>

static int add_ints(int a, int b) {
  return a + b;
}

int main(void) {
  ffi_cif cif;
  ffi_type* args[2] = { &ffi_type_sint, &ffi_type_sint };
  int a = 19;
  int b = 23;
  void* values[2] = { &a, &b };
  ffi_arg result = 0;

  ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_sint, args);
  if (status != FFI_OK) {
    printf("libffi_call_test: prep failed (%d)\n", (int)status);
    return 1;
  }

  ffi_call(&cif, FFI_FN(add_ints), &result, values);
  if (result != 42) {
    printf("libffi_call_test: result mismatch (%d)\n", (int)result);
    return 1;
  }

  printf("libffi_call_test: ok result=%d\n", (int)result);
  return 0;
}
