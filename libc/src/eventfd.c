#include <errno.h>
#include <sys/eventfd.h>
#include <unistd.h>

#if defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_WINDOWS)
/* Linux: a real eventfd2(2) syscall trampoline. Windows: a real, from-
 * scratch emulation (a Win32 Event HANDLE + a 64-bit counter, see
 * libc/src/arch/windows/common/syscall.c's own CRT_FD_KIND_EVENTFD/
 * __crt_sys_eventfd2() comments for the full read()/write()/poll()
 * semantics) -- both share this one name deliberately, so this dispatch
 * stays a plain "which real implementation" choice, not a stub-vs-real
 * one. macOS still gets the ENOSYS stub below (no real or emulated
 * implementation there yet). */
long __crt_sys_eventfd2(unsigned int initval, int flags);

static int normalize_syscall_result(long result) {
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return (int)result;
}
#endif

int eventfd(unsigned int initval, int flags) {
#if defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_WINDOWS)
  return normalize_syscall_result(__crt_sys_eventfd2(initval, flags));
#else
  (void)initval;
  (void)flags;
  return __set_errno(ENOSYS);
#endif
}

int eventfd_read(int fd, eventfd_t* value) {
  if (value == 0) {
    errno = EFAULT;
    return -1;
  }
  if (read(fd, value, sizeof(*value)) != (ssize_t)sizeof(*value)) {
    return -1;
  }
  return 0;
}

int eventfd_write(int fd, eventfd_t value) {
  if (write(fd, &value, sizeof(value)) != (ssize_t)sizeof(value)) {
    return -1;
  }
  return 0;
}
