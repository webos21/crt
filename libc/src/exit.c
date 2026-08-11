#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CRT_ATEXIT_MAX 32

void __crt_sys_exit(int status) __attribute__((noreturn));
void __cxa_finalize(void* dso) __attribute__((weak));
/* Only Linux currently defines __crt_run_fini_array()
 * (libc/src/arch/linux/common/init_fini_array.c, always linked into the
 * executable's own crt1 object) -- see that file's own comment for why
 * this is scoped to Linux for now, and for the statically-linked-only
 * caveat. Guarded by CRT_TARGET_OS_LINUX (not just a plain
 * __attribute__((weak)) declaration left to resolve to NULL, the way
 * __cxa_finalize above does) because that only works portably when the
 * weak symbol ends up genuinely satisfied by *something* in the link --
 * __cxa_finalize is: Apple's libSystem.dylib really provides it, so it's
 * weak-but-resolved on macOS. __crt_run_fini_array has no such fallback
 * definition anywhere outside Linux, and Mach-O's linker (unlike ELF)
 * does not silently bind a truly unresolved weak function reference to
 * NULL -- confirmed for real: macOS CI failed libc.dylib with "Undefined
 * symbols ... ___crt_run_fini_array" the first time this shipped without
 * the guard. */
#if defined(CRT_TARGET_OS_LINUX)
void __crt_run_fini_array(void) __attribute__((weak));
#endif

static void (*atexit_handlers[CRT_ATEXIT_MAX])(void);
static int atexit_count;

int atexit(void (*function)(void)) {
  if (function == 0 || atexit_count >= CRT_ATEXIT_MAX) {
    return -1;
  }
  atexit_handlers[atexit_count++] = function;
  return 0;
}

void exit(int status) {
  if (__cxa_finalize != 0) {
    __cxa_finalize(0);
  }
  while (atexit_count > 0) {
    void (*handler)(void) = atexit_handlers[--atexit_count];
    handler();
  }
#if defined(CRT_TARGET_OS_LINUX)
  if (__crt_run_fini_array != 0) {
    __crt_run_fini_array();
  }
#endif
  (void)fflush(0);
  __crt_sys_exit(status);
}

void _exit(int status) {
  __crt_sys_exit(status);
}
