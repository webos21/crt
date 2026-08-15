#include <stdio.h>

#include <ffi.h>

/* Regression repro for the documented, previously-uncommitted libffi
 * register-corruption bug (see porting/recipes/libffi.json's own notes
 * and HISTORY.md's 2026-08-07 entry for the full trail): ffi_call() alone
 * and closures alone each work correctly in isolation, but calling
 * ffi_call() and then making any further libffi call in the same process
 * -- here, a second ffi_prep_cif(), the simplest possible "any further
 * call" -- reliably segfaulted, but only when the caller (this file) was
 * compiled at -O1/-O2, never -O0. Root-caused on aarch64 to a
 * callee-saved GPR (X19) that the caller's own compiled code trusts
 * AAPCS64 to preserve across ffi_call(), getting corrupted somewhere in
 * the ffi_call()/ffi_call_SYSV chain -- never isolated to an exact
 * instruction, and never re-tested on x86_64. This file recreates the
 * exact minimal repro from the recipe notes ("prep_cif, ffi_call
 * (add_ints), prep_cif again, no closures, compiled at -O1") as a
 * permanent, committed test instead of an uncommitted scratch file, so
 * it can run on every host (including CI's Windows arm64 leg) going
 * forward. porting/recipes/libffi.json's own test entries for this file
 * force -O1 explicitly via cflags, regardless of whatever optimization
 * level the surrounding build otherwise uses, since -O0 never reproduces
 * the bug. */
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
  int result = 0;
  ffi_status status;

  status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_sint, args);
  if (status != FFI_OK) {
    printf("libffi_repeat_call_test: first prep failed (%d)\n", (int)status);
    return 1;
  }

  ffi_call(&cif, FFI_FN(add_ints), &result, values);
  if (result != 42) {
    printf("libffi_repeat_call_test: result mismatch (%d)\n", result);
    return 1;
  }

  /* The documented crash trigger: any further libffi call after the one
   * above, with the caller compiled at -O1/-O2. A second, completely
   * independent ffi_prep_cif() (not reusing cif, not depending on args/
   * values still being valid) is the simplest possible "any further
   * call" -- if this segfaults, it confirms the bug still reproduces on
   * this host/architecture; if it returns FFI_OK cleanly, the bug is
   * fixed (or was never present) here. */
  status = ffi_prep_cif(&cif2, FFI_DEFAULT_ABI, 2, &ffi_type_sint, args);
  if (status != FFI_OK) {
    printf("libffi_repeat_call_test: second prep failed (%d)\n", (int)status);
    return 1;
  }

  printf("libffi_repeat_call_test: ok result=%d\n", result);
  return 0;
}
