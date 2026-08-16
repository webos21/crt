/* Regression for a real bug found while investigating toybox's `timeout`
 * applet hanging instead of enforcing its deadline (2026-08-16, see
 * HISTORY.md): polling a pipe's *write* end for POLLIN, with nothing ever
 * written to it, reported POLLIN ready almost instantly on Windows instead
 * of correctly blocking until data arrives or the timeout elapses.
 * __crt_sys_poll()'s poll_handle() called PeekNamedPipe() unconditionally
 * on any FILE_TYPE_PIPE handle -- that function's documented behavior only
 * covers the read end; on the write end it does not reliably report "no
 * data available" the way POLLIN semantics require. Fixed by tracking
 * which fd is a pipe write end (fd_pipe_write_only[] in
 * libc/src/arch/windows/common/syscall.c) and never calling
 * PeekNamedPipe() on one. */
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "poll_pipe_write_end_test: %s\n", message);
  return 1;
}

int main(void) {
  int fds[2];
  struct pollfd pfd;
  struct timespec start;
  struct timespec end;
  int rv;
  double elapsed_ms;

  if (pipe(fds) != 0) {
    return fail("pipe() failed");
  }

  pfd.fd = fds[1]; /* the write end -- nothing ever writes to it below */
  pfd.events = POLLIN;
  pfd.revents = 0;

  clock_gettime(CLOCK_MONOTONIC, &start);
  rv = poll(&pfd, 1, 300);
  clock_gettime(CLOCK_MONOTONIC, &end);

  close(fds[0]);
  close(fds[1]);

  if (rv != 0) {
    return fail("poll() on an unwritten pipe write end did not time out "
                "(POLLIN incorrectly reported ready)");
  }

  elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
               (end.tv_nsec - start.tv_nsec) / 1e6;
  /* Must have actually waited close to the real 300ms timeout, not
   * returned early for an unrelated reason. */
  if (elapsed_ms < 250.0) {
    return fail("poll() returned far too early to have honored the timeout");
  }

  printf("poll_pipe_write_end_test: ok\n");
  return 0;
}
