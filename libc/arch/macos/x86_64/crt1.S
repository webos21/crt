.text
.globl _start
.p2align 4, 0x90
_start:
  xorl %ebp, %ebp
  movq %rdx, %rdi
  callq ___crt_env_init
  callq _main
  movl %eax, %edi
  callq _exit
