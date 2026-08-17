/* timerfd_create()/timerfd_settime()/timerfd_gettime() -- Linux-only in
 * real Bionic too, see docs/bionic_libc_gaps.md and HISTORY.md's
 * 2026-08-17 entry. Same unverified-pending-real-Linux caveat as
 * eventfd_test.c's own comment. */
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/timerfd.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "timerfd_test: %s\n", message);
  return 1;
}

int main(void) {
#if defined(CRT_TARGET_OS_LINUX)
  int fd;
  struct itimerspec spec;
  struct pollfd pfd;
  uint64_t expirations = 0;

  fd = timerfd_create(CLOCK_MONOTONIC, 0);
  if (fd < 0) {
    return fail("timerfd_create");
  }

  memset(&spec, 0, sizeof(spec));
  spec.it_value.tv_sec = 0;
  spec.it_value.tv_nsec = 50000000L; /* 50ms, one-shot (it_interval left 0) */
  if (timerfd_settime(fd, 0, &spec, 0) != 0) {
    close(fd);
    return fail("timerfd_settime");
  }

  pfd.fd = fd;
  pfd.events = POLLIN;
  pfd.revents = 0;
  if (poll(&pfd, 1, 0) != 0) {
    close(fd);
    return fail("should not have fired immediately");
  }

  if (poll(&pfd, 1, 1000) < 1 || (pfd.revents & POLLIN) == 0) {
    close(fd);
    return fail("did not fire within 1s of a 50ms one-shot timer");
  }
  if (read(fd, &expirations, sizeof(expirations)) != (ssize_t)sizeof(expirations) ||
      expirations < 1) {
    close(fd);
    return fail("read did not report an expiration count");
  }
  close(fd);
#else
  struct itimerspec spec;

  memset(&spec, 0, sizeof(spec));
  if (timerfd_create(CLOCK_MONOTONIC, 0) >= 0 || errno != ENOSYS) {
    return fail("timerfd_create should be ENOSYS on this host");
  }
  if (timerfd_settime(0, 0, &spec, 0) >= 0 || errno != ENOSYS) {
    return fail("timerfd_settime should be ENOSYS on this host");
  }
  if (timerfd_gettime(0, &spec) >= 0 || errno != ENOSYS) {
    return fail("timerfd_gettime should be ENOSYS on this host");
  }
#endif
  printf("timerfd_test: ok\n");
  return 0;
}
