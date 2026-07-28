#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#define CRT_SIGNAL_MAX 32

static sighandler_t signal_handlers[CRT_SIGNAL_MAX];

static int signal_valid(int sig) {
  return sig > 0 && sig < CRT_SIGNAL_MAX;
}

sighandler_t signal(int sig, sighandler_t handler) {
  sighandler_t previous;

  if (!signal_valid(sig) || handler == SIG_ERR) {
    errno = EINVAL;
    return SIG_ERR;
  }
  previous = signal_handlers[sig];
  signal_handlers[sig] = handler;
  return previous;
}

int raise(int sig) {
  sighandler_t handler;

  if (!signal_valid(sig)) {
    errno = EINVAL;
    return -1;
  }
  handler = signal_handlers[sig];
  if (handler == SIG_IGN) {
    return 0;
  }
  if (handler != SIG_DFL) {
    handler(sig);
    return 0;
  }
  if (sig == SIGABRT || sig == SIGTERM || sig == SIGINT) {
    abort();
  }
  return 0;
}

void abort(void) {
  _exit(128 + SIGABRT);
}
