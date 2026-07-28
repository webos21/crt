#include <stdio.h>
#include <unistd.h>

void __crt_sys_exit(int status) __attribute__((noreturn));

void exit(int status) {
  (void)fflush(0);
  __crt_sys_exit(status);
}

void _exit(int status) {
  __crt_sys_exit(status);
}
