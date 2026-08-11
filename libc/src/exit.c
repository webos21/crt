#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CRT_ATEXIT_MAX 32

void __crt_sys_exit(int status) __attribute__((noreturn));
void __cxa_finalize(void* dso) __attribute__((weak));
/* Weak: only Linux currently defines a strong __crt_run_fini_array()
 * (libc/src/arch/linux/common/init_fini_array.c, always linked into the
 * executable's own crt1 object) -- see that file's own comment for why
 * this is scoped to Linux for now, and for the statically-linked-only
 * caveat. macOS/Windows leave this an unresolved weak reference, so the
 * call below is simply skipped there, same as before this existed. */
void __crt_run_fini_array(void) __attribute__((weak));

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
  if (__crt_run_fini_array != 0) {
    __crt_run_fini_array();
  }
  (void)fflush(0);
  __crt_sys_exit(status);
}

void _exit(int status) {
  __crt_sys_exit(status);
}
