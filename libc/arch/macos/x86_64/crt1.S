.text
.globl _start
.p2align 4, 0x90
_start:
  xorl %ebp, %ebp
  movq %rdi, %r12
  movq %rsi, %r13
  movq %rdx, %r14
  movq %rdx, %rdi
  callq ___crt_env_set_initial
  movq %r12, %rdi
  movq %r13, %rsi
  callq ___crt_rootfs_bootstrap
  movq %r12, %rdi
  movq %r13, %rsi
  movq %r14, %rdx
  callq _main
  movl %eax, %edi
  callq _exit
