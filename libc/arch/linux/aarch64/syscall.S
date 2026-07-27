.text

.globl __crt_sys_read
.type __crt_sys_read, %function
.p2align 2
__crt_sys_read:
  mov x8, #63
  svc #0
  ret
.size __crt_sys_read, . - __crt_sys_read

.globl __crt_sys_write
.type __crt_sys_write, %function
.p2align 2
__crt_sys_write:
  mov x8, #64
  svc #0
  ret
.size __crt_sys_write, . - __crt_sys_write

.globl __crt_sys_open
.type __crt_sys_open, %function
.p2align 2
__crt_sys_open:
  mov x3, x2
  mov x2, x1
  mov x1, x0
  mov x0, #-100
  mov x8, #56
  svc #0
  ret
.size __crt_sys_open, . - __crt_sys_open

.globl __crt_sys_close
.type __crt_sys_close, %function
.p2align 2
__crt_sys_close:
  mov x8, #57
  svc #0
  ret
.size __crt_sys_close, . - __crt_sys_close

.globl __crt_sys_lseek
.type __crt_sys_lseek, %function
.p2align 2
__crt_sys_lseek:
  mov x8, #62
  svc #0
  ret
.size __crt_sys_lseek, . - __crt_sys_lseek

.globl __crt_sys_mmap
.type __crt_sys_mmap, %function
.p2align 2
__crt_sys_mmap:
  mov x8, #222
  svc #0
  ret
.size __crt_sys_mmap, . - __crt_sys_mmap

.globl __crt_sys_munmap
.type __crt_sys_munmap, %function
.p2align 2
__crt_sys_munmap:
  mov x8, #215
  svc #0
  ret
.size __crt_sys_munmap, . - __crt_sys_munmap

.globl __crt_sys_gettimeofday
.type __crt_sys_gettimeofday, %function
.p2align 2
__crt_sys_gettimeofday:
  mov x1, #0
  mov x8, #169
  svc #0
  ret
.size __crt_sys_gettimeofday, . - __crt_sys_gettimeofday

.globl __crt_sys_nanosleep
.type __crt_sys_nanosleep, %function
.p2align 2
__crt_sys_nanosleep:
  mov x3, x1
  mov x2, x0
  mov x1, #0
  mov x0, #1
  mov x8, #115
  svc #0
  ret
.size __crt_sys_nanosleep, . - __crt_sys_nanosleep

.globl __crt_sys_exit
.type __crt_sys_exit, %function
.p2align 2
__crt_sys_exit:
  mov x8, #93
  svc #0
1:
  b 1b
.size __crt_sys_exit, . - __crt_sys_exit
