#ifndef CRT_UCONTEXT_H
#define CRT_UCONTEXT_H

#include <signal.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Real, working getcontext()/setcontext()/makecontext()/swapcontext() on
 * every host this project targets (Linux/macOS/Windows, x86_64/aarch64)
 * -- unlike most of the other "lower priority" gaps this session closed,
 * there is no honest host-level reason to hold any of the three hosts
 * back here: the underlying mechanism (save/restore the callee-saved
 * register set + stack pointer + resume address) is pure userspace state
 * manipulation, no syscall or OS-specific primitive involved, and this
 * project already has real, verified per-host/per-arch assembly doing
 * exactly that shape of work for setjmp()/longjmp() (see libc/src/arch/
 * <os>/<x86_64,aarch64>/setjmp.S) that this implementation mirrors
 * closely.
 *
 * On macOS and Windows/aarch64 this project's mcontext_t/ucontext_t are a private,
 * self-consistent layout -- NOT bit-compatible with the host's native
 * ucontext_t -- because signal delivery there does not hand a real ucontext_t
 * to SA_SIGINFO handlers, so get/set/swapcontext and makecontext only ever need
 * to agree with each other. On Linux (x86_64 and aarch64) the types are the
 * Bionic/kernel layouts below instead (2026-10-03, Web Tranche 1: JavaScriptCore
 * reads uc_mcontext.gregs[] in its thread-suspend signal handler), and the Linux
 * signal backend forwards the kernel's real siginfo_t and ucontext_t. Windows/x86_64 has the
 * Linux/x86_64 layout as well (Web Tranche 1 Windows signal gate): the CRT's own signal delivery
 * there fills it from the thread's CONTEXT, and get/set/swapcontext keep their Windows-only
 * slots (rdi, rsi, xmm6-xmm15, the TEB stack fields) in parts of the structure a signal frame
 * does not use (see private/crt_ucontext_offsets.h).
 *
 * makecontext() supports up to 4 int-sized (pointer-width) arguments --
 * a real, documented scope limit (not a silent gap): 4 is the number of
 * integer argument registers available on every ABI this implementation
 * covers without needing stack-spilled arguments (Windows x64 has only
 * 4; SysV x86_64 and AAPCS64 have 6+, but this implementation uses just
 * 4 of them for a uniform contract and a uniform, simple bootstrap
 * struct layout across all six per-host/per-arch assembly files). Real-
 * world makecontext() usage is overwhelmingly 0-2 arguments in practice.
 */

/* stack_t in the Linux kernel / Bionic order (ss_sp, ss_flags, ss_size). */
typedef struct {
  void* ss_sp;
  int ss_flags;
  size_t ss_size;
} stack_t;

#if defined(__x86_64__) && (defined(__linux__) || defined(CRT_TARGET_OS_WINDOWS))

/* Linux and Windows x86_64: the Bionic/kernel layout (bionic/libc/include/sys/ucontext.h), the
 * very structure the kernel hands to an SA_SIGINFO handler as its third
 * argument -- so a handler can read uc_mcontext.gregs[REG_RIP] and friends.
 * getcontext()/swapcontext() save the callee-saved registers into the same gregs
 * slots (what glibc does), makecontext() seeds REG_RBX/REG_RSP/REG_RIP. */
enum {
  REG_R8 = 0,
  REG_R9,
  REG_R10,
  REG_R11,
  REG_R12,
  REG_R13,
  REG_R14,
  REG_R15,
  REG_RDI,
  REG_RSI,
  REG_RBP,
  REG_RBX,
  REG_RDX,
  REG_RAX,
  REG_RCX,
  REG_RSP,
  REG_RIP,
  REG_EFL,
  REG_CSGSFS,
  REG_ERR,
  REG_TRAPNO,
  REG_OLDMASK,
  REG_CR2,
  NGREG
};
#define REG_R8 REG_R8
#define REG_R9 REG_R9
#define REG_R10 REG_R10
#define REG_R11 REG_R11
#define REG_R12 REG_R12
#define REG_R13 REG_R13
#define REG_R14 REG_R14
#define REG_R15 REG_R15
#define REG_RDI REG_RDI
#define REG_RSI REG_RSI
#define REG_RBP REG_RBP
#define REG_RBX REG_RBX
#define REG_RDX REG_RDX
#define REG_RAX REG_RAX
#define REG_RCX REG_RCX
#define REG_RSP REG_RSP
#define REG_RIP REG_RIP
#define REG_EFL REG_EFL
#define REG_CSGSFS REG_CSGSFS
#define REG_ERR REG_ERR
#define REG_TRAPNO REG_TRAPNO
#define REG_OLDMASK REG_OLDMASK
#define REG_CR2 REG_CR2

