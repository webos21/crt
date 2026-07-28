.text
.globl _start
.type _start, %function
.p2align 2
_start:
  mov x29, #0
  mov x30, #0
  bl main
  bl exit
.size _start, . - _start
