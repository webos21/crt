#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t signal_count;
static volatile sig_atomic_t signal_seen;
static volatile sig_atomic_t siginfo_seen;
static volatile sig_atomic_t siginfo_pid_seen;
static int atexit_state;

static int fail(const char* message) {
  fprintf(stderr, "process_signal_test: %s\n", message);
  return 1;
}

static void handle_usr1(int sig) {
  signal_seen = sig;
  ++signal_count;
}

static void handle_siginfo(int sig, siginfo_t* info, void* context) {
  (void)context;

  siginfo_seen = sig;
  if (info != 0 && info->si_signo == sig && info->si_pid == getpid()) {
    siginfo_pid_seen = 1;
  }
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
  struct sigaction action;
  struct sigaction old_action;
  sigset_t set;
  sigset_t oldset;
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
  if (sigemptyset(&set) != 0 ||
      sigaddset(&set, SIGUSR2) != 0 ||
      sigismember(&set, SIGUSR2) != 1 ||
      sigdelset(&set, SIGUSR2) != 0 ||
      sigismember(&set, SIGUSR2) != 0) {
    return fail("sigset basics");
  }
  errno = 0;
  if (sigaddset(&set, 0) == 0 || errno != EINVAL) {
    return fail("sigset invalid");
  }
  if (sigemptyset(&set) != 0 ||
      sigaddset(&set, SIGUSR1) != 0 ||
      signal(SIGUSR1, handle_usr1) == SIG_ERR ||
      sigprocmask(SIG_BLOCK, &set, &oldset) != 0 ||
      raise(SIGUSR1) != 0 ||
      signal_count != 1 ||
      sigprocmask(SIG_UNBLOCK, &set, 0) != 0 ||
      raise(SIGUSR1) != 0 ||
      signal_count != 2 ||
      sigprocmask(SIG_SETMASK, &oldset, 0) != 0) {
    return fail("sigprocmask");
  }
  memset(&action, 0, sizeof(action));
  action.sa_sigaction = handle_siginfo;
  action.sa_flags = SA_SIGINFO;
  if (sigaction(SIGUSR2, &action, &old_action) != 0 ||
      sigaction(SIGUSR2, 0, &old_action) != 0 ||
      (old_action.sa_flags & SA_SIGINFO) == 0 ||
      raise(SIGUSR2) != 0 ||
      siginfo_seen != SIGUSR2 ||
      siginfo_pid_seen != 1) {
    return fail("sigaction siginfo");
  }

  if (atexit(atexit_first) != 0 || atexit(atexit_second) != 0) {
    return fail("atexit");
  }
  return 0;
}
