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

.globl __crt_sys_clock_gettime
.type __crt_sys_clock_gettime, %function
.p2align 2
__crt_sys_clock_gettime:
  mov x8, #113
  svc #0
  ret
.size __crt_sys_clock_gettime, . - __crt_sys_clock_gettime

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

.globl __crt_sys_sched_yield
.type __crt_sys_sched_yield, %function
.p2align 2
__crt_sys_sched_yield:
  mov x8, #124
  svc #0
  ret
.size __crt_sys_sched_yield, . - __crt_sys_sched_yield

.globl __crt_sys_thread_id
.type __crt_sys_thread_id, %function
.p2align 2
__crt_sys_thread_id:
  mov x8, #178
  svc #0
  ret
.size __crt_sys_thread_id, . - __crt_sys_thread_id

.globl __crt_sys_clone_thread
.type __crt_sys_clone_thread, %function
.p2align 2
__crt_sys_clone_thread:
  stp x19, x20, [sp, #-16]!
  mov x19, x1
  mov x20, x2
  mov x1, x0
  mov x0, x3
  mov x2, #0
  mov x3, #0
  mov x4, #0
  mov x8, #220
  svc #0
  cbz x0, 1f
  ldp x19, x20, [sp], #16
  ret
1:
  mov x0, x20
  blr x19
  mov x8, #93
  svc #0
2:
  b 2b
.size __crt_sys_clone_thread, . - __crt_sys_clone_thread

.globl __crt_sys_wait4
.type __crt_sys_wait4, %function
.p2align 2
__crt_sys_wait4:
  mov x8, #260
  svc #0
  ret
.size __crt_sys_wait4, . - __crt_sys_wait4

.globl __crt_sys_thread_exit
.type __crt_sys_thread_exit, %function
.p2align 2
__crt_sys_thread_exit:
  mov x8, #93
  svc #0
1:
  b 1b
.size __crt_sys_thread_exit, . - __crt_sys_thread_exit

.globl __crt_sys_exit
.type __crt_sys_exit, %function
.p2align 2
__crt_sys_exit:
  mov x8, #93
  svc #0
1:
  b 1b
.size __crt_sys_exit, . - __crt_sys_exit
