#include <errno.h>
#include <sys/sendfile.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <unistd.h>

/* sysinfo(2) and sendfile(2): Linux system calls that Bionic exposes through
 * <sys/sysinfo.h> and <sys/sendfile.h>. They have no counterpart on macOS or
 * Windows, where they fail with ENOSYS so a caller can fall back (WTF reads
 * total RAM through sysinfo only on Linux). */
#if defined(CRT_TARGET_OS_LINUX)
int sysinfo(struct sysinfo* info) {
  return syscall(SYS_sysinfo, (long)info) < 0 ? -1 : 0;
}

ssize_t sendfile(int out_fd, int in_fd, off_t* offset, size_t count) {
  return (ssize_t)syscall(SYS_sendfile, (long)out_fd, (long)in_fd, (long)offset, (long)count);
}
#else
int sysinfo(struct sysinfo* info) {
  (void)info;
  errno = ENOSYS;
  return -1;
}

ssize_t sendfile(int out_fd, int in_fd, off_t* offset, size_t count) {
  (void)out_fd;
  (void)in_fd;
  (void)offset;
  (void)count;
  errno = ENOSYS;
  return -1;
}
#endif
