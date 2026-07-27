.text
.globl _start
.p2align 2
_start:
  mov x29, #0
  mov x30, #0
  bl _main
  bl ___crt_sys_exit
