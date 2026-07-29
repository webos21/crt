.text

.globl setjmp
.def setjmp; .scl 2; .type 32; .endef
.p2align 4, 0x90
setjmp:
.globl _setjmp
.def _setjmp; .scl 2; .type 32; .endef
_setjmp:
  movq %rbx, 0(%rcx)
  movq %rbp, 8(%rcx)
  movq %rdi, 16(%rcx)
  movq %rsi, 24(%rcx)
  movq %r12, 32(%rcx)
  movq %r13, 40(%rcx)
  movq %r14, 48(%rcx)
  movq %r15, 56(%rcx)
  leaq 8(%rsp), %rax
  movq %rax, 64(%rcx)
  movq (%rsp), %rax
  movq %rax, 72(%rcx)
  movdqu %xmm6, 80(%rcx)
  movdqu %xmm7, 96(%rcx)
  movdqu %xmm8, 112(%rcx)
  movdqu %xmm9, 128(%rcx)
  movdqu %xmm10, 144(%rcx)
  movdqu %xmm11, 160(%rcx)
  movdqu %xmm12, 176(%rcx)
  movdqu %xmm13, 192(%rcx)
  movdqu %xmm14, 208(%rcx)
  movdqu %xmm15, 224(%rcx)
  xorl %eax, %eax
  retq

.globl longjmp
.def longjmp; .scl 2; .type 32; .endef
.p2align 4, 0x90
longjmp:
.globl _longjmp
.def _longjmp; .scl 2; .type 32; .endef
_longjmp:
  movl %edx, %eax
  testl %eax, %eax
  jne 1f
  movl $1, %eax
1:
  movq 0(%rcx), %rbx
  movq 8(%rcx), %rbp
  movq 16(%rcx), %rdi
  movq 24(%rcx), %rsi
  movq 32(%rcx), %r12
  movq 40(%rcx), %r13
  movq 48(%rcx), %r14
  movq 56(%rcx), %r15
  movq 64(%rcx), %rsp
  movdqu 80(%rcx), %xmm6
  movdqu 96(%rcx), %xmm7
  movdqu 112(%rcx), %xmm8
  movdqu 128(%rcx), %xmm9
  movdqu 144(%rcx), %xmm10
  movdqu 160(%rcx), %xmm11
  movdqu 176(%rcx), %xmm12
  movdqu 192(%rcx), %xmm13
  movdqu 208(%rcx), %xmm14
  movdqu 224(%rcx), %xmm15
  jmp *72(%rcx)
