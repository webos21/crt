.text

.globl __crt_sys_read
.type __crt_sys_read, @function
.p2align 4, 0x90
__crt_sys_read:
  movq $0, %rax
  syscall
  retq
.size __crt_sys_read, . - __crt_sys_read

.globl __crt_sys_write
.type __crt_sys_write, @function
.p2align 4, 0x90
__crt_sys_write:
  movq $1, %rax
  syscall
  retq
.size __crt_sys_write, . - __crt_sys_write

.globl __crt_sys_open
.type __crt_sys_open, @function
.p2align 4, 0x90
__crt_sys_open:
  movq $2, %rax
  syscall
  retq
.size __crt_sys_open, . - __crt_sys_open

.globl __crt_sys_close
.type __crt_sys_close, @function
.p2align 4, 0x90
__crt_sys_close:
  movq $3, %rax
  syscall
  retq
.size __crt_sys_close, . - __crt_sys_close

.globl __crt_sys_lseek
.type __crt_sys_lseek, @function
.p2align 4, 0x90
__crt_sys_lseek:
  movq $8, %rax
  syscall
  retq
.size __crt_sys_lseek, . - __crt_sys_lseek

.globl __crt_sys_exit
.type __crt_sys_exit, @function
.p2align 4, 0x90
__crt_sys_exit:
  movq $60, %rax
  syscall
1:
  jmp 1b
.size __crt_sys_exit, . - __crt_sys_exit
