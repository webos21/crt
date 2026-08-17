#include <errno.h>
#include <sys/timerfd.h>

#if defined(CRT_TARGET_OS_LINUX)
long __crt_sys_timerfd_create(int clockid, int flags);
long __crt_sys_timerfd_settime(
    int fd, int flags, const struct itimerspec* new_value, struct itimerspec* old_value);
long __crt_sys_timerfd_gettime(int fd, struct itimerspec* curr_value);

static int normalize_syscall_result(long result) {
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return (int)result;
}
#endif

int timerfd_create(int clockid, int flags) {
#if defined(CRT_TARGET_OS_LINUX)
  return normalize_syscall_result(__crt_sys_timerfd_create(clockid, flags));
#else
  (void)clockid;
  (void)flags;
  return __set_errno(ENOSYS);
#endif
}

int timerfd_settime(
    int fd, int flags, const struct itimerspec* new_value, struct itimerspec* old_value) {
  if (new_value == 0) {
    errno = EFAULT;
    return -1;
  }
#if defined(CRT_TARGET_OS_LINUX)
  return normalize_syscall_result(__crt_sys_timerfd_settime(fd, flags, new_value, old_value));
#else
  (void)fd;
  (void)flags;
  (void)old_value;
  return __set_errno(ENOSYS);
#endif
}

int timerfd_gettime(int fd, struct itimerspec* curr_value) {
  if (curr_value == 0) {
    errno = EFAULT;
    return -1;
  }
#if defined(CRT_TARGET_OS_LINUX)
  return normalize_syscall_result(__crt_sys_timerfd_gettime(fd, curr_value));
#else
  (void)fd;
  return __set_errno(ENOSYS);
#endif
}
