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
 * This project's mcontext_t/ucontext_t are a private, self-consistent
 * layout -- NOT bit-compatible with any host's native ucontext_t (glibc's,
 * Darwin's, or a real Linux kernel sigcontext). That is a deliberate,
 * safe choice: this project's own signal delivery does not hand a real
 * ucontext_t to SA_SIGINFO handlers today (sa_sigaction's third
 * parameter is an opaque void*, see signal.h), so there is no existing
 * ABI this needs to match -- get/set/swapcontext and makecontext only
 * ever need to agree with each other.
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

typedef struct {
  void* ss_sp;
  size_t ss_size;
  int ss_flags;
} stack_t;

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

int getcontext(ucontext_t* ucp);
int setcontext(const ucontext_t* ucp);
void makecontext(ucontext_t* ucp, void (*func)(void), int argc, ...);
int swapcontext(ucontext_t* oucp, ucontext_t* ucp);

#ifdef __cplusplus
}
#endif

#endif
