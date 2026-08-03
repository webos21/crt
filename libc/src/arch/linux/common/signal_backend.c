#include <errno.h>
#include <signal.h>
#include <stddef.h>

#include <private/crt_signal_backend.h>

/* Real, asynchronous OS signal delivery for Linux.
 *
 * libc/src/signal.c's sigaction()/signal_actions[] dispatch is pure software
 * bookkeeping: raise()/abort() invoke a registered handler directly and
 * synchronously, but nothing tells the real kernel to route an actual signal
 * (a child exiting and generating SIGCHLD, a real kill() from another
 * process, Ctrl-C, ...) through it. Without this, any code that relies on a
 * blocking host call being interrupted by a real signal -- GNU make's
 * jobserver_acquire(), for one concrete, reproduced example -- hangs
 * forever even after the event it is waiting for has already happened,
 * because nothing ever runs the handler or unblocks the wait.
 *
 * Unlike macOS (which goes through libSystem's public sigaction()/
 * sigprocmask() via dlsym(), since this project has no CRT-owned ELF dynamic
 * linker yet), CRT Linux executables are linked with -nostdlib
 * -nostartfiles -nodefaultlibs and own their entire syscall surface, so this
 * backend calls the raw rt_sigaction(2)/rt_sigprocmask(2) syscalls directly
 * (libc/src/arch/{x86_64,aarch64}/syscall.S), matching Android Bionic's own
 * libc/bionic/sigaction.cpp.
 *
 * This project's own public signal numbering (libc/include/signal.h) and
 * SA_SIGINFO/SIG_BLOCK/SIG_UNBLOCK/SIG_SETMASK values already match the real
 * Linux kernel ABI exactly (Linux is Bionic's native platform), so -- unlike
 * macOS -- no signal-number or flag translation table is needed here. This
 * project's sigset_t is also a plain 64-bit `unsigned long`, the same size
 * and layout the kernel expects for the sigsetsize the raw syscalls require,
 * so masks are passed straight through as well.
 */

/* Linux kernel struct sigaction (asm-generic/signal.h), NOT glibc's public
 * struct sigaction, which is a different, ABI-incompatible shape. Field
 * order and layout are the same on x86_64 and aarch64.
 *
 * Field names deliberately avoid sa_handler/sa_sigaction: <signal.h>
 * #defines those (as __sigaction_handler.sa_handler / .sa_sigaction) for
 * this project's own public struct sigaction, and those macros would
 * otherwise rewrite this unrelated kernel-ABI struct's member names too. */
struct crt_kernel_sigaction {
  union {
    void (*handler_plain)(int);
    void (*handler_siginfo)(int, siginfo_t*, void*);
  } handler;
  unsigned long sa_flags;
  void (*sa_restorer)(void);
  unsigned long sa_mask;
};

#define CRT_KERNEL_SIG_DFL ((void (*)(int))0)
#define CRT_KERNEL_SIG_IGN ((void (*)(int))1)
#define CRT_KERNEL_SA_RESTORER 0x04000000UL

long __crt_sys_rt_sigaction(int sig, const struct crt_kernel_sigaction* act, struct crt_kernel_sigaction* oldact,
                             unsigned long sigsetsize);
long __crt_sys_rt_sigprocmask(int how, const unsigned long* set, unsigned long* oldset, unsigned long sigsetsize);

#if defined(__x86_64__)
/* Tiny `rt_sigreturn` trampoline; see libc/src/arch/linux/x86_64/syscall.S.
 * Only x86_64's rt_sigaction requires SA_RESTORER + a real restorer address
 * -- aarch64 leaves both unset and lets the kernel supply its own default
 * restorer from the vDSO (matching Android Bionic's arch-arm64 comment). */
void __crt_signal_restore_rt(void);
#endif

static int normalize_syscall_result(long result) {
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return (int)result;
}

/* Installed as the real kernel handler for every dispatched signal. Called
 * by the kernel with a normal C calling convention (via the sa_restorer
 * trampoline on x86_64, or the kernel/vDSO default restorer on aarch64). */
static void crt_linux_signal_entry(int sig, siginfo_t* info, void* uctx) {
  (void)info;
  (void)uctx;
  __crt_signal_dispatch(sig);
}

int __crt_signal_backend_set_action(int bionic_sig, enum crt_signal_backend_action action) {
  struct crt_kernel_sigaction sa;

  sa.sa_mask = 0;
  sa.sa_flags = 0;
  sa.sa_restorer = 0;
  switch (action) {
    case CRT_SIGNAL_BACKEND_DEFAULT:
      sa.handler.handler_plain = CRT_KERNEL_SIG_DFL;
      break;
    case CRT_SIGNAL_BACKEND_IGNORE:
      sa.handler.handler_plain = CRT_KERNEL_SIG_IGN;
      break;
    case CRT_SIGNAL_BACKEND_DISPATCH:
    default:
      sa.handler.handler_siginfo = crt_linux_signal_entry;
      sa.sa_flags = SA_SIGINFO;
#if defined(__x86_64__)
      sa.sa_flags |= CRT_KERNEL_SA_RESTORER;
      sa.sa_restorer = __crt_signal_restore_rt;
#endif
      break;
  }
  return normalize_syscall_result(__crt_sys_rt_sigaction(bionic_sig, &sa, 0, sizeof(unsigned long)));
}

int __crt_signal_backend_set_mask(int how, const sigset_t* set) {
  if (set == 0) {
    return 0;
  }
  return normalize_syscall_result(__crt_sys_rt_sigprocmask(how, set, 0, sizeof(unsigned long)));
}
