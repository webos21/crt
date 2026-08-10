/* Permanent regression test for the fork() + blocked-SIGCHLD + pselect()
 * lost-wakeup fix described in docs/signal_delivery.md ("pselect()
 * Atomicity"). Before that fix, a SIGCHLD that was already pending (blocked,
 * child already exited) before the caller ever entered pselect() was
 * silently swallowed by the non-atomic "unblock, then select()" sequence,
 * and the call blocked forever -- the exact bug that hung GNU make's
 * jobserver_acquire() under `make -j 10`. This is the isolated repro from
 * that doc's "Verification" section, turned into a ctest entry per its
 * "Next Steps" list. */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t sigchld_seen;

static int fail(const char* message) {
  fprintf(stderr, "pselect_sigchld_test: %s\n", message);
  return 1;
}

static void handle_sigchld(int sig) {
  (void)sig;
  sigchld_seen = 1;
}

static long elapsed_ms(const struct timespec* start, const struct timespec* end) {
  return (end->tv_sec - start->tv_sec) * 1000L +
         (end->tv_nsec - start->tv_nsec) / 1000000L;
}

int main(void) {
  struct sigaction action;
  sigset_t block_set;
  sigset_t empty_set;
  pid_t pid;
  int status = 0;
  fd_set readfds;
  struct timespec timeout;
  struct timespec settle;
  struct timespec start;
  struct timespec end;
  int result;
  int dummy_pipe[2];

  if (pipe(dummy_pipe) != 0) {
    return fail("pipe");
  }

  /* A real handler (not SIG_IGN/default) is required: signal.c's
   * deliver_signal() only bumps the delivery generation pselect() checks
   * for a *caught* signal, matching real EINTR semantics. */
  memset(&action, 0, sizeof(action));
  action.sa_handler = handle_sigchld;
  if (sigaction(SIGCHLD, &action, 0) != 0) {
    close(dummy_pipe[0]);
    close(dummy_pipe[1]);
    return fail("sigaction");
  }

  if (sigemptyset(&block_set) != 0 || sigaddset(&block_set, SIGCHLD) != 0 ||
      sigprocmask(SIG_BLOCK, &block_set, 0) != 0) {
    close(dummy_pipe[0]);
    close(dummy_pipe[1]);
    return fail("sigprocmask block");
  }

  pid = fork();
  if (pid < 0) {
#if defined(CRT_TARGET_OS_WINDOWS)
    if (errno == ENOTSUP) {
      close(dummy_pipe[0]);
      close(dummy_pipe[1]);
      puts("pselect_sigchld_test: ok");
      return 0;
    }
#endif
    close(dummy_pipe[0]);
    close(dummy_pipe[1]);
    return fail("fork");
  }
  if (pid == 0) {
    close(dummy_pipe[0]);
    close(dummy_pipe[1]);
    _exit(42);
  }
  /* Deliberately keep dummy_pipe[1] open in the parent for the rest of the
   * test: if it were closed here, dummy_pipe[0] would become EOF-readable
   * immediately (no process would hold the write end open any more), and
   * pselect() would report it ready regardless of the signal-atomicity
   * behavior under test -- masking the exact bug this test exists to catch.
   * Keeping it open means the only way pselect() can return before the 5s
   * timeout is the already-pending SIGCHLD. */

  /* Give the child time to actually exit and the kernel to queue the
   * (blocked) SIGCHLD as pending *before* pselect() is ever called -- this
   * is the exact lost-wakeup scenario: the signal is already pending when
   * the atomic unblock-and-wait begins, not merely "about to arrive". */
  settle.tv_sec = 0;
  settle.tv_nsec = 200000000L; /* 200ms */
  nanosleep(&settle, 0);

  if (sigemptyset(&empty_set) != 0) {
    close(dummy_pipe[0]);
    close(dummy_pipe[1]);
    return fail("sigemptyset");
  }

  FD_ZERO(&readfds);
  FD_SET(dummy_pipe[0], &readfds);
  timeout.tv_sec = 5;
  timeout.tv_nsec = 0;

  if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
    close(dummy_pipe[0]);
    close(dummy_pipe[1]);
    return fail("clock_gettime start");
  }
  result = pselect(dummy_pipe[0] + 1, &readfds, 0, 0, &timeout, &empty_set);
  if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
    close(dummy_pipe[0]);
    close(dummy_pipe[1]);
    return fail("clock_gettime end");
  }
  close(dummy_pipe[0]);
  close(dummy_pipe[1]);

#if defined(CRT_TARGET_OS_WINDOWS)
  /* Windows' signal backend is an honest no-op stub (docs/signal_delivery.md,
   * "Windows"): there is no real SIGCHLD-equivalent kernel delivery path, so
   * pselect() legitimately just runs out its bounded timeout here rather
   * than observing an EINTR wakeup. Only the bounded-timeout half of the
   * contract applies on this host. */
  if (result != 0 || elapsed_ms(&start, &end) < 4000) {
    return fail("windows pselect timeout behavior");
  }
#else
  if (result != -1 || errno != EINTR) {
    return fail("pselect did not report the already-pending SIGCHLD");
  }
  /* The actual regression check: before the pselect() atomicity fix, this
   * call silently missed the already-pending signal and blocked for the
   * full 5-second timeout (in the original GNU make jobserver hang,
   * forever). Require it to return in well under that -- catches a real
   * regression instead of merely tolerating a slow pass. */
  if (elapsed_ms(&start, &end) >= 2000) {
    return fail("pselect took too long to report EINTR");
  }
  if (!sigchld_seen) {
    return fail("SIGCHLD handler never ran");
  }
#endif

  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
      WEXITSTATUS(status) != 42) {
    return fail("wait status");
  }

  puts("pselect_sigchld_test: ok");
  return 0;
}
