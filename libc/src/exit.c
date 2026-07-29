#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CRT_ATEXIT_MAX 32

void __crt_sys_exit(int status) __attribute__((noreturn));
void __cxa_finalize(void* dso) __attribute__((weak));

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
  (void)fflush(0);
  __crt_sys_exit(status);
}

void _exit(int status) {
  __crt_sys_exit(status);
}
