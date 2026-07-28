.text
.globl _start
.type _start, %function
.p2align 2
_start:
  mov x29, #0
  mov x30, #0
  ldr x9, [sp]
  add x0, sp, #16
  add x0, x0, x9, lsl #3
  bl __crt_env_set_initial
  bl main
  bl exit
.size _start, . - _start
