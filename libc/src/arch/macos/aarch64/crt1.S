.text
.globl _start
.p2align 2
_start:
  mov x29, #0
  mov x30, #0
  mov x19, x0
  mov x20, x1
  mov x21, x2
  mov x0, x2
  bl ___crt_env_set_initial
  mov x0, x19
  mov x1, x20
  bl ___crt_rootfs_bootstrap
  mov x0, x19
  mov x1, x20
  mov x2, x21
  bl _main
  bl _exit
