#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>

#include <private/crt_macho_symbol.h>
#include <private/crt_signal_backend.h>

/* Real, asynchronous OS signal delivery for macOS.
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
 * This backend closes that gap by calling the *real* libSystem
 * sigaction()/sigprocmask() and registering a plain C function as the host
 * handler. Resolving those real symbols despite this libc defining public
 * symbols with the exact same names uses the shared Mach-O export-trie
 * engine in libc/src/arch/macos/common/macho_symbol.c directly (not
 * dlopen()/dlsym(): libdl depends on libc, so libc calling into libdl here
 * would be a circular target dependency -- see
 * libc/include/private/crt_macho_symbol.h). Earlier investigation considered
 * doing this via the raw sigaction(2) syscall directly (matching the
 * low-level kernel ABI, which additionally requires a "sa_tramp" trampoline
 * pointer), but going through the public, documented libSystem entry points
 * avoids needing to get that kernel-level ABI exactly right: Apple's own
 * internal trampoline machinery handles the raw syscall boundary for us, and
 * our handler is just called with a normal C calling convention, the same
 * way any ordinary macOS C program's signal handler would be.
 */

/* Darwin's PUBLIC struct sigaction/siginfo_t (not the raw kernel ABI struct,
 * which additionally carries a kernel trampoline function pointer that only
 * matters when calling the raw sigaction(2) syscall directly -- see above).
 * Only the fields this file actually reads/writes are declared. */
struct crt_darwin_siginfo {
  int si_signo;
  int si_errno;
  int si_code;
  int32_t si_pid;
  uint32_t si_uid;
  int si_status;
  /* remaining fields (si_addr, si_value, si_band, reserved padding) are not
   * used here and are intentionally omitted from this declaration. */
};

/* Field names deliberately avoid sa_handler/sa_sigaction: <signal.h> #defines
 * those (as __sigaction_handler.sa_handler / .sa_sigaction) for this
 * project's own public struct sigaction, and those macros would otherwise
 * rewrite this unrelated Darwin-shaped struct's member names too. */
struct crt_darwin_sigaction {
  union {
    void (*handler_plain)(int);
    void (*handler_siginfo)(int, struct crt_darwin_siginfo*, void*);
  } handler;
  uint32_t sa_mask;
  int sa_flags;
};

#define CRT_DARWIN_SA_SIGINFO 0x0040
#define CRT_DARWIN_SIG_DFL ((void (*)(int))0)
#define CRT_DARWIN_SIG_IGN ((void (*)(int))1)

#define CRT_DARWIN_SIG_BLOCK 1
#define CRT_DARWIN_SIG_UNBLOCK 2
#define CRT_DARWIN_SIG_SETMASK 3

typedef int (*crt_darwin_sigaction_fn)(int, const struct crt_darwin_sigaction*, struct crt_darwin_sigaction*);
typedef int (*crt_darwin_sigprocmask_fn)(int, const uint32_t*, uint32_t*);

static crt_darwin_sigaction_fn real_sigaction;
static crt_darwin_sigprocmask_fn real_sigprocmask;
static int real_symbols_resolved;
static int real_symbols_available;

/* Bionic/Linux signal number -> Darwin signal number, indexed by Bionic
 * number (1..31); 0 means "no Darwin equivalent" (SIGSTKFLT=16, SIGPWR=30
 * are Linux-only). Compare libc/include/signal.h against Darwin's
 * bsd/sys/signal.h -- most low numbers coincide, but several do not
 * (SIGBUS, SIGUSR1/2, SIGCHLD, SIGSTOP/TSTP/CONT, SIGURG, SIGIO, SIGSYS). */
static const int kBionicToDarwin[32] = {
  0,                                   /* 0 (unused) */
  1, 2, 3, 4, 5, 6, 10, 8, 9, 30, 11,  /* 1..11 */
  31, 13, 14, 15, 0, 20, 19, 17, 18,   /* 12..20 */
  21, 22, 16, 24, 25, 26, 27, 28, 23,  /* 21..29 */
  0, 12,                               /* 30..31 */
};

/* Inverse of the above, indexed by Darwin number (1..31); 0 means "no
 * Bionic/Linux equivalent" (SIGEMT=7, SIGINFO=29 are Darwin/BSD-only). */
