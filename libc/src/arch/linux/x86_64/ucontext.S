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
.text

.globl getcontext
.type getcontext, @function
.p2align 4, 0x90
getcontext:
  movq %rbx, 0(%rdi)
  movq %rbp, 8(%rdi)
  movq %r12, 16(%rdi)
  movq %r13, 24(%rdi)
  movq %r14, 32(%rdi)
  movq %r15, 40(%rdi)
  leaq 8(%rsp), %rax
  movq %rax, 48(%rdi)
  movq (%rsp), %rax
  movq %rax, 56(%rdi)
  xorl %eax, %eax
  retq
.size getcontext, . - getcontext

.globl setcontext
.type setcontext, @function
.p2align 4, 0x90
setcontext:
  movq 0(%rdi), %rbx
  movq 8(%rdi), %rbp
  movq 16(%rdi), %r12
  movq 24(%rdi), %r13
  movq 32(%rdi), %r14
  movq 40(%rdi), %r15
  movq 48(%rdi), %rsp
  jmp *56(%rdi)
.size setcontext, . - setcontext

.globl swapcontext
.type swapcontext, @function
.p2align 4, 0x90
swapcontext:
  movq %rbx, 0(%rdi)
  movq %rbp, 8(%rdi)
  movq %r12, 16(%rdi)
  movq %r13, 24(%rdi)
  movq %r14, 32(%rdi)
  movq %r15, 40(%rdi)
  movq %rsp, 48(%rdi)
  leaq 1f(%rip), %rax
  movq %rax, 56(%rdi)
  movq 0(%rsi), %rbx
  movq 8(%rsi), %rbp
  movq 16(%rsi), %r12
  movq 24(%rsi), %r13
  movq 32(%rsi), %r14
  movq 40(%rsi), %r15
  movq 48(%rsi), %rsp
  jmp *56(%rsi)
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
