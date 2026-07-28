.text
.globl _start
.p2align 4, 0x90
_start:
  xorl %ebp, %ebp
  callq _main
  movl %eax, %edi
  callq _exit
