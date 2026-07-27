#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>

long __crt_sys_read(int fd, void* buf, unsigned long count);
long __crt_sys_write(int fd, const void* buf, unsigned long count);
long __crt_sys_open(const char* path, int flags, unsigned int mode);
long __crt_sys_close(int fd);
long long __crt_sys_lseek(int fd, long long offset, int whence);

static long normalize_syscall_result(long result) {
  if (result < 0 && result >= -4095) {
    return __set_errno((int)-result);
  }
  return result;
}

ssize_t read(int fd, void* buf, size_t count) {
  return (ssize_t)normalize_syscall_result(__crt_sys_read(fd, buf, (unsigned long)count));
}

ssize_t write(int fd, const void* buf, size_t count) {
  return (ssize_t)normalize_syscall_result(__crt_sys_write(fd, buf, (unsigned long)count));
}

int open(const char* path, int flags, ...) {
  unsigned int mode = 0;
  va_list args;

  if ((flags & O_CREAT) != 0) {
    va_start(args, flags);
    mode = (unsigned int)va_arg(args, int);
    va_end(args);
  }

  return (int)normalize_syscall_result(__crt_sys_open(path, flags, mode));
}

int close(int fd) {
  return (int)normalize_syscall_result(__crt_sys_close(fd));
}

off_t lseek(int fd, off_t offset, int whence) {
  long long result = __crt_sys_lseek(fd, (long long)offset, whence);
  if (result < 0 && result >= -4095) {
    return (off_t)__set_errno((int)-result);
  }
  return (off_t)result;
}
