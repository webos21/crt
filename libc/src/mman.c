#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>

void* __crt_sys_mmap(void* addr, unsigned long length, int prot, int flags, int fd, long long offset);
long __crt_sys_munmap(void* addr, unsigned long length);

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
  void* result;
  intptr_t value;

  if (length == 0) {
    errno = EINVAL;
    return MAP_FAILED;
  }

  result = __crt_sys_mmap(addr, (unsigned long)length, prot, flags, fd, (long long)offset);
  value = (intptr_t)result;
  if (value < 0 && value >= -4095) {
    errno = (int)-value;
    return MAP_FAILED;
  }
  return result;
}

int munmap(void* addr, size_t length) {
  long result;

  if (length == 0) {
    errno = EINVAL;
    return -1;
  }

  result = __crt_sys_munmap(addr, (unsigned long)length);
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return (int)result;
}
