.text

.globl __crt_sys_write
.type __crt_sys_write, @function
.p2align 4, 0x90
__crt_sys_write:
  movq $1, %rax
  syscall
  retq
.size __crt_sys_write, . - __crt_sys_write

.globl __crt_sys_exit
.type __crt_sys_exit, @function
.p2align 4, 0x90
__crt_sys_exit:
  movq $60, %rax
  syscall
1:
  jmp 1b
.size __crt_sys_exit, . - __crt_sys_exit
