#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

long __crt_sys_read(int fd, void* buf, unsigned long count);
long __crt_sys_write(int fd, const void* buf, unsigned long count);
long __crt_sys_open(const char* path, int flags, unsigned int mode);
long __crt_sys_close(int fd);
long long __crt_sys_lseek(int fd, long long offset, int whence);
long __crt_sys_access(const char* path, int mode);
long __crt_sys_mkdir(const char* path, unsigned int mode);
long __crt_sys_rmdir(const char* path);
long __crt_sys_chdir(const char* path);
#if !defined(CRT_TARGET_OS_MACOS)
long __crt_sys_getcwd(char* buf, unsigned long size);
#endif
long __crt_sys_dup(int oldfd);
long __crt_sys_dup2(int oldfd, int newfd);

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

int access(const char* path, int mode) {
  if (path == 0 || (mode & ~(R_OK | W_OK | X_OK)) != 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_access(path, mode));
}

int mkdir(const char* path, mode_t mode) {
  if (path == 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_mkdir(path, (unsigned int)mode));
}

int rmdir(const char* path) {
  if (path == 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_rmdir(path));
}

int chdir(const char* path) {
  if (path == 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_chdir(path));
}

#if !defined(CRT_TARGET_OS_MACOS)
char* getcwd(char* buf, size_t size) {
  long result;

  if (buf == 0 || size == 0) {
    __set_errno(EINVAL);
    return 0;
  }
  result = __crt_sys_getcwd(buf, (unsigned long)size);
  if (result < 0 && result >= -4095) {
    __set_errno((int)-result);
    return 0;
  }
  return buf;
}
#endif

int dup(int oldfd) {
  return (int)normalize_syscall_result(__crt_sys_dup(oldfd));
}

int dup2(int oldfd, int newfd) {
  if (newfd < 0) {
    return (int)__set_errno(EBADF);
  }
  return (int)normalize_syscall_result(__crt_sys_dup2(oldfd, newfd));
}

int stat(const char* path, struct stat* st) {
  int fd;
  off_t current;
  off_t end;

  if (path == 0 || st == 0) {
    return (int)__set_errno(EINVAL);
  }
  fd = open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  memset(st, 0, sizeof(*st));
  st->st_mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  st->st_nlink = 1;
  st->st_blksize = 4096;
  current = lseek(fd, 0, SEEK_CUR);
  end = lseek(fd, 0, SEEK_END);
  if (end >= 0) {
    st->st_size = end;
    st->st_blocks = (end + 511) / 512;
  }
  if (current >= 0) {
    (void)lseek(fd, current, SEEK_SET);
  }
  close(fd);
  return 0;
}
