/* Public descriptor semantics for Windows' HANDLE-less devices, also runnable
 * against native device fds on Linux/macOS. No random-byte value is assumed. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CHECK(expression) do { \
  if (!(expression)) { \
    fprintf(stderr, "virtual_device_dup_test: line %d: %s (errno=%d)\n", \
            __LINE__, #expression, errno); \
    return 1; \
  } \
} while (0)

static int read_device(int fd, int zero) {
  unsigned char bytes[16];
  size_t i;

  memset(bytes, 0xa5, sizeof(bytes));
  CHECK(read(fd, bytes, sizeof(bytes)) == (ssize_t)sizeof(bytes));
  if (zero) {
    for (i = 0; i < sizeof(bytes); ++i) CHECK(bytes[i] == 0);
  }
  return 0;
}

static int check_device(const char* path, int zero) {
  int fd = open(path, O_RDONLY);
  int copy;
  int target;
  int pipes[2];
  int nullfd;
  int oldfd;
  int i;
  char byte;

  CHECK(fd >= 0);
  CHECK(read_device(fd, zero) == 0);
  CHECK(fcntl(fd, F_SETFD, FD_CLOEXEC) == 0);
  CHECK(dup2(fd, fd) == fd);
  CHECK(fcntl(fd, F_GETFD) == FD_CLOEXEC);
  CHECK(read_device(fd, zero) == 0);

  copy = dup(fd);
  CHECK(copy >= 0 && copy != fd);
  CHECK(fcntl(copy, F_GETFD) == 0);
  CHECK(fcntl(fd, F_GETFD) == FD_CLOEXEC);
  CHECK(read_device(copy, zero) == 0);
  CHECK(close(copy) == 0);
  CHECK(read_device(fd, zero) == 0);

  copy = fcntl(fd, F_DUPFD, 32);
  CHECK(copy >= 32);
  CHECK(fcntl(copy, F_GETFD) == 0);
  CHECK(read_device(copy, zero) == 0);
  CHECK(close(copy) == 0);
  copy = fcntl(fd, F_DUPFD_CLOEXEC, 32);
  CHECK(copy >= 32);
  CHECK(fcntl(copy, F_GETFD) == FD_CLOEXEC);
  CHECK(read_device(copy, zero) == 0);
  CHECK(close(copy) == 0);
  errno = 0;
  CHECK(fcntl(fd, F_DUPFD, -1) == -1 && errno == EINVAL);
  errno = 0;
  CHECK(dup2(fd, -1) == -1 && errno == EBADF);

  /* Closing the original must not invalidate the new descriptor. */
  copy = dup(fd);
  CHECK(copy >= 0);
  oldfd = fd;
  CHECK(close(fd) == 0);
  CHECK(read_device(copy, zero) == 0);
  errno = 0;
  CHECK(dup2(oldfd, oldfd) == -1 && errno == EBADF);
  errno = 0;
  CHECK(dup(oldfd) == -1 && errno == EBADF);
  errno = 0;
  CHECK(dup2(oldfd, copy) == -1 && errno == EBADF);
  CHECK(read_device(copy, zero) == 0);
  CHECK(close(copy) == 0);

  /* Reuse a slot repeatedly; replacing another synthetic slot must never
   * CloseHandle its placeholder, and must clear only the new fd's CLOEXEC. */
  for (i = 0; i < 16; ++i) {
    fd = open(path, O_RDONLY);
    target = open("/dev/zero", O_RDONLY);
    CHECK(fd >= 0 && target >= 0);
    CHECK(fcntl(fd, F_SETFD, FD_CLOEXEC) == 0);
    CHECK(fcntl(target, F_SETFD, FD_CLOEXEC) == 0);
    CHECK(dup2(fd, target) == target);
    CHECK(fcntl(target, F_GETFD) == 0);
    CHECK(fcntl(fd, F_GETFD) == FD_CLOEXEC);
    CHECK(read_device(target, zero) == 0);
    CHECK(close(fd) == 0);
    CHECK(read_device(target, zero) == 0);
    CHECK(close(target) == 0);
  }

  fd = open(path, O_RDONLY);
  CHECK(fd >= 0);
  CHECK(pipe(pipes) == 0);
  /* Replacing the last real write handle must give the reader EOF. A leaked
   * handle hangs here and is bounded by the CTest timeout. */
  CHECK(dup2(fd, pipes[1]) == pipes[1]);
  CHECK(read(pipes[0], &byte, 1) == 0);
  CHECK(close(pipes[0]) == 0);
  CHECK(read_device(pipes[1], zero) == 0);
  CHECK(close(pipes[1]) == 0);
  CHECK(read_device(fd, zero) == 0);

  /* Exercise the reverse replacement too: real HANDLE over virtual slot. */
  nullfd = open("/dev/null", O_RDWR);
  CHECK(nullfd >= 0);
  CHECK(dup2(nullfd, fd) == fd);
  CHECK(read(fd, &byte, 1) == 0);
  CHECK(write(fd, "x", 1) == 1);
  CHECK(close(fd) == 0);
  CHECK(write(nullfd, "x", 1) == 1);
  CHECK(close(nullfd) == 0);
  return 0;
}

int main(void) {
  CHECK(check_device("/dev/urandom", 0) == 0);
  CHECK(check_device("/dev/zero", 1) == 0);
#if defined(CRT_TARGET_OS_WINDOWS)
  /* The Windows random-device aliases share the same synthetic fd kind. */
  CHECK(check_device("/dev/random", 0) == 0);
#endif
  puts("virtual_device_dup_test: ok");
  return 0;
}
