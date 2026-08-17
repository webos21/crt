/*
 * getcontext()/setcontext()/swapcontext() + the makecontext() trampoline,
 * macOS x86_64 (SysV ABI -- identical register-save shape to the Linux
 * x86_64 variant of this same file, only the symbol-naming convention
 * differs, exactly like this directory's own setjmp.S vs the Linux one).
 * See linux/x86_64/ucontext.S's header comment for the full mcontext_t/
 * bootstrap-frame byte-offset layout -- identical here, including the
 * "swapcontext()'s SP slot is saved unadjusted, unlike getcontext()'s"
 * bug/fix that same comment documents in full.
 */
.text

.globl _getcontext
.p2align 4, 0x90
_getcontext:
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

.globl _setcontext
.p2align 4, 0x90
_setcontext:
  movq 0(%rdi), %rbx
  movq 8(%rdi), %rbp
  movq 16(%rdi), %r12
  movq 24(%rdi), %r13
  movq 32(%rdi), %r14
  movq 40(%rdi), %r15
  movq 48(%rdi), %rsp
  jmp *56(%rdi)

.globl _swapcontext
.p2align 4, 0x90
_swapcontext:
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

.globl ___crt_makecontext_trampoline
.p2align 4, 0x90
___crt_makecontext_trampoline:
  movq 16(%rbx), %rdi
  movq 24(%rbx), %rsi
  movq 32(%rbx), %rdx
  movq 40(%rbx), %rcx
  call *0(%rbx)
  movq 48(%rbx), %rdi
  testq %rdi, %rdi
  jz 2f
  call _setcontext
2:
  xorl %edi, %edi
  call _exit
  ud2
