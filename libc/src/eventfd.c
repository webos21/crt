#include <errno.h>
#include <sys/eventfd.h>
#include <unistd.h>

#if defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_WINDOWS) || defined(CRT_TARGET_OS_MACOS)
/* Linux: a real eventfd2(2) syscall trampoline. Windows: a real, from-
 * scratch emulation (a Win32 Event HANDLE + a 64-bit counter, see
 * libc/src/arch/windows/common/syscall.c's own CRT_FD_KIND_EVENTFD/
 * __crt_sys_eventfd2() comments for the full read()/write()/poll()
 * semantics). macOS: a real, from-scratch emulation too, added
 * 2026-09-02 (a real pipe(2) pair as the underlying kernel object -- see
 * libc/src/fd.c's own "Real eventfd() emulation for macOS" comment,
 * right where __crt_sys_eventfd2() is defined for this host, for the
 * full design and read()/write()/close() semantics). All three share
 * this one name deliberately, so this dispatch stays a plain "which real
 * implementation" choice, not a stub-vs-real one. */
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
#if defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_WINDOWS) || defined(CRT_TARGET_OS_MACOS)
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
