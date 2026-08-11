.text
.globl _start
.type _start, %function
.p2align 2
_start:
  mov x29, #0
  mov x30, #0
  ldr x19, [sp]
  add x20, sp, #8
  add x0, sp, #16
  add x0, x0, x19, lsl #3
  mov x21, x0
  bl __crt_env_set_initial
  mov x0, x19
  mov x1, x20
  bl __crt_rootfs_bootstrap
  bl __crt_run_init_array
  mov x0, x19
  mov x1, x20
  mov x2, x21
  bl main
  bl exit
.size _start, . - _start
