/*
 * getcontext()/setcontext()/swapcontext() + the makecontext() trampoline,
 * Linux x86_64 (SysV ABI). Mirrors this same directory's setjmp.S
 * register set exactly (rbx/rbp/r12-r15 callee-saved GPRs + rsp + rip;
 * SysV does not require XMM registers to be callee-saved, so none are
 * saved here, matching setjmp.S). See include/ucontext.h for the private
 * mcontext_t byte-offset layout this file and ucontext.c both rely on:
 *   0: rbx (doubles as the makecontext() bootstrap-frame pointer)
 *   8: rbp   16: r12   24: r13   32: r14   40: r15
 *   48: rsp  56: rip (resume address)
 *
 * makecontext() bootstrap frame layout (ucontext.c), read by the
 * trampoline below via rbx (which setcontext()/swapcontext() will have
 * just restored from mcontext offset 0 on a makecontext()-created
 * context's first activation):
 *   0: func   8: argc (unused here)   16..40: argv[0..3]   48: link
 *
 * swapcontext()'s SP slot (offset 48) is deliberately saved *unadjusted*
 * (plain %rsp, not %rsp+8 the way getcontext()/setcontext() save it):
 * its own resume point (the "1:" label below) completes via a real `retq`
 * -- which itself pops the return address and advances %rsp by 8 -- not
 * via a raw `jmp` to a separately-saved target address the way
 * getcontext()'s resume path works. Saving the +8-adjusted value here
 * too skips past the real return address entirely, so `retq` at "1:"
 * pops whatever unrelated stack slot happens to follow it instead and
 * jumps to garbage -- a real bug, caught (and fixed) on the Windows
 * x86_64 sibling of this file by this project's own ucontext_test.c
 * coroutine round trip, which reliably crashed jumping to address 0
 * inside exactly this path. Reasoned to apply identically here (the
 * mechanism is pure x86_64 `call`/`ret` semantics, not OS-specific) but
 * not independently re-verified on real Linux hardware from this
 * Windows-only dev session.
 */
#include <private/crt_ucontext_offsets.h>

/* Register slots: the Bionic/kernel gregs[] slots of ucontext_t (see
 * include/ucontext.h and private/crt_ucontext_offsets.h), not the private
 * 0/8/.../56 layout the comment above describes for macOS/Windows. */
.text

.globl getcontext
.type getcontext, @function
.p2align 4, 0x90
getcontext:
  movq %rbx, CRT_UC_RBX(%rdi)
  movq %rbp, CRT_UC_RBP(%rdi)
  movq %r12, CRT_UC_R12(%rdi)
  movq %r13, CRT_UC_R13(%rdi)
  movq %r14, CRT_UC_R14(%rdi)
  movq %r15, CRT_UC_R15(%rdi)
  leaq 8(%rsp), %rax
  movq %rax, CRT_UC_RSP(%rdi)
  movq (%rsp), %rax
  movq %rax, CRT_UC_RIP(%rdi)
  xorl %eax, %eax
  retq
.size getcontext, . - getcontext

.globl setcontext
.type setcontext, @function
.p2align 4, 0x90
setcontext:
  movq CRT_UC_RBX(%rdi), %rbx
  movq CRT_UC_RBP(%rdi), %rbp
  movq CRT_UC_R12(%rdi), %r12
  movq CRT_UC_R13(%rdi), %r13
  movq CRT_UC_R14(%rdi), %r14
  movq CRT_UC_R15(%rdi), %r15
  movq CRT_UC_RSP(%rdi), %rsp
  jmp *CRT_UC_RIP(%rdi)
.size setcontext, . - setcontext

.globl swapcontext
.type swapcontext, @function
.p2align 4, 0x90
swapcontext:
  movq %rbx, CRT_UC_RBX(%rdi)
  movq %rbp, CRT_UC_RBP(%rdi)
  movq %r12, CRT_UC_R12(%rdi)
  movq %r13, CRT_UC_R13(%rdi)
  movq %r14, CRT_UC_R14(%rdi)
  movq %r15, CRT_UC_R15(%rdi)
  movq %rsp, CRT_UC_RSP(%rdi)
  leaq 1f(%rip), %rax
  movq %rax, CRT_UC_RIP(%rdi)
  movq CRT_UC_RBX(%rsi), %rbx
  movq CRT_UC_RBP(%rsi), %rbp
  movq CRT_UC_R12(%rsi), %r12
  movq CRT_UC_R13(%rsi), %r13
  movq CRT_UC_R14(%rsi), %r14
  movq CRT_UC_R15(%rsi), %r15
  movq CRT_UC_RSP(%rsi), %rsp
  jmp *CRT_UC_RIP(%rsi)
1:
  xorl %eax, %eax
  retq
.size swapcontext, . - swapcontext

.globl __crt_makecontext_trampoline
.type __crt_makecontext_trampoline, @function
.p2align 4, 0x90
__crt_makecontext_trampoline:
  movq 16(%rbx), %rdi
  movq 24(%rbx), %rsi
  movq 32(%rbx), %rdx
  movq 40(%rbx), %rcx
  call *0(%rbx)
  movq 48(%rbx), %rdi
  testq %rdi, %rdi
  jz 2f
  call setcontext
2:
  xorl %edi, %edi
  call exit
  ud2
.size __crt_makecontext_trampoline, . - __crt_makecontext_trampoline
