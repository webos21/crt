#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <private/crt_linux_ucontext_aarch64.h>
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
  /* Named host_* because <signal.h> #defines si_pid/si_uid/si_status as
   * accessors for this project's own siginfo_t (Bionic spelling), which would
   * otherwise rewrite these member names too. */
  int32_t host_si_pid;
  uint32_t host_si_uid;
  int host_si_status;
  void* host_si_addr;
  /* remaining fields (si_value, si_band, reserved padding) are not
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

/* Darwin sa_flags (<sys/signal.h>) versus the Bionic/Linux values this project's <signal.h> uses. */
#define CRT_DARWIN_SA_ONSTACK 0x0001
#define CRT_DARWIN_SA_RESTART 0x0002
#define CRT_DARWIN_SA_RESETHAND 0x0004
#define CRT_DARWIN_SA_NOCLDSTOP 0x0008
#define CRT_DARWIN_SA_NODEFER 0x0010
#define CRT_DARWIN_SA_NOCLDWAIT 0x0020

static uint32_t darwin_mask_from_bionic(const sigset_t* set) {
  uint32_t darwin_mask = 0;
  int sig;

  if (set == 0) {
    return 0;
  }
  for (sig = 1; sig <= 31; ++sig) {
    if ((*set & ((sigset_t)1UL << (unsigned int)(sig - 1))) != 0 && kBionicToDarwin[sig] != 0) {
      darwin_mask |= (uint32_t)1U << (unsigned int)(kBionicToDarwin[sig] - 1);
    }
  }
  return darwin_mask;
}

static sigset_t bionic_mask_from_darwin(uint32_t darwin_mask) {
  sigset_t mask = 0;
  int sig;

  for (sig = 1; sig <= 31; ++sig) {
    if ((darwin_mask & ((uint32_t)1U << (unsigned int)(sig - 1))) != 0 && kDarwinToBionic[sig] != 0) {
      mask |= (sigset_t)1UL << (unsigned int)(kDarwinToBionic[sig] - 1);
    }
  }
  return mask;
}

static int darwin_flags_from_bionic(int flags) {
  int darwin_flags = 0;

  if ((flags & SA_ONSTACK) != 0) darwin_flags |= CRT_DARWIN_SA_ONSTACK;
  if ((flags & SA_RESTART) != 0) darwin_flags |= CRT_DARWIN_SA_RESTART;
  if ((flags & (int)SA_RESETHAND) != 0) darwin_flags |= CRT_DARWIN_SA_RESETHAND;
  if ((flags & SA_NOCLDSTOP) != 0) darwin_flags |= CRT_DARWIN_SA_NOCLDSTOP;
  if ((flags & SA_NODEFER) != 0) darwin_flags |= CRT_DARWIN_SA_NODEFER;
  if ((flags & SA_NOCLDWAIT) != 0) darwin_flags |= CRT_DARWIN_SA_NOCLDWAIT;
  return darwin_flags;
}

typedef int (*crt_darwin_pthread_sigmask_fn)(int, const uint32_t*, uint32_t*);
typedef int (*crt_darwin_sigsuspend_fn)(const uint32_t*);
typedef int (*crt_darwin_pthread_kill_fn)(void*, int);
typedef void* (*crt_darwin_pthread_main_thread_fn)(void);

static crt_darwin_pthread_sigmask_fn real_pthread_sigmask;
static crt_darwin_sigsuspend_fn real_sigsuspend;
static crt_darwin_pthread_kill_fn real_pthread_kill;
static crt_darwin_pthread_main_thread_fn real_pthread_main_thread_np;

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
  /* The thread-directed pieces live in libsystem_pthread/libsystem_kernel; ask the libSystem umbrella
   * first and then the exact library, the way pthread.c finds pthread_create(). */
  real_pthread_sigmask = (crt_darwin_pthread_sigmask_fn)__crt_macho_find_symbol_in_image(image, "pthread_sigmask");
  real_sigsuspend = (crt_darwin_sigsuspend_fn)__crt_macho_find_symbol_in_image(image, "sigsuspend");
  real_pthread_kill = (crt_darwin_pthread_kill_fn)__crt_macho_find_symbol_in_image(image, "pthread_kill");
  real_pthread_main_thread_np =
      (crt_darwin_pthread_main_thread_fn)__crt_macho_find_symbol_in_image(image, "pthread_main_thread_np");
  if (real_pthread_sigmask == 0) {
    real_pthread_sigmask = (crt_darwin_pthread_sigmask_fn)__crt_macho_find_symbol_in_loaded_image(
        "/usr/lib/system/libsystem_pthread.dylib", "pthread_sigmask");
  }
  if (real_pthread_kill == 0) {
    real_pthread_kill = (crt_darwin_pthread_kill_fn)__crt_macho_find_symbol_in_loaded_image(
        "/usr/lib/system/libsystem_pthread.dylib", "pthread_kill");
  }
  if (real_pthread_main_thread_np == 0) {
    real_pthread_main_thread_np = (crt_darwin_pthread_main_thread_fn)__crt_macho_find_symbol_in_loaded_image(
        "/usr/lib/system/libsystem_pthread.dylib", "pthread_main_thread_np");
  }
  if (real_sigsuspend == 0) {
    real_sigsuspend = (crt_darwin_sigsuspend_fn)__crt_macho_find_symbol_in_loaded_image(
        "/usr/lib/system/libsystem_kernel.dylib", "sigsuspend");
  }
}

