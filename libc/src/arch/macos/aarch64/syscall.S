.text

.globl ___crt_sys_read
.p2align 2
___crt_sys_read:
  mov x16, #3
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_pread
.p2align 2
___crt_sys_pread:
  mov x16, #153
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_pwrite
.p2align 2
___crt_sys_pwrite:
  mov x16, #154
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_fcntl
.p2align 2
___crt_sys_fcntl:
  mov x16, #92
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_ioctl
.p2align 2
___crt_sys_ioctl:
  mov x16, #54
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_macos_sysctl
.p2align 2
___crt_sys_macos_sysctl:
  mov x16, #202
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_macos_statfs64
.p2align 2
___crt_sys_macos_statfs64:
  mov x16, #345
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_macos_fstatfs64
.p2align 2
___crt_sys_macos_fstatfs64:
  mov x16, #346
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_fchdir
.p2align 2
___crt_sys_fchdir:
  mov x16, #13
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_getsockopt
.p2align 2
___crt_sys_getsockopt:
  mov x16, #118
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

/* sendmsg()/recvmsg() -- Darwin/XNU BSD syscall numbers 28 / 27, from the
 * classic BSD socket syscall block this project's own already-tested
 * recvfrom=29 and accept=30 both sit in -- recvmsg/sendmsg are the two
 * slots immediately before recvfrom in that same table. NOT independently
 * verified on real macOS hardware from this session (Windows-only dev
 * environment) -- see HISTORY.md's 2026-08-16 entry, matching the same
 * caveat linkat()'s own trampolines carried until real hardware
 * verification closed it. */
.globl ___crt_sys_sendmsg
.p2align 2
___crt_sys_sendmsg:
  mov x16, #28
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_recvmsg
.p2align 2
___crt_sys_recvmsg:
  mov x16, #27
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

/* link(oldpath, newpath): Darwin keeps the legacy 2-arg BSD link syscall
 * (9) available on every arch -- no *at() shuffle needed, same reasoning
 * as x86_64 Linux's own __crt_sys_link. __crt_sys_link(oldpath, newpath)
 * already matches link(2)'s own (x0, x1) argument order. */
.globl ___crt_sys_link
.p2align 2
___crt_sys_link:
  mov x16, #9
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_mprotect
.p2align 2
___crt_sys_mprotect:
  mov x16, #74
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_msync
.p2align 2
___crt_sys_msync:
  mov x16, #65
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_mremap
.p2align 2
___crt_sys_mremap:
  mov x0, #-38
  ret

.globl ___crt_sys_mlockall
.p2align 2
___crt_sys_mlockall:
  mov x0, #-38
  ret

.globl ___crt_sys_munlockall
.p2align 2
___crt_sys_munlockall:
  mov x0, #-38
  ret

.globl ___crt_sys_mlock
.p2align 2
___crt_sys_mlock:
  mov x16, #203
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_mlock2
.p2align 2
___crt_sys_mlock2:
  mov x0, #-38
  ret

.globl ___crt_sys_munlock
.p2align 2
___crt_sys_munlock:
  mov x16, #204
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_mincore
.p2align 2
___crt_sys_mincore:
  mov x16, #78
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_madvise
.p2align 2
___crt_sys_madvise:
  mov x16, #75
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_utimes
.p2align 2
___crt_sys_utimes:
  mov x16, #138
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_futimes
.p2align 2
___crt_sys_futimes:
  mov x16, #139
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_setpgid
.p2align 2
___crt_sys_setpgid:
  mov x16, #82
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_getpgrp
.p2align 2
___crt_sys_getpgrp:
  mov x16, #81
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_setsid
.p2align 2
___crt_sys_setsid:
  mov x16, #147
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_fork
.p2align 2
___crt_sys_fork:
  mov x16, #2
  svc #0x80
  b.cs 1f
  cbz x1, 2f
  mov x0, #0
2:
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_execve
.p2align 2
___crt_sys_execve:
  mov x16, #59
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_wait4
.p2align 2
___crt_sys_wait4:
  mov x16, #7
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_geteuid
.p2align 2
___crt_sys_geteuid:
  mov x16, #25
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

/* getegid() -- Darwin/XNU BSD syscall number 43, from the classic BSD
 * syscall table this project's own already-tested/wired ___crt_sys_
 * dup=41 / ___crt_sys_pipe=42 sit in immediately before it (see those two
 * trampolines above) -- getegid is the very next slot in that same,
 * essentially unchanged-for-decades numbering, matching the identical
 * x86_64 reasoning (libc/src/arch/macos/x86_64/syscall.S's own
 * ___crt_sys_getegid, same comment in full). Added 2026-09-02 as the real
 * macOS counterpart to a real bug found and fixed on Linux the same day
 * (HISTORY.md's 2026-09-02 entry) -- see that x86_64 comment for the
 * full story. NOT independently verified on real macOS hardware from
 * this session (Windows-only dev environment), matching this file's own
 * established unverified-syscall-trampoline discipline. */
.globl ___crt_sys_getegid
.p2align 2
___crt_sys_getegid:
  mov x16, #43
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
  neg x0, x0
  ret

.globl ___crt_sys_fchown
.p2align 2
___crt_sys_fchown:
  mov x16, #123
  svc #0x80
  b.cs 1f
  ret
1:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  bl ___crt_macos_to_bionic_errno
  ldp x29, x30, [sp], #16
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
