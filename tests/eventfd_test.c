/* eventfd() -- Linux-only in real Bionic too, see docs/bionic_libc_gaps.md
 * and HISTORY.md's 2026-08-17 entry. The real behavior (below, under
 * CRT_TARGET_OS_LINUX) exercises the new raw eventfd2 syscall trampoline
 * (libc/src/arch/linux/{x86_64,aarch64}/syscall.S) for real -- reasoned
 * carefully but NOT independently verified on real Linux hardware from
 * the Windows-only session that wrote it; this test is what verifies it
 * the next time it runs on real Linux CI or hardware, matching the same
 * pattern sendmsg()/recvmsg()'s own trampolines already went through. */
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <sys/eventfd.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "eventfd_test: %s\n", message);
  return 1;
}

int main(void) {
#if defined(CRT_TARGET_OS_LINUX)
  int fd;
  eventfd_t value = 0;
  struct pollfd pfd;

  fd = eventfd(0, EFD_NONBLOCK);
  if (fd < 0) {
    return fail("eventfd");
  }

  pfd.fd = fd;
  pfd.events = POLLIN;
  pfd.revents = 0;
  if (poll(&pfd, 1, 0) != 0) {
    close(fd);
    return fail("should not be readable before any write");
  }

  if (eventfd_write(fd, 5) != 0 || eventfd_write(fd, 3) != 0) {
    close(fd);
    return fail("eventfd_write");
  }
  /* Real eventfd semantics: writes accumulate (5+3=8); one read consumes
   * the whole counter and resets it to 0. */
  if (eventfd_read(fd, &value) != 0 || value != 8) {
    close(fd);
    return fail("eventfd_read did not see the accumulated value");
  }

  pfd.revents = 0;
  if (poll(&pfd, 1, 0) != 0) {
    close(fd);
    return fail("should not be readable again after being drained");
  }
  close(fd);
#else
  if (eventfd(0, 0) >= 0 || errno != ENOSYS) {
    return fail("eventfd should be ENOSYS on this host");
  }
#endif
  printf("eventfd_test: ok\n");
  return 0;
}
