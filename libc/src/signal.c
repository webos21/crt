#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
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
    if (__crt_signal_backend_set_action_ex(sig, backend_action, (int)act->sa_flags, &act->sa_mask) != 0) {
      return -1;
    }
  }
  return 0;
}

/* The calling thread's signal mask. On Linux it is the kernel's, per thread; elsewhere it is
 * this file's software mask (see the backend notes in private/crt_signal_backend.h). */
static sigset_t current_signal_mask(void) {
  sigset_t current = 0;

  if (__crt_signal_backend_sigprocmask(SIG_BLOCK, 0, &current) == 1) {
    return signal_mask;
  }
  return current;
}

int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
  int result = __crt_signal_backend_sigprocmask(how, set, oldset);

  if (result != 1) {
    return result;
  }
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

/* pause(): wait until a signal has been delivered to a handler, then fail with EINTR
 * (POSIX/Bionic). Waits by polling the signal-delivery generation every 10 ms rather
 * than blocking in the kernel: CRT delivers a signal to its own handlers through this
 * file, on every host, and each delivery bumps the generation just before it calls the
 * handler (so the wait can end while the handler is still running). */
int pause(void) {
  unsigned long generation = __crt_signal_delivery_generation();
  struct timespec interval;

  interval.tv_sec = 0;
  interval.tv_nsec = 10L * 1000L * 1000L;
  while (__crt_signal_delivery_generation() == generation) {
    nanosleep(&interval, 0);
  }
  errno = EINTR;
  return -1;
}

int sigsuspend(const sigset_t* mask) {
  sigset_t old_mask;
  int result;

  if (mask == 0) {
    errno = EINVAL;
    return -1;
  }
  /* Linux: really wait (rt_sigsuspend). WTF's thread suspend/resume parks the target thread here
   * until the resume signal arrives; returning at once would let a thread that another thread
   * believes is stopped keep running. */
  result = __crt_signal_backend_sigsuspend(mask);
  if (result != 1) {
    return result;
  }
  old_mask = signal_mask;
  signal_mask = *mask;
  signal_mask = old_mask;
  errno = EINTR;
  return -1;
}

void __crt_signal_get_mask(sigset64_t* mask) {
  if (mask != 0) {
    *mask = (sigset64_t)current_signal_mask();
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
static void deliver_signal(int sig, const siginfo_t* host_info, void* host_context) {
  struct sigaction* action = &signal_actions[sig];
  sighandler_t handler = action->sa_handler;

  if (handler == SIG_IGN) {
    return;
  }
  if (handler != SIG_DFL && handler != 0 && (action->sa_flags & SA_RESETHAND) != 0) {
    /* One-shot handler: restore the default disposition before the handler runs. */
    struct sigaction one_shot = *action;

    action->sa_handler = SIG_DFL;
    action->sa_flags = 0;
    action->sa_mask = 0;
    __crt_signal_backend_set_action(sig, CRT_SIGNAL_BACKEND_DEFAULT);
    action = &one_shot;
    handler = action->sa_handler;
  }
  if ((action->sa_flags & SA_SIGINFO) != 0 && action->sa_sigaction != 0) {
    siginfo_t info;

    if (host_info != 0) {
      info = *host_info;
    } else {
      memset(&info, 0, sizeof(info));
      info.si_signo = sig;
      info.si_errno = 0;
      info.si_code = 0;
      info.si_pid = getpid();
      info.si_uid = geteuid();
      info.si_status = 0;
    }
    ++signal_delivery_generation;
    action->sa_sigaction(sig, &info, host_context);
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
  if ((current_signal_mask() & signal_bit(sig)) != 0) {
    return 0;
  }
  deliver_signal(sig, 0, 0);
  return 0;
}

/* Called by each backend's own OS-level signal entry point after
 * translating the host's native signal number back to Bionic/Linux
 * numbering. See libc/include/private/crt_signal_backend.h. */
void __crt_signal_dispatch(int sig) {
  if (!signal_valid(sig)) {
    return;
  }
  deliver_signal(sig, 0, 0);
}

void __crt_signal_dispatch_info(int sig, const siginfo_t* info, void* uctx) {
  if (!signal_valid(sig)) {
    return;
  }
  deliver_signal(sig, info, uctx);
}

void abort(void) {
  _exit(128 + SIGABRT);
}
