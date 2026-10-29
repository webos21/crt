#include <errno.h>
#include <sys/sendfile.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <unistd.h>

/* sysinfo(2) and sendfile(2): Linux system calls that Bionic exposes through
 * <sys/sysinfo.h> and <sys/sendfile.h>. sysinfo has no counterpart on macOS or
 * Windows and fails with ENOSYS (WTF reads total RAM through it only on Linux).
 * sendfile is emulated there with a pread/read + write loop: callers such as
 * libc++'s filesystem copy_file select sendfile whenever the header exists and
 * do not fall back when it fails, so ENOSYS would make copy_file throw. */
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
  char buffer[65536];
  size_t total = 0;
  if (count > 0x7ffff000u) count = 0x7ffff000u;
  while (total < count) {
    size_t want = count - total < sizeof(buffer) ? count - total : sizeof(buffer);
    ssize_t got = offset ? pread(in_fd, buffer, want, *offset) : read(in_fd, buffer, want);
    if (got < 0) {
      if (errno == EINTR) continue;
      return total ? (ssize_t)total : -1;
    }
    if (got == 0) break;
    for (ssize_t done = 0; done < got;) {
      ssize_t put = write(out_fd, buffer + done, (size_t)(got - done));
      if (put < 0) {
        if (errno == EINTR) continue;
        if (offset) *offset += done;
        return total + (size_t)done ? (ssize_t)(total + (size_t)done) : -1;
      }
      done += put;
    }
    if (offset) *offset += got;
    total += (size_t)got;
  }
  return (ssize_t)total;
}
#endif
