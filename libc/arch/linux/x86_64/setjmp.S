.text

.globl setjmp
.type setjmp, @function
.p2align 4, 0x90
setjmp:
.globl _setjmp
.type _setjmp, @function
_setjmp:
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
.size setjmp, . - setjmp
.size _setjmp, . - _setjmp

.globl longjmp
.type longjmp, @function
.p2align 4, 0x90
longjmp:
.globl _longjmp
.type _longjmp, @function
_longjmp:
  movl %esi, %eax
  testl %eax, %eax
  jne 1f
  movl $1, %eax
1:
  movq 0(%rdi), %rbx
  movq 8(%rdi), %rbp
  movq 16(%rdi), %r12
  movq 24(%rdi), %r13
  movq 32(%rdi), %r14
  movq 40(%rdi), %r15
  movq 48(%rdi), %rsp
  jmp *56(%rdi)
.size longjmp, . - longjmp
.size _longjmp, . - _longjmp
