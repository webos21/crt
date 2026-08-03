#ifndef CRT_PRIVATE_CRT_SIGNAL_BACKEND_H
#define CRT_PRIVATE_CRT_SIGNAL_BACKEND_H

#include <signal.h>

/* Real, host OS-level signal disposition to request for a CRT/Bionic-numbered
 * signal (1..NSIG-1). This is orthogonal to the existing pure-software
 * signal_actions[]/raise() dispatch in signal.c, which keeps working
 * unchanged for self-directed signals (raise()/abort()). This backend layer
 * is what lets a *real*, asynchronously OS-delivered signal (SIGCHLD from an
 * actual child exit, a real kill() from another process, Ctrl-C, ...)
 * actually reach a handler registered through sigaction()/signal(), instead
 * of silently going to the host's default disposition. */
enum crt_signal_backend_action {
  /* Restore the host's own default disposition for this signal. */
  CRT_SIGNAL_BACKEND_DEFAULT,
  /* Ask the host to drop the signal without invoking anything. */
  CRT_SIGNAL_BACKEND_IGNORE,
  /* Route real delivery of this signal through __crt_signal_dispatch(). */
  CRT_SIGNAL_BACKEND_DISPATCH
};

/* Implemented once per host under libc/src/arch/{linux,macos,windows}/common/.
 *
 * Returns 0 on success. A host is free to treat a signal that has no real
 * OS-level equivalent (e.g. Linux-only SIGSTKFLT/SIGPWR on macOS) as a
 * successful no-op: the software raise()/signal_actions[] path already
 * covers self-directed delivery for such signals regardless, and there is
 * nothing at the OS level to hook up. Returns -1 with errno set only for a
 * genuine failure to change a signal that *does* have a real OS mapping. */
int __crt_signal_backend_set_action(int bionic_sig, enum crt_signal_backend_action action);

/* Applies (and optionally reads back) `how`/`set` the same way sigprocmask()
 * does, but against the real host signal mask, so that blocking host calls
 * (poll, sigsuspend, ...) actually observe the block/unblock state. Best
 * effort, one-way push: signal.c's own `signal_mask` bookkeeping remains the
 * source of truth read back through sigprocmask()'s `oldset`, this just keeps
 * the host mask in sync. Returns 0 on success, -1 with errno set on failure. */
int __crt_signal_backend_set_mask(int how, const sigset_t* set);

/* Implemented once, in libc/src/signal.c. Every backend's own OS-level signal
 * entry point calls this after translating the host's native signal number
 * back to Bionic/Linux numbering. Looks up signal_actions[bionic_sig] and
 * invokes it the same way raise() does for a self-directed signal. */
void __crt_signal_dispatch(int bionic_sig);

#endif
