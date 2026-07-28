.text
.globl _start
.type _start, @function
.p2align 4, 0x90
_start:
  xorl %ebp, %ebp
  movq (%rsp), %rdi
  leaq 16(%rsp,%rdi,8), %rdi
  call __crt_env_init
  call main
  movl %eax, %edi
  call exit
.size _start, . - _start
