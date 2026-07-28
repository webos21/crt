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

.globl ___crt_sys_ftruncate
.p2align 2
___crt_sys_ftruncate:
  mov x16, #201
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_fsync
.p2align 2
___crt_sys_fsync:
  mov x16, #95
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_access
.p2align 2
___crt_sys_access:
  mov x16, #33
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_mkdir
.p2align 2
___crt_sys_mkdir:
  mov x16, #136
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_rmdir
.p2align 2
___crt_sys_rmdir:
  mov x16, #137
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_chdir
.p2align 2
___crt_sys_chdir:
  mov x16, #12
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_chmod
.p2align 2
___crt_sys_chmod:
  mov x16, #15
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_fchmod
.p2align 2
___crt_sys_fchmod:
  mov x16, #124
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_umask
.p2align 2
___crt_sys_umask:
  mov x16, #60
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_macos_fcntl
.p2align 2
___crt_sys_macos_fcntl:
  mov x16, #92
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_dup
.p2align 2
___crt_sys_dup:
  mov x16, #41
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_dup2
.p2align 2
___crt_sys_dup2:
  mov x16, #90
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_pipe
.p2align 2
___crt_sys_pipe:
  mov x9, x0
  mov x16, #42
  svc #0x80
  b.cs 1f
  str w0, [x9]
  str w1, [x9, #4]
  mov x0, #0
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_poll
.p2align 2
___crt_sys_poll:
  mov x16, #230
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_socket
.p2align 2
___crt_sys_socket:
  mov x16, #97
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_connect
.p2align 2
___crt_sys_connect:
  mov x16, #98
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_accept
.p2align 2
___crt_sys_accept:
  mov x16, #30
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_getsockname
.p2align 2
___crt_sys_getsockname:
  mov x16, #32
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_bind
.p2align 2
___crt_sys_bind:
  mov x16, #104
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_setsockopt
.p2align 2
___crt_sys_setsockopt:
  mov x16, #105
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_listen
.p2align 2
___crt_sys_listen:
  mov x16, #106
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_sendto
.p2align 2
___crt_sys_sendto:
  mov x16, #133
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_shutdown
.p2align 2
___crt_sys_shutdown:
  mov x16, #134
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_recvfrom
.p2align 2
___crt_sys_recvfrom:
  mov x16, #29
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_readlink
.p2align 2
___crt_sys_readlink:
  mov x16, #58
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_symlink
.p2align 2
___crt_sys_symlink:
  mov x16, #57
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_getdents
.p2align 2
___crt_sys_getdents:
  sub sp, sp, #16
  mov x3, sp
  mov x16, #344
  svc #0x80
  add sp, sp, #16
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_macos_stat64
.p2align 2
___crt_sys_macos_stat64:
  mov x16, #338
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_macos_fstat64
.p2align 2
___crt_sys_macos_fstat64:
  mov x16, #339
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_macos_lstat64
.p2align 2
___crt_sys_macos_lstat64:
  mov x16, #340
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_unlink
.p2align 2
___crt_sys_unlink:
  mov x16, #10
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_rename
.p2align 2
___crt_sys_rename:
  mov x16, #128
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

.globl ___crt_sys_getpid
.p2align 2
___crt_sys_getpid:
  mov x16, #20
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_getppid
.p2align 2
___crt_sys_getppid:
  mov x16, #39
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_kill
.p2align 2
___crt_sys_kill:
  mov x16, #37
  svc #0x80
  b.cs 1f
  ret
1:
  neg x0, x0
  ret

.globl ___crt_sys_thread_exit
.p2align 2
___crt_sys_thread_exit:
  mov x16, #1
  svc #0x80
1:
  b 1b

.globl ___crt_sys_exit
.p2align 2
___crt_sys_exit:
  mov x16, #1
  svc #0x80
1:
  b 1b
