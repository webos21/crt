.text

.globl ___crt_sys_write
.p2align 2
___crt_sys_write:
  mov x16, #4
  svc #0x80
  ret

.globl ___crt_sys_exit
.p2align 2
___crt_sys_exit:
  mov x16, #1
  svc #0x80
1:
  b 1b
