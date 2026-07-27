#include <unistd.h>

void __crt_sys_exit(int status) __attribute__((noreturn));

void _exit(int status) {
  __crt_sys_exit(status);
}
