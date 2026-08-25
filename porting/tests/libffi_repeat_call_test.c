#include <stdio.h>

#include <ffi.h>

/* Keep the optimized repeated-call sequence as a regression test. libffi's
 * ffi_call contract requires rvalue storage to be at least one system
 * register wide even when the declared C return type is narrower. Using an
 * int here previously let the aarch64 backend write an ffi_arg-sized result
 * over adjacent stack state, producing optimization-dependent corruption and
 * SIGSEGV that was incorrectly attributed to a callee-saved register bug. */
static int add_ints(int a, int b) {
  return a + b;
}

int main(void) {
  ffi_cif cif;
  ffi_cif cif2;
  ffi_type* args[2] = { &ffi_type_sint, &ffi_type_sint };
  int a = 19;
  int b = 23;
  void* values[2] = { &a, &b };
  ffi_arg result = 0;
  ffi_status status;

  status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_sint, args);
  if (status != FFI_OK) {
    printf("libffi_repeat_call_test: first prep failed (%d)\n", (int)status);
    return 1;
  }

  ffi_call(&cif, FFI_FN(add_ints), &result, values);
  if (result != 42) {
    printf("libffi_repeat_call_test: result mismatch (%d)\n", (int)result);
    return 1;
  }

  /* Retain an independent second preparation call to catch damage to caller
   * state after an optimized ffi_call. */
  status = ffi_prep_cif(&cif2, FFI_DEFAULT_ABI, 2, &ffi_type_sint, args);
  if (status != FFI_OK) {
    printf("libffi_repeat_call_test: second prep failed (%d)\n", (int)status);
    return 1;
  }

  printf("libffi_repeat_call_test: ok result=%d\n", (int)result);
  return 0;
}
