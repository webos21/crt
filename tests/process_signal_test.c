#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t signal_count;
static volatile sig_atomic_t signal_seen;
static int atexit_state;

static int fail(const char* message) {
  fprintf(stderr, "process_signal_test: %s\n", message);
  return 1;
}

static void handle_usr1(int sig) {
  signal_seen = sig;
  ++signal_count;
}

static void atexit_first(void) {
  if (atexit_state == 2) {
    puts("process_signal_test: ok");
  }
}

static void atexit_second(void) {
  atexit_state = 2;
}

int main(void) {
  sighandler_t previous;
  pid_t pid = getpid();

  if (pid <= 0) {
    return fail("getpid");
  }
  if (getppid() < 0) {
    return fail("getppid");
  }
  if (kill(pid, 0) != 0) {
    return fail("kill self probe");
  }
  errno = 0;
  if (kill(pid, -1) == 0 || errno != EINVAL) {
    return fail("kill invalid signal");
  }

  previous = signal(SIGUSR1, handle_usr1);
  if (previous == SIG_ERR) {
    return fail("signal install");
  }
  if (raise(SIGUSR1) != 0 || signal_seen != SIGUSR1 || signal_count != 1) {
    return fail("raise handler");
  }
  if (signal(SIGUSR1, SIG_IGN) != handle_usr1 ||
      raise(SIGUSR1) != 0 ||
      signal_count != 1) {
    return fail("signal ignore");
  }
  errno = 0;
  if (signal(0, handle_usr1) != SIG_ERR || errno != EINVAL) {
    return fail("signal invalid");
  }

  if (atexit(atexit_first) != 0 || atexit(atexit_second) != 0) {
    return fail("atexit");
  }
  return 0;
}
