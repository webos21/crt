.text
.globl _start
.p2align 2
_start:
  mov x29, #0
  mov x30, #0
  mov x0, x2
  bl ___crt_env_init
  bl _main
  bl _exit
