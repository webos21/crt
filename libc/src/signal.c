#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <private/crt_signal.h>
#include <private/crt_signal_backend.h>

#define CRT_SIGNAL_MAX 32

static struct sigaction signal_actions[CRT_SIGNAL_MAX];
static sigset_t signal_mask;

const char* const sys_signame[NSIG] = {
  "Signal 0",
  "HUP",
  "INT",
  "QUIT",
  "ILL",
  "TRAP",
  "ABRT",
  "BUS",
  "FPE",
  "KILL",
  "USR1",
  "SEGV",
  "USR2",
  "PIPE",
  "ALRM",
  "TERM",
  "STKFLT",
  "CHLD",
  "CONT",
  "STOP",
  "TSTP",
  "TTIN",
  "TTOU",
  "URG",
  "XCPU",
  "XFSZ",
  "VTALRM",
  "PROF",
  "WINCH",
  "IO",
  "PWR",
  "SYS",
};

const char* const sys_siglist[NSIG] = {
  "Signal 0",
  "Hangup",
  "Interrupt",
  "Quit",
  "Illegal instruction",
  "Trace/breakpoint trap",
  "Aborted",
  "Bus error",
  "Floating point exception",
  "Killed",
  "User signal 1",
  "Segmentation fault",
  "User signal 2",
  "Broken pipe",
  "Alarm clock",
  "Terminated",
  "Stack fault",
  "Child exited",
  "Continued",
  "Stopped",
  "Stopped",
  "Stopped",
  "Stopped",
  "Urgent I/O condition",
  "CPU time limit exceeded",
  "File size limit exceeded",
  "Virtual timer expired",
  "Profiling timer expired",
  "Window changed",
  "I/O possible",
  "Power failure",
  "Bad system call",
};

static int signal_valid(int sig) {
  return sig > 0 && sig < CRT_SIGNAL_MAX;
}

static sigset_t signal_bit(int sig) {
  return (sigset_t)1UL << (unsigned int)(sig - 1);
}

int sigemptyset(sigset_t* set) {
  if (set == 0) {
    errno = EINVAL;
    return -1;
  }
  *set = 0;
  return 0;
}

int sigfillset(sigset_t* set) {
  if (set == 0) {
    errno = EINVAL;
    return -1;
  }
  *set = 0;
  {
    int sig;

    for (sig = 1; sig < CRT_SIGNAL_MAX; ++sig) {
      *set |= signal_bit(sig);
    }
  }
  return 0;
}

int sigaddset(sigset_t* set, int sig) {
  if (set == 0 || !signal_valid(sig)) {
    errno = EINVAL;
    return -1;
  }
  *set |= signal_bit(sig);
  return 0;
}

int sigdelset(sigset_t* set, int sig) {
  if (set == 0 || !signal_valid(sig)) {
    errno = EINVAL;
    return -1;
  }
  *set &= ~signal_bit(sig);
  return 0;
}

int sigismember(const sigset_t* set, int sig) {
  if (set == 0 || !signal_valid(sig)) {
    errno = EINVAL;
    return -1;
  }
  return (*set & signal_bit(sig)) != 0;
}

int sigaction(int sig, const struct sigaction* act, struct sigaction* oldact) {
  if (!signal_valid(sig)) {
    errno = EINVAL;
    return -1;
  }
  if (oldact != 0) {
    *oldact = signal_actions[sig];
  }
  if (act != 0) {
    enum crt_signal_backend_action backend_action;

    signal_actions[sig] = *act;
    if (act->sa_handler == SIG_IGN) {
      backend_action = CRT_SIGNAL_BACKEND_IGNORE;
    } else if (act->sa_handler == SIG_DFL) {
      backend_action = CRT_SIGNAL_BACKEND_DEFAULT;
    } else {
      backend_action = CRT_SIGNAL_BACKEND_DISPATCH;
    }
    if (__crt_signal_backend_set_action(sig, backend_action) != 0) {
      return -1;
    }
  }
  return 0;
}

int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
  if (oldset != 0) {
    *oldset = signal_mask;
  }
  if (set == 0) {
    return 0;
  }
  if (how == SIG_BLOCK) {
    signal_mask |= *set;
  } else if (how == SIG_UNBLOCK) {
    signal_mask &= ~*set;
  } else if (how == SIG_SETMASK) {
    signal_mask = *set;
  } else {
    errno = EINVAL;
    return -1;
  }
  if (__crt_signal_backend_set_mask(how, set) != 0) {
    return -1;
  }
  return 0;
}

int pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset) {
  return sigprocmask(how, set, oldset);
}

int sigsuspend(const sigset_t* mask) {
  sigset_t old_mask = signal_mask;

  if (mask == 0) {
    errno = EINVAL;
    return -1;
  }
  signal_mask = *mask;
  signal_mask = old_mask;
  errno = EINTR;
  return -1;
}

void __crt_signal_get_mask(sigset64_t* mask) {
  if (mask != 0) {
    *mask = (sigset64_t)signal_mask;
  }
}

void __crt_signal_set_mask(sigset64_t mask) {
  sigset_t new_mask = (sigset_t)mask;

  signal_mask = new_mask;
  /* The real POSIX signal mask survives exec(), unlike per-signal
   * dispositions set to a handler function, so POSIX_SPAWN_SETSIGMASK must
   * reach the host's real mask here, not just this bookkeeping copy. */
  __crt_signal_backend_set_mask(SIG_SETMASK, &new_mask);
}

void __crt_signal_reset_defaults(sigset64_t mask) {
  int sig;

  for (sig = 1; sig < CRT_SIGNAL_MAX; ++sig) {
    if ((mask & ((sigset64_t)1ULL << (unsigned int)(sig - 1))) != 0) {
      signal_actions[sig].sa_handler = SIG_DFL;
      signal_actions[sig].sa_mask = 0;
      signal_actions[sig].sa_flags = 0;
      /* SIG_IGN (unlike a handler function) survives exec() at the real OS
       * level too, so POSIX_SPAWN_SETSIGDEF must reset the host's real
       * disposition here, not just this bookkeeping copy. */
      __crt_signal_backend_set_action(sig, CRT_SIGNAL_BACKEND_DEFAULT);
    }
  }
}

sighandler_t signal(int sig, sighandler_t handler) {
  struct sigaction action;
  struct sigaction previous;

  if (!signal_valid(sig) || handler == SIG_ERR) {
    errno = EINVAL;
    return SIG_ERR;
  }
  action.sa_handler = handler;
  action.sa_mask = 0;
  action.sa_flags = 0;
  if (sigaction(sig, &action, &previous) != 0) {
    return SIG_ERR;
  }
  return previous.sa_handler;
}

sighandler_t bsd_signal(int sig, sighandler_t handler) {
  return signal(sig, handler);
}

/* See private/crt_signal.h. */
static volatile unsigned long signal_delivery_generation;

unsigned long __crt_signal_delivery_generation(void) {
  return signal_delivery_generation;
}

/* Shared handler-invocation logic for a signal that is known to be
 * deliverable right now (raise() has already checked signal_mask; a real
 * OS-delivered signal reaching __crt_signal_dispatch() was already allowed
 * through by the host's own mask, kept in sync by
 * __crt_signal_backend_set_mask()). */
static void deliver_signal(int sig) {
  struct sigaction* action = &signal_actions[sig];
  sighandler_t handler = action->sa_handler;

  if (handler == SIG_IGN) {
    return;
  }
  if ((action->sa_flags & SA_SIGINFO) != 0 && action->sa_sigaction != 0) {
    siginfo_t info;

    info.si_signo = sig;
    info.si_errno = 0;
    info.si_code = 0;
    info.si_pid = getpid();
    info.si_uid = geteuid();
    info.si_status = 0;
    ++signal_delivery_generation;
    action->sa_sigaction(sig, &info, 0);
    return;
  }
  if (handler != SIG_DFL && handler != 0) {
    ++signal_delivery_generation;
    handler(sig);
    return;
  }
  if (sig == SIGABRT || sig == SIGTERM || sig == SIGINT) {
    abort();
  }
}

int raise(int sig) {
  if (!signal_valid(sig)) {
    errno = EINVAL;
    return -1;
  }
  if ((signal_mask & signal_bit(sig)) != 0) {
    return 0;
  }
  deliver_signal(sig);
  return 0;
}

/* Called by each backend's own OS-level signal entry point after
 * translating the host's native signal number back to Bionic/Linux
 * numbering. See libc/include/private/crt_signal_backend.h. */
void __crt_signal_dispatch(int sig) {
  if (!signal_valid(sig)) {
    return;
  }
  deliver_signal(sig);
}

void abort(void) {
  _exit(128 + SIGABRT);
}
