#include <errno.h>

#if defined(CRT_TARGET_OS_WINDOWS)
/*
 * Windows PE TLS needs runtime startup support such as _tls_index. Until the
 * TLS tranche exists, keep errno process-global on Windows freestanding tests.
 */
static int __crt_errno_value;
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
