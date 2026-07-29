#include <errno.h>

#include <private/crt_tls.h>

int* __errno(void) {
  return __crt_thread_errno();
}

int __set_errno(int value) {
  errno = value;
  return -1;
}