static const int kDarwinToBionic[32] = {
  0,                                   /* 0 (unused) */
  1, 2, 3, 4, 5, 6, 0, 8, 9, 7,        /* 1..10 */
  11, 31, 13, 14, 15, 23, 19, 20, 18,  /* 11..19 */
  17, 21, 22, 29, 24, 25, 26, 27, 28,  /* 20..28 */
  0, 10, 12,                          /* 29..31 */
};

static void resolve_real_symbols(void) {
  const void* image;

  if (real_symbols_resolved) {
    return;
  }
  real_symbols_resolved = 1;
  image = __crt_macho_find_loaded_image("/usr/lib/libSystem.B.dylib");
  if (image == 0) {
    return;
  }
  real_sigaction = (crt_darwin_sigaction_fn)__crt_macho_find_symbol_in_image(image, "sigaction");
  real_sigprocmask = (crt_darwin_sigprocmask_fn)__crt_macho_find_symbol_in_image(image, "sigprocmask");
  real_symbols_available = real_sigaction != 0 && real_sigprocmask != 0;
}

/* Installed as the real host handler for every Bionic signal that maps to a
 * Darwin signal and is not SIG_DFL/SIG_IGN. Called by the kernel (via
 * Apple's own trampoline machinery) with a normal C calling convention. */
static void crt_macos_signal_entry(int darwin_sig, struct crt_darwin_siginfo* info, void* uctx) {
  int bionic_sig;

  (void)info;
  (void)uctx;
  if (darwin_sig < 1 || darwin_sig > 31) {
    return;
  }
  bionic_sig = kDarwinToBionic[darwin_sig];
  if (bionic_sig == 0) {
    return;
  }
  __crt_signal_dispatch(bionic_sig);
}

int __crt_signal_backend_set_action(int bionic_sig, enum crt_signal_backend_action action) {
  int darwin_sig;
  struct crt_darwin_sigaction sa;

  if (bionic_sig < 1 || bionic_sig > 31) {
    errno = EINVAL;
    return -1;
  }
  darwin_sig = kBionicToDarwin[bionic_sig];
  if (darwin_sig == 0) {
    /* No real host signal to hook up; the software raise()/signal_actions[]
     * path already covers self-directed delivery for it regardless. */
    return 0;
  }
  resolve_real_symbols();
  if (!real_symbols_available) {
    return 0;
  }

  sa.sa_mask = 0;
  sa.sa_flags = 0;
  switch (action) {
    case CRT_SIGNAL_BACKEND_DEFAULT:
      sa.handler.handler_plain = CRT_DARWIN_SIG_DFL;
      break;
    case CRT_SIGNAL_BACKEND_IGNORE:
      sa.handler.handler_plain = CRT_DARWIN_SIG_IGN;
      break;
    case CRT_SIGNAL_BACKEND_DISPATCH:
    default:
      sa.handler.handler_siginfo = crt_macos_signal_entry;
      sa.sa_flags = CRT_DARWIN_SA_SIGINFO;
      break;
  }
  if (real_sigaction(darwin_sig, &sa, 0) != 0) {
    errno = EINVAL;
    return -1;
  }
  return 0;
}

int __crt_signal_backend_set_mask(int how, const sigset_t* set) {
  uint32_t darwin_mask = 0;
  int darwin_how;
  int sig;

  if (set == 0) {
    return 0;
  }
  resolve_real_symbols();
  if (!real_symbols_available) {
    return 0;
  }
  for (sig = 1; sig <= 31; ++sig) {
    if ((*set & ((sigset_t)1UL << (unsigned int)(sig - 1))) != 0) {
      int darwin_sig = kBionicToDarwin[sig];

      if (darwin_sig != 0) {
        darwin_mask |= (uint32_t)1U << (unsigned int)(darwin_sig - 1);
      }
    }
  }
  if (how == SIG_BLOCK) {
    darwin_how = CRT_DARWIN_SIG_BLOCK;
  } else if (how == SIG_UNBLOCK) {
    darwin_how = CRT_DARWIN_SIG_UNBLOCK;
  } else if (how == SIG_SETMASK) {
    darwin_how = CRT_DARWIN_SIG_SETMASK;
  } else {
    errno = EINVAL;
    return -1;
  }
  if (real_sigprocmask(darwin_how, &darwin_mask, 0) != 0) {
    errno = EINVAL;
    return -1;
  }
  return 0;
}
