#include <errno.h>
#include <signal.h>
#include <stddef.h>

#include <private/crt_signal_backend.h>

/* Windows has no real kernel-level asynchronous POSIX signal delivery
 * mechanism in general -- there is no SetConsoleCtrlHandler-style analog for
 * SIGSEGV/SIGFPE/etc, and no bridge exists here from those Win32 mechanisms
 * into signal_actions[]/raise() (left for later, see below). For SIGCHLD
 * specifically, though, Windows *does* have everything needed to build a
 * real (if synchronous/polled rather than kernel-async) equivalent: a
 * process handle that becomes signaled on exit, already tracked per child in
 * syscall.c's child registry for waitpid(). __crt_windows_check_sigchld_
 * pending() (libc/src/arch/windows/common/syscall.c) does exactly that scan;
 * this file's __crt_signal_backend_set_mask() below calls it at the one
 * point real kernel signal delivery would matter here -- unblocking SIGCHLD
 * synchronously delivers an already-pending instance, matching real signal
 * delivery happening on the way back to userspace from the unblocking
 * sigprocmask() syscall on Linux/macOS (see docs/signal_delivery.md's
 * "pselect() Atomicity" section, which libc/src/poll.c's pselect() depends
 * on this for). __crt_sys_poll()'s own blocking loop (syscall.c) makes the
 * same check on every iteration, covering a child that exits while a
 * pselect()/select()/poll() call is genuinely blocked rather than already
 * exited beforehand.
 *
 * Scope: this covers pselect()/select()/poll() interruption by a real
 * SIGCHLD, the concrete case that motivated this whole backend interface
 * (GNU make's jobserver_acquire(), see docs/signal_delivery.md). It does
 * *not* cover interrupting a plain blocking read()/write()/etc with EINTR --
 * those are single Win32 syscalls on this host with no polling loop to hook
 * a check into, and making them interruptible would need a much larger
 * overlapped-I/O rework, out of scope here.
 *
 * Console control events (Ctrl-C, Ctrl-Break) and structured exception
 * handling (access violations, etc.) remain a separate, real Win32
 * mechanism that could eventually be bridged into signal_actions[]/raise()
 * the same general way (SetConsoleCtrlHandler for SIGINT/SIGBREAK-style
 * delivery, a vectored exception handler for SIGSEGV/SIGFPE/SIGILL), but
 * that is distinct future work, unrelated to SIGCHLD -- sigaction()/
 * sigprocmask() for every signal other than SIGCHLD stay pure software
 * bookkeeping (self-delivery only via raise()/abort()), which is still
 * honest: Windows genuinely has no OS mechanism to hook into for those. */

/* Implemented in libc/src/arch/windows/common/syscall.c: scans the child
 * registry for an unblocked, previously-unobserved exited child. Returns 1
 * (and marks it observed) if found, 0 otherwise (including "still
 * blocked"). */
int __crt_windows_check_sigchld_pending(void);

int __crt_signal_backend_set_action(int bionic_sig, enum crt_signal_backend_action action) {
  (void)bionic_sig;
  (void)action;
  return 0;
}

int __crt_signal_backend_set_mask(int how, const sigset_t* set) {
  (void)how;
  (void)set;
  if (__crt_windows_check_sigchld_pending()) {
    __crt_signal_dispatch(SIGCHLD);
  }
  return 0;
}
