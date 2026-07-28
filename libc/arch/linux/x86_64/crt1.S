.text
.globl _start
.type _start, @function
.p2align 4, 0x90
_start:
  xorl %ebp, %ebp
  call main
  movl %eax, %edi
  call exit
.size _start, . - _start