#if defined(__aarch64__)
/* Darwin's records as the kernel hands them to an SA_SIGINFO handler on arm64 (struct __darwin_ucontext,
 * __darwin_mcontext64 and its exception/thread state), declared here because this libc does not
 * use Apple's headers. */
struct crt_darwin_stack {
  void* ss_sp;
  size_t ss_size;
  int ss_flags;
};

struct crt_darwin_arm_exception_state64 {
  uint64_t far;
  uint32_t esr;
  uint32_t exception;
};

struct crt_darwin_arm_thread_state64 {
  uint64_t x[29];
  uint64_t fp;
  uint64_t lr;
  uint64_t sp;
  uint64_t pc;
  uint32_t cpsr;
  uint32_t flags;
};

struct crt_darwin_mcontext64 {
  struct crt_darwin_arm_exception_state64 es;
  struct crt_darwin_arm_thread_state64 ss;
  /* the NEON state follows and is not used */
};

struct crt_darwin_ucontext {
  int uc_onstack;
  uint32_t uc_sigmask;
  struct crt_darwin_stack uc_stack;
  struct crt_darwin_ucontext* uc_link;
  size_t uc_mcsize;
  struct crt_darwin_mcontext64* uc_mcontext;
};


static void convert_to_linux_context(struct crt_linux_aarch64_ucontext* linux_context,
                                     const struct crt_darwin_ucontext* darwin_context) {
  const struct crt_darwin_mcontext64* machine = darwin_context->uc_mcontext;
  int i;

  memset(linux_context, 0, offsetof(struct crt_linux_aarch64_ucontext, uc_mcontext.reserved) + 16);
  linux_context->uc_sigmask = bionic_mask_from_darwin(darwin_context->uc_sigmask);
  linux_context->uc_stack.ss_sp = darwin_context->uc_stack.ss_sp;
  linux_context->uc_stack.ss_size = darwin_context->uc_stack.ss_size;
  if (machine == 0) {
    return;
  }
  linux_context->uc_mcontext.fault_address = machine->es.far;
  for (i = 0; i < 29; ++i) {
    linux_context->uc_mcontext.regs[i] = machine->ss.x[i];
  }
  linux_context->uc_mcontext.regs[29] = machine->ss.fp;
  linux_context->uc_mcontext.regs[30] = machine->ss.lr;
  linux_context->uc_mcontext.sp = machine->ss.sp;
  linux_context->uc_mcontext.pc = machine->ss.pc;
  linux_context->uc_mcontext.pstate = machine->ss.cpsr;
}

/* A handler that edits the interrupted context (WTF's thread-suspend and VM-trap handlers change the
 * pc and stack) must see its edits take effect on return, so they are written back into the
 * kernel's record. */
static void convert_from_linux_context(const struct crt_linux_aarch64_ucontext* linux_context,
                                       struct crt_darwin_ucontext* darwin_context) {
  struct crt_darwin_mcontext64* machine = darwin_context->uc_mcontext;
  int i;

  if (machine == 0) {
    return;
  }
  for (i = 0; i < 29; ++i) {
    machine->ss.x[i] = linux_context->uc_mcontext.regs[i];
  }
  machine->ss.fp = linux_context->uc_mcontext.regs[29];
  machine->ss.lr = linux_context->uc_mcontext.regs[30];
  machine->ss.sp = linux_context->uc_mcontext.sp;
  machine->ss.pc = linux_context->uc_mcontext.pc;
  machine->ss.cpsr = (uint32_t)linux_context->uc_mcontext.pstate;
}

static void convert_siginfo(siginfo_t* out, const struct crt_darwin_siginfo* in, int bionic_sig) {
  memset(out, 0, sizeof(*out));
  out->si_signo = bionic_sig;
  out->si_errno = in->si_errno;
  out->si_code = in->si_code;
  out->si_pid = in->host_si_pid;
  out->si_uid = (uid_t)in->host_si_uid;
  out->si_status = in->host_si_status;
  /* si_addr (the faulting address for SIGSEGV/SIGBUS/SIGILL/SIGFPE) sits after si_status at offset
   * 24 in Darwin's record, in the same union slot Bionic's si_addr uses. It aliases pid/uid for the
   * signals that carry those instead, so it is only read for the fault signals. */
  if (bionic_sig == SIGSEGV || bionic_sig == SIGBUS || bionic_sig == SIGILL || bionic_sig == SIGFPE ||
      bionic_sig == SIGTRAP) {
    out->si_addr = in->host_si_addr;
  }
}
#endif /* __aarch64__ */

/* Installed as the real host handler for every Bionic signal that maps to a
 * Darwin signal and is not SIG_DFL/SIG_IGN. Called by the kernel (via
 * Apple's own trampoline machinery) with a normal C calling convention. */
