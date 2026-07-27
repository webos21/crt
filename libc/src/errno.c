#include <errno.h>

#if defined(_WIN32)
__declspec(thread) static int __crt_errno_value;
#else
static __thread int __crt_errno_value;
#endif

int* __errno(void) {
  return &__crt_errno_value;
}

int __set_errno(int value) {
  errno = value;
  return -1;
}
