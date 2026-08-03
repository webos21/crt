.text
.globl _start
.type _start, @function
.p2align 4, 0x90
_start:
  xorl %ebp, %ebp
  movq (%rsp), %r12
  leaq 8(%rsp), %r13
  leaq 16(%rsp,%r12,8), %r14
  movq %r14, %rdi
  call __crt_env_set_initial
  movq %r12, %rdi
  movq %r13, %rsi
  call __crt_rootfs_bootstrap
  movq %r12, %rdi
  movq %r13, %rsi
  movq %r14, %rdx
  call main
  movl %eax, %edi
  call exit
.size _start, . - _start