static void crt_macos_signal_entry(int darwin_sig, struct crt_darwin_siginfo* info, void* uctx) {
  int bionic_sig;

  if (darwin_sig < 1 || darwin_sig > 31) {
    return;
  }
  bionic_sig = kDarwinToBionic[darwin_sig];
  if (bionic_sig == 0) {
    return;
  }
#if defined(__aarch64__)
  if (info != 0 && uctx != 0) {
    siginfo_t bionic_info;
    struct crt_linux_aarch64_ucontext linux_context;

    convert_siginfo(&bionic_info, info, bionic_sig);
    convert_to_linux_context(&linux_context, (const struct crt_darwin_ucontext*)uctx);
    __crt_signal_dispatch_info(bionic_sig, &bionic_info, &linux_context);
    convert_from_linux_context(&linux_context, (struct crt_darwin_ucontext*)uctx);
    return;
  }
#endif
  (void)info;
  (void)uctx;
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

/* set_action_ex: like set_action(DISPATCH) but with the handler's real sa_flags and sa_mask, so a
 * handler such as WTF's that blocks every other signal while it runs keeps that guarantee. */
int __crt_signal_backend_set_action_ex(int bionic_sig, enum crt_signal_backend_action action,
                                       int flags, const sigset_t* mask) {
#if defined(__aarch64__)
  int darwin_sig;
  struct crt_darwin_sigaction sa;

  if (action != CRT_SIGNAL_BACKEND_DISPATCH) {
    return __crt_signal_backend_set_action(bionic_sig, action);
  }
  if (bionic_sig < 1 || bionic_sig > 31) {
    errno = EINVAL;
    return -1;
  }
  darwin_sig = kBionicToDarwin[bionic_sig];
  if (darwin_sig == 0) {
    return 0;
  }
  resolve_real_symbols();
  if (!real_symbols_available) {
    return 0;
  }
  sa.handler.handler_siginfo = crt_macos_signal_entry;
  sa.sa_mask = darwin_mask_from_bionic(mask);
  sa.sa_flags = CRT_DARWIN_SA_SIGINFO | darwin_flags_from_bionic(flags);
  if (real_sigaction(darwin_sig, &sa, 0) != 0) {
    errno = EINVAL;
    return -1;
  }
  return 0;
#else
  (void)flags;
  (void)mask;
  return __crt_signal_backend_set_action(bionic_sig, action);
#endif
}

/* The calling thread's real signal mask (it is per thread in the kernel). */
int __crt_signal_backend_sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
#if defined(__aarch64__)
  uint32_t darwin_set;
  uint32_t darwin_old = 0;
  int darwin_how;

  resolve_real_symbols();
  if (real_pthread_sigmask == 0) {
    return 1;
  }
  if (set == 0) {
    /* Read-only query: Darwin ignores `how` when the set is null. */
    if (real_pthread_sigmask(CRT_DARWIN_SIG_BLOCK, 0, &darwin_old) != 0) {
      errno = EINVAL;
      return -1;
    }
    if (oldset != 0) {
      *oldset = bionic_mask_from_darwin(darwin_old);
    }
    return 0;
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
  darwin_set = darwin_mask_from_bionic(set);
  if (real_pthread_sigmask(darwin_how, &darwin_set, &darwin_old) != 0) {
    errno = EINVAL;
    return -1;
  }
  if (oldset != 0) {
    *oldset = bionic_mask_from_darwin(darwin_old);
  }
  return 0;
#else
  (void)how;
  (void)set;
  (void)oldset;
  return 1;
#endif
}

/* Atomically replace the thread's mask and wait for a signal whose handler returns (EINTR). */
int __crt_signal_backend_sigsuspend(const sigset_t* mask) {
#if defined(__aarch64__)
  uint32_t darwin_mask;

  resolve_real_symbols();
  if (real_sigsuspend == 0) {
    return 1;
  }
  darwin_mask = darwin_mask_from_bionic(mask);
  real_sigsuspend(&darwin_mask);
  errno = EINTR;
  return -1;
#else
  (void)mask;
  return 1;
#endif
}

/* pthread_kill() for a thread created by pthread_create(): that thread is a real Apple pthread
 * underneath (see pthread.c), so the signal goes through Apple's pthread_kill with the Darwin number.
 * Returns 0, or an errno value. */
int __crt_macos_thread_kill(void* apple_pthread, int bionic_sig) {
  int darwin_sig;

  if (bionic_sig < 0 || bionic_sig > 31) {
    return EINVAL;
  }
  resolve_real_symbols();
  if (real_pthread_kill == 0) {
    return ENOTSUP;
  }
  darwin_sig = bionic_sig == 0 ? 0 : kBionicToDarwin[bionic_sig];
  if (bionic_sig != 0 && darwin_sig == 0) {
    return EINVAL;
  }
  return real_pthread_kill(apple_pthread, darwin_sig);
}

/* Apple's pthread_t of the process's initial thread (whose CRT pthread_t is its native thread id). */
void* __crt_macos_main_pthread(void) {
  resolve_real_symbols();
  return real_pthread_main_thread_np != 0 ? real_pthread_main_thread_np() : 0;
}