/* 64 bits wide on every host: Windows is LLP64, where `long` is 32 bits and the whole structure
 * (REG_*, offsetof) would shift. */
typedef long long greg_t;
typedef greg_t gregset_t[NGREG];

struct _libc_fpxreg {
  unsigned short significand[4];
  unsigned short exponent;
  unsigned short padding[3];
};

struct _libc_xmmreg {
  unsigned int element[4];
};

struct _libc_fpstate {
  unsigned short cwd;
  unsigned short swd;
  unsigned short ftw;
  unsigned short fop;
  unsigned long long rip;
  unsigned long long rdp;
  unsigned int mxcsr;
  unsigned int mxcr_mask;
  struct _libc_fpxreg _st[8];
  struct _libc_xmmreg _xmm[16];
  unsigned int __padding[24];
};

typedef struct _libc_fpstate* fpregset_t;

typedef struct {
  gregset_t gregs;
  fpregset_t fpregs;
  unsigned long long __reserved1[8];
} mcontext_t;

typedef struct __crt_ucontext {
  unsigned long long uc_flags;
  struct __crt_ucontext* uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  union {
    sigset_t uc_sigmask;
    sigset64_t uc_sigmask64;
  };
  struct _libc_fpstate __fpregs_mem;
  unsigned long long __ssp[4];
} ucontext_t;

#elif defined(__linux__) && defined(__aarch64__)

/* Linux aarch64: Bionic's `typedef struct sigcontext mcontext_t` and ucontext
 * (bionic/libc/include/sys/ucontext.h), the kernel's own signal frame layout.
 * getcontext()/swapcontext() save x19-x30, sp and pc into regs[]/sp/pc (the same
 * slots a signal frame uses); the callee-saved d8-d15 have no slot of their own
 * and are kept in the first 64 bytes of __reserved, which is free in a context
 * that did not come from a signal frame. */
struct sigcontext {
  unsigned long long fault_address;
  unsigned long long regs[31];
  unsigned long long sp;
  unsigned long long pc;
  unsigned long long pstate;
  unsigned char __reserved[4096] __attribute__((__aligned__(16)));
};

typedef struct sigcontext mcontext_t;

typedef struct __crt_ucontext {
  unsigned long uc_flags;
  struct __crt_ucontext* uc_link;
  stack_t uc_stack;
  union {
    sigset_t uc_sigmask;
    sigset64_t uc_sigmask64;
  };
  char __padding[128 - sizeof(sigset_t)];
  mcontext_t uc_mcontext;
} ucontext_t;

#else

typedef struct {
  /* Opaque; the byte layout is private to this file's per-arch assembly
   * (get/set/swapcontext.S) and to makecontext()'s own per-arch helper
   * in ucontext.c. Sized generously to comfortably fit the largest real
   * per-arch/per-OS need (Windows x86_64: 10 callee-saved GPRs + 10
   * callee-saved XMM registers + sp + pc + the TEB NT_TIB.StackBase/
   * StackLimit/DeallocationStack fields Windows specifically needs
   * updated on every stack switch -- see that file's own comment for
   * why -- 264 bytes). */
  unsigned char __opaque[264];
} mcontext_t;

typedef struct __crt_ucontext {
  /* uc_mcontext is deliberately the FIRST member (unlike glibc/Bionic's
   * real field order) so the per-arch assembly can treat a ucontext_t*
   * directly as a pointer to its own register-save area, exactly like
   * setjmp()'s jmp_buf -- POSIX does not mandate ucontext_t's field
   * order, only that it contains at least these four named members. */
  mcontext_t uc_mcontext;
  struct __crt_ucontext* uc_link;
  sigset_t uc_sigmask;
  stack_t uc_stack;
} ucontext_t;

#endif

int getcontext(ucontext_t* ucp);
int setcontext(const ucontext_t* ucp);
void makecontext(ucontext_t* ucp, void (*func)(void), int argc, ...);
int swapcontext(ucontext_t* oucp, ucontext_t* ucp);

#ifdef __cplusplus
}
#endif

#endif
