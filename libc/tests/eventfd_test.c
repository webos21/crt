/* eventfd() -- Linux-only in real Bionic too, see docs/bionic_libc_gaps.md
 * and HISTORY.md's 2026-08-17 entry. The real behavior (below, under
 * CRT_TARGET_OS_LINUX) exercises the new raw eventfd2 syscall trampoline
 * (libc/src/arch/linux/{x86_64,aarch64}/syscall.S) for real -- reasoned
 * carefully but NOT independently verified on real Linux hardware from
 * the Windows-only session that wrote it; this test is what verifies it
 * the next time it runs on real Linux CI or hardware, matching the same
 * pattern sendmsg()/recvmsg()'s own trampolines already went through.
 *
 * Windows also runs the real-behavior branch as of 2026-09-01: a real,
 * from-scratch emulation (Win32 Event HANDLE + 64-bit counter, see
 * libc/src/arch/windows/common/syscall.c's own CRT_FD_KIND_EVENTFD
 * comment), not a stub -- added after a portable consumer's own
 * configure-time feature probe (curl's) misdetected the original
 * ENOSYS-stub version as usable and broke; see HISTORY.md's dated entry
 * for that regression. macOS also runs the real-behavior branch as of
 * 2026-09-02: a real, from-scratch emulation too (a real pipe(2) pair as
 * the underlying kernel object -- see libc/src/fd.c's own "Real
 * eventfd() emulation for macOS" comment), added for the same
 * curl-threaded-resolver reason. */
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "eventfd_test: %s\n", message);
  return 1;
}

#if defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_WINDOWS) || defined(CRT_TARGET_OS_MACOS)

struct blocking_read_ctx {
  int fd;
  eventfd_t value;
  int result;
};

static void* blocking_read_thread(void* arg) {
  struct blocking_read_ctx* ctx = (struct blocking_read_ctx*)arg;
  ctx->result = eventfd_read(ctx->fd, &ctx->value);
  return 0;
}

#endif

int main(void) {
#if defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_WINDOWS) || defined(CRT_TARGET_OS_MACOS)
  int fd;
  eventfd_t value = 0;
  struct pollfd pfd;

  /* Basic accumulate/drain semantics, non-blocking mode. */
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

  /* Draining an empty, non-blocking eventfd fails EAGAIN, not a short
   * read or a block. */
  if (eventfd_read(fd, &value) == 0 || errno != EAGAIN) {
    close(fd);
    return fail("eventfd_read on an empty non-blocking eventfd should be EAGAIN");
  }
  close(fd);

  /* EFD_SEMAPHORE mode: each read consumes exactly 1, not the whole
   * counter at once. */
  fd = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
  if (fd < 0) {
    return fail("eventfd (semaphore mode)");
  }
  if (eventfd_write(fd, 3) != 0) {
    close(fd);
    return fail("eventfd_write (semaphore mode)");
  }
  if (eventfd_read(fd, &value) != 0 || value != 1) {
    close(fd);
    return fail("first semaphore-mode read should return 1");
  }
  if (eventfd_read(fd, &value) != 0 || value != 1) {
    close(fd);
    return fail("second semaphore-mode read should return 1");
  }
  if (eventfd_read(fd, &value) != 0 || value != 1) {
    close(fd);
    return fail("third semaphore-mode read should return 1");
  }
  if (eventfd_read(fd, &value) == 0 || errno != EAGAIN) {
    close(fd);
    return fail("fourth semaphore-mode read should be EAGAIN (counter exhausted)");
  }
  close(fd);

  /* A genuinely blocking read() (no EFD_NONBLOCK) on another thread
   * really blocks until a write() from this thread wakes it -- exercises
   * the real wait/wake path (a Win32 Event HANDLE on Windows), not just
   * the non-blocking EAGAIN path already covered above. */
  {
    pthread_t thread;
    struct blocking_read_ctx ctx;
    struct timespec delay;

    fd = eventfd(0, 0);
    if (fd < 0) {
      return fail("eventfd (blocking)");
    }
    ctx.fd = fd;
    ctx.value = 0;
    ctx.result = -1;
    if (pthread_create(&thread, 0, blocking_read_thread, &ctx) != 0) {
      close(fd);
      return fail("pthread_create");
    }
    /* Give the reader thread a real chance to actually block inside
     * read() before this thread writes -- not required for correctness
     * (a write() that lands first just means the reader's own read()
     * never blocks at all, which is still a pass), but makes this test
     * actually exercise the blocking path most of the time rather than
     * the non-blocking-would-have-worked-anyway path. */
    delay.tv_sec = 0;
    delay.tv_nsec = 100000000L; /* 100ms */
    nanosleep(&delay, 0);
    if (eventfd_write(fd, 42) != 0) {
      pthread_join(thread, 0);
      close(fd);
      return fail("eventfd_write to wake the blocked reader");
    }
    pthread_join(thread, 0);
    close(fd);
    if (ctx.result != 0 || ctx.value != 42) {
      return fail("blocked read() did not see the value written after it woke");
    }
  }
#else
  if (eventfd(0, 0) >= 0 || errno != ENOSYS) {
    return fail("eventfd should be ENOSYS on this host");
  }
#endif
  printf("eventfd_test: ok\n");
  return 0;
}
