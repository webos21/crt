#include <errno.h>
#include <stdarg.h>
#include <sys/prctl.h>

#if defined(CRT_TARGET_OS_LINUX)
long __crt_sys_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4,
                      unsigned long arg5);

static int normalize_syscall_result(long result) {
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return (int)result;
}
#endif

int prctl(int option, ...) {
#if defined(CRT_TARGET_OS_LINUX)
  va_list ap;
  unsigned long arg2;
  unsigned long arg3;
  unsigned long arg4;
  unsigned long arg5;

  /*
   * prctl() is a real variadic function in glibc/Bionic too, always
   * reading four more arguments regardless of how many the caller
   * actually passed -- options that don't need arg2..arg5 simply ignore
   * whatever garbage lands in the unused registers/stack slots. This
   * matches the real, long-established implementation technique both of
   * those libcs use, not a novel risk introduced here.
   */
  va_start(ap, option);
  arg2 = va_arg(ap, unsigned long);
  arg3 = va_arg(ap, unsigned long);
  arg4 = va_arg(ap, unsigned long);
  arg5 = va_arg(ap, unsigned long);
  va_end(ap);

  return normalize_syscall_result(__crt_sys_prctl(option, arg2, arg3, arg4, arg5));
#else
  (void)option;
  return __set_errno(ENOSYS);
#endif
}
