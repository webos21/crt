#include <errno.h>
#include <sys/eventfd.h>
#include <unistd.h>

#if defined(CRT_TARGET_OS_LINUX)
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
#if defined(CRT_TARGET_OS_LINUX)
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
