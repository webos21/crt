.text

.globl ___crt_sys_read
.p2align 2
___crt_sys_read:
  mov x16, #3
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_write
.p2align 2
___crt_sys_write:
  mov x16, #4
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_open
.p2align 2
___crt_sys_open:
  mov x16, #5
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_close
.p2align 2
___crt_sys_close:
  mov x16, #6
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_lseek
.p2align 2
___crt_sys_lseek:
  mov x16, #199
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_mmap
.p2align 2
___crt_sys_mmap:
  mov x16, #197
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_munmap
.p2align 2
___crt_sys_munmap:
  mov x16, #73
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_gettimeofday
.p2align 2
___crt_sys_gettimeofday:
  mov x1, #0
  mov x16, #116
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_sleep_ms
.p2align 2
___crt_sys_sleep_ms:
  mov x2, x0
  mov x0, #0
  mov x1, #0
  mov x16, #230
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_sched_yield
.p2align 2
___crt_sys_sched_yield:
  mov x0, #0
  mov x1, #0
  mov x2, #0
  mov x16, #230
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_thread_id
.p2align 2
___crt_sys_thread_id:
  mov x16, #372
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_exit
.p2align 2
___crt_sys_exit:
  mov x16, #1
  svc #0x80
1:
  b 1b
