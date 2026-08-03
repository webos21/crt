#include <errno.h>
#include <signal.h>
#include <stddef.h>

#include <private/crt_signal_backend.h>

/* Windows has no real OS-level equivalent of asynchronous POSIX signal
 * delivery for the case this backend interface exists to fix (SIGCHLD from a
 * child process exiting, waking up a blocking wait elsewhere in the same
 * process): there is no SIGCHLD-generating kernel mechanism on Windows at
 * all. Child completion is already observed directly through
 * __crt_shell_spawn()/waitpid() and the child registry (see
 * docs/windows_fork_emulation.md), not through a signal.
 *
 * Console control events (Ctrl-C, Ctrl-Break) and structured exception
 * handling (access violations, etc.) are a separate, real Win32 mechanism
 * that could eventually be bridged into this same signal_actions[]/raise()
 * dispatch (SetConsoleCtrlHandler for SIGINT/SIGBREAK-style delivery, a
 * vectored exception handler for SIGSEGV/SIGFPE/SIGILL), but that is a
 * distinct piece of work from the SIGCHLD-driven deadlock this file exists
 * to address, and is left for later. Until then this backend is a
 * documented no-op: sigaction()/sigprocmask() keep working exactly as
 * before (pure software bookkeeping, self-delivery only via raise()/
 * abort()), which is honest given Windows does not have the underlying OS
 * mechanism this interface is meant to hook into yet. */

int __crt_signal_backend_set_action(int bionic_sig, enum crt_signal_backend_action action) {
  (void)bionic_sig;
  (void)action;
  return 0;
}

int __crt_signal_backend_set_mask(int how, const sigset_t* set) {
  (void)how;
  (void)set;
  return 0;
}
