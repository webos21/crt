#include <unistd.h>

long __crt_sys_write(int fd, const void* buf, unsigned long count);

ssize_t write(int fd, const void* buf, size_t count) {
  return (ssize_t)__crt_sys_write(fd, buf, (unsigned long)count);
}
