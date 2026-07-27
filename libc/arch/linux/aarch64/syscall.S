.text

.globl __crt_sys_write
.type __crt_sys_write, %function
.p2align 2
__crt_sys_write:
  mov x8, #64
  svc #0
  ret
.size __crt_sys_write, . - __crt_sys_write

.globl __crt_sys_exit
.type __crt_sys_exit, %function
.p2align 2
__crt_sys_exit:
  mov x8, #93
  svc #0
1:
  b 1b
.size __crt_sys_exit, . - __crt_sys_exit
