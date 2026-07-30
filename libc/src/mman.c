#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/mman.h>

void* __crt_sys_mmap(void* addr, unsigned long length, int prot, int flags, int fd, long long offset);
long __crt_sys_mprotect(void* addr, unsigned long length, int prot);
long __crt_sys_munmap(void* addr, unsigned long length);
long __crt_sys_msync(void* addr, unsigned long length, int flags);
void* __crt_sys_mremap(void* old_addr, unsigned long old_size, unsigned long new_size, int flags, void* new_addr);
long __crt_sys_mlockall(int flags);
long __crt_sys_munlockall(void);
long __crt_sys_mlock(const void* addr, unsigned long length);
long __crt_sys_mlock2(const void* addr, unsigned long length, int flags);
long __crt_sys_munlock(const void* addr, unsigned long length);
long __crt_sys_mincore(void* addr, unsigned long length, unsigned char* vector);
long __crt_sys_madvise(void* addr, unsigned long length, int advice);

static int invalid_length(size_t length) {
  if (length == 0) {
    errno = EINVAL;
    return 1;
  }
  return 0;
}

static long normalize_long(long result) {
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return result;
}

static int host_mmap_flags(int flags) {
#if defined(CRT_TARGET_OS_MACOS)
  int host_flags = flags;

  host_flags &= ~(MAP_GROWSDOWN | MAP_DENYWRITE | MAP_EXECUTABLE | MAP_LOCKED |
                  MAP_POPULATE | MAP_NONBLOCK | MAP_STACK | MAP_HUGETLB);
  if ((host_flags & MAP_ANONYMOUS) != 0) {
    host_flags &= ~MAP_ANONYMOUS;
    host_flags |= 0x1000;
  }
  if ((host_flags & MAP_NORESERVE) != 0) {
    host_flags &= ~MAP_NORESERVE;
    host_flags |= 0x0040;
  }
  return host_flags;
#else
  return flags;
#endif
}

void* mmap64(void* addr, size_t length, int prot, int flags, int fd, off64_t offset) {
  void* result;
  intptr_t value;

  if (invalid_length(length)) {
    return MAP_FAILED;
  }

  result = __crt_sys_mmap(addr, (unsigned long)length, prot, host_mmap_flags(flags), fd, (long long)offset);
  value = (intptr_t)result;
  if (value < 0 && value >= -4095) {
    errno = (int)-value;
    return MAP_FAILED;
  }
  return result;
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
  return mmap64(addr, length, prot, flags, fd, (off64_t)offset);
}

int munmap(void* addr, size_t length) {
  long result;

  if (invalid_length(length)) {
    return -1;
  }

  result = __crt_sys_munmap(addr, (unsigned long)length);
  return (int)normalize_long(result);
}

int mprotect(void* addr, size_t length, int prot) {
  long result;

  if (invalid_length(length)) {
    return -1;
  }

  result = __crt_sys_mprotect(addr, (unsigned long)length, prot);
  return (int)normalize_long(result);
}

int msync(void* addr, size_t length, int flags) {
  long result;

  if (invalid_length(length)) {
    return -1;
  }
  if ((flags & ~(MS_ASYNC | MS_INVALIDATE | MS_SYNC)) != 0 || ((flags & MS_ASYNC) && (flags & MS_SYNC))) {
    errno = EINVAL;
    return -1;
  }
  result = __crt_sys_msync(addr, (unsigned long)length, flags);
  return (int)normalize_long(result);
}

void* mremap(void* old_addr, size_t old_size, size_t new_size, int flags, ...) {
  void* new_addr = 0;
  void* result;
  intptr_t value;

  if (old_size == 0 || new_size == 0) {
    errno = EINVAL;
    return MAP_FAILED;
  }
  if ((flags & MREMAP_FIXED) != 0) {
    va_list ap;
    va_start(ap, flags);
    new_addr = va_arg(ap, void*);
    va_end(ap);
  }

  result = __crt_sys_mremap(old_addr, (unsigned long)old_size, (unsigned long)new_size, flags, new_addr);
  value = (intptr_t)result;
  if (value < 0 && value >= -4095) {
    errno = (int)-value;
    return MAP_FAILED;
  }
  return result;
}

int mlockall(int flags) {
  long result;

  if ((flags & ~(MCL_CURRENT | MCL_FUTURE | MCL_ONFAULT)) != 0) {
    errno = EINVAL;
    return -1;
  }
  result = __crt_sys_mlockall(flags);
  return (int)normalize_long(result);
}

int munlockall(void) {
  return (int)normalize_long(__crt_sys_munlockall());
}

int mlock(const void* addr, size_t length) {
  if (invalid_length(length)) {
    return -1;
  }
  return (int)normalize_long(__crt_sys_mlock(addr, (unsigned long)length));
}

int mlock2(const void* addr, size_t length, int flags) {
  if (invalid_length(length)) {
    return -1;
  }
  return (int)normalize_long(__crt_sys_mlock2(addr, (unsigned long)length, flags));
}

int munlock(const void* addr, size_t length) {
  if (invalid_length(length)) {
    return -1;
  }
  return (int)normalize_long(__crt_sys_munlock(addr, (unsigned long)length));
}

int mincore(void* addr, size_t length, unsigned char* vector) {
  if (invalid_length(length)) {
    return -1;
  }
  if (vector == 0) {
    errno = EFAULT;
    return -1;
  }
  return (int)normalize_long(__crt_sys_mincore(addr, (unsigned long)length, vector));
}

int madvise(void* addr, size_t length, int advice) {
  if (invalid_length(length)) {
    return -1;
  }
  return (int)normalize_long(__crt_sys_madvise(addr, (unsigned long)length, advice));
}

int posix_madvise(void* addr, size_t length, int advice) {
  long result;

  if (length == 0) {
    return 0;
  }
  result = __crt_sys_madvise(addr, (unsigned long)length, advice);
  if (result < 0 && result >= -4095) {
    return (int)-result;
  }
  return (int)result;
}
