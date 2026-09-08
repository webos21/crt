/* epoll_create1()/epoll_ctl()/epoll_wait() -- Linux-only in real Bionic
 * too, see docs/bionic_libc_gaps.md and HISTORY.md's 2026-08-17 entry.
 *
 * The struct epoll_event size check below runs on every host/architecture
 * this project builds for (not just Linux) -- it's purely about this
 * project's own header matching the real Linux kernel ABI's architecture-
 * conditional layout (12 bytes packed on x86_64, 16 bytes naturally
 * aligned on aarch64; see sys/epoll.h's own comment), so it's exactly as
 * meaningful to check on a Windows/macOS build of this same header as on
 * Linux. The real epoll_create1/epoll_ctl/epoll_wait behavior (under
 * CRT_TARGET_OS_LINUX below) has the same unverified-pending-real-Linux
 * caveat as eventfd_test.c's/timerfd_test.c's own comments. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "epoll_test: %s\n", message);
  return 1;
}

int main(void) {
#if defined(__x86_64__) || defined(_M_X64)
  if (sizeof(struct epoll_event) != 12) {
    return fail("struct epoll_event must be 12 bytes (packed) on x86_64");
  }
#else
  if (sizeof(struct epoll_event) != 16) {
    return fail("struct epoll_event must be 16 bytes on aarch64");
  }
#endif

#if defined(CRT_TARGET_OS_LINUX)
  {
    int epfd;
    int pfds[2];
    struct epoll_event event;
    struct epoll_event out_event;
    int n;

    epfd = epoll_create1(0);
    if (epfd < 0) {
      return fail("epoll_create1");
    }
    if (pipe(pfds) != 0) {
      close(epfd);
      return fail("pipe");
    }

    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = pfds[0];
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, pfds[0], &event) != 0) {
      close(pfds[0]);
      close(pfds[1]);
      close(epfd);
      return fail("epoll_ctl ADD");
    }

    memset(&out_event, 0, sizeof(out_event));
    n = epoll_wait(epfd, &out_event, 1, 0);
    if (n != 0) {
      close(pfds[0]);
      close(pfds[1]);
      close(epfd);
      return fail("should report no events before any write");
    }

    if (write(pfds[1], "x", 1) != 1) {
      close(pfds[0]);
      close(pfds[1]);
      close(epfd);
      return fail("write to pipe");
    }

    n = epoll_wait(epfd, &out_event, 1, 1000);
    if (n != 1 || (out_event.events & EPOLLIN) == 0 || out_event.data.fd != pfds[0]) {
      close(pfds[0]);
      close(pfds[1]);
      close(epfd);
      return fail("did not report the pipe becoming readable");
    }

    if (epoll_ctl(epfd, EPOLL_CTL_DEL, pfds[0], 0) != 0) {
      close(pfds[0]);
      close(pfds[1]);
      close(epfd);
      return fail("epoll_ctl DEL");
    }

    close(pfds[0]);
    close(pfds[1]);
    close(epfd);
  }
#else
  if (epoll_create1(0) >= 0 || errno != ENOSYS) {
    return fail("epoll_create1 should be ENOSYS on this host");
  }
#endif

  printf("epoll_test: ok\n");
  return 0;
}
