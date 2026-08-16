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

.globl __crt_sys_pread
.type __crt_sys_pread, %function
.p2align 2
__crt_sys_pread:
  mov x8, #67
  svc #0
  ret
.size __crt_sys_pread, . - __crt_sys_pread

.globl __crt_sys_pwrite
.type __crt_sys_pwrite, %function
.p2align 2
__crt_sys_pwrite:
  mov x8, #68
  svc #0
  ret
.size __crt_sys_pwrite, . - __crt_sys_pwrite

.globl __crt_sys_fcntl
.type __crt_sys_fcntl, %function
.p2align 2
__crt_sys_fcntl:
  mov x8, #25
  svc #0
  ret
.size __crt_sys_fcntl, . - __crt_sys_fcntl

.globl __crt_sys_ioctl
.type __crt_sys_ioctl, %function
.p2align 2
__crt_sys_ioctl:
  mov x8, #29
  svc #0
  ret
.size __crt_sys_ioctl, . - __crt_sys_ioctl

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

.globl __crt_sys_ftruncate
.type __crt_sys_ftruncate, %function
.p2align 2
__crt_sys_ftruncate:
  mov x8, #46
  svc #0
  ret
.size __crt_sys_ftruncate, . - __crt_sys_ftruncate

.globl __crt_sys_fsync
.type __crt_sys_fsync, %function
.p2align 2
__crt_sys_fsync:
  mov x8, #82
  svc #0
  ret
.size __crt_sys_fsync, . - __crt_sys_fsync

.globl __crt_sys_statfs
.type __crt_sys_statfs, %function
.p2align 2
__crt_sys_statfs:
  mov x8, #43
  svc #0
  ret
.size __crt_sys_statfs, . - __crt_sys_statfs

.globl __crt_sys_fstatfs
.type __crt_sys_fstatfs, %function
.p2align 2
__crt_sys_fstatfs:
  mov x8, #44
  svc #0
  ret
.size __crt_sys_fstatfs, . - __crt_sys_fstatfs

.globl __crt_sys_access
.type __crt_sys_access, %function
.p2align 2
__crt_sys_access:
  mov x2, #0
  mov x1, x0
  mov x0, #-100
  mov x8, #48
  svc #0
  ret
.size __crt_sys_access, . - __crt_sys_access

.globl __crt_sys_mkdir
.type __crt_sys_mkdir, %function
.p2align 2
__crt_sys_mkdir:
  mov x2, x1
  mov x1, x0
  mov x0, #-100
  mov x8, #34
  svc #0
  ret
.size __crt_sys_mkdir, . - __crt_sys_mkdir

.globl __crt_sys_rmdir
.type __crt_sys_rmdir, %function
.p2align 2
__crt_sys_rmdir:
  mov x2, #512
  mov x1, x0
  mov x0, #-100
  mov x8, #35
  svc #0
  ret
.size __crt_sys_rmdir, . - __crt_sys_rmdir

.globl __crt_sys_chdir
.type __crt_sys_chdir, %function
.p2align 2
__crt_sys_chdir:
  mov x8, #49
  svc #0
  ret
.size __crt_sys_chdir, . - __crt_sys_chdir

.globl __crt_sys_fchdir
.type __crt_sys_fchdir, %function
.p2align 2
__crt_sys_fchdir:
  mov x8, #50
  svc #0
  ret
.size __crt_sys_fchdir, . - __crt_sys_fchdir

.globl __crt_sys_chmod
.type __crt_sys_chmod, %function
.p2align 2
__crt_sys_chmod:
  mov x2, x1
  mov x1, x0
  mov x0, #-100
  mov x3, #0
  mov x8, #53
  svc #0
  ret
.size __crt_sys_chmod, . - __crt_sys_chmod

.globl __crt_sys_fchmod
.type __crt_sys_fchmod, %function
.p2align 2
__crt_sys_fchmod:
  mov x8, #52
  svc #0
  ret
.size __crt_sys_fchmod, . - __crt_sys_fchmod

.globl __crt_sys_umask
.type __crt_sys_umask, %function
.p2align 2
__crt_sys_umask:
  mov x8, #166
  svc #0
  ret
.size __crt_sys_umask, . - __crt_sys_umask

.globl __crt_sys_getcwd
.type __crt_sys_getcwd, %function
.p2align 2
__crt_sys_getcwd:
  mov x8, #17
  svc #0
  ret
.size __crt_sys_getcwd, . - __crt_sys_getcwd

.globl __crt_sys_dup
.type __crt_sys_dup, %function
.p2align 2
__crt_sys_dup:
  mov x8, #23
  svc #0
  ret
.size __crt_sys_dup, . - __crt_sys_dup

.globl __crt_sys_dup2
.type __crt_sys_dup2, %function
.p2align 2
__crt_sys_dup2:
  mov x2, #0
  mov x8, #24
  svc #0
  ret
.size __crt_sys_dup2, . - __crt_sys_dup2

.globl __crt_sys_pipe
.type __crt_sys_pipe, %function
.p2align 2
__crt_sys_pipe:
  mov x1, #0
  mov x8, #59
  svc #0
  ret
.size __crt_sys_pipe, . - __crt_sys_pipe

.globl __crt_sys_ppoll
.type __crt_sys_ppoll, %function
.p2align 2
__crt_sys_ppoll:
  mov x3, #0
  mov x4, #0
  mov x8, #73
  svc #0
  ret
.size __crt_sys_ppoll, . - __crt_sys_ppoll

.globl __crt_sys_socket
.type __crt_sys_socket, %function
.p2align 2
__crt_sys_socket:
  mov x8, #198
  svc #0
  ret
.size __crt_sys_socket, . - __crt_sys_socket

.globl __crt_sys_connect
.type __crt_sys_connect, %function
.p2align 2
__crt_sys_connect:
  mov x8, #203
  svc #0
  ret
.size __crt_sys_connect, . - __crt_sys_connect

.globl __crt_sys_accept
.type __crt_sys_accept, %function
.p2align 2
__crt_sys_accept:
  mov x3, #0
  mov x8, #242
  svc #0
  ret
.size __crt_sys_accept, . - __crt_sys_accept

.globl __crt_sys_sendto
.type __crt_sys_sendto, %function
.p2align 2
__crt_sys_sendto:
  mov x8, #206
  svc #0
  ret
.size __crt_sys_sendto, . - __crt_sys_sendto

.globl __crt_sys_recvfrom
.type __crt_sys_recvfrom, %function
.p2align 2
__crt_sys_recvfrom:
  mov x8, #207
  svc #0
  ret
.size __crt_sys_recvfrom, . - __crt_sys_recvfrom

/* sendmsg(int sockfd, const struct msghdr *msg, int flags). Syscall
 * numbers 211/212 per include/uapi/asm-generic/unistd.h (the generic
 * table aarch64 uses), right after sendto=206/recvfrom=207 above. NOT
 * independently verified on real Linux hardware from this session
 * (Windows-only dev environment) -- see HISTORY.md's 2026-08-16 entry,
 * matching the same caveat linkat()'s own trampolines carried until real
 * hardware verification closed it. */
.globl __crt_sys_sendmsg
.type __crt_sys_sendmsg, %function
.p2align 2
__crt_sys_sendmsg:
  mov x8, #211
  svc #0
  ret
.size __crt_sys_sendmsg, . - __crt_sys_sendmsg

.globl __crt_sys_recvmsg
.type __crt_sys_recvmsg, %function
.p2align 2
__crt_sys_recvmsg:
  mov x8, #212
  svc #0
  ret
.size __crt_sys_recvmsg, . - __crt_sys_recvmsg

.globl __crt_sys_bind
.type __crt_sys_bind, %function
.p2align 2
__crt_sys_bind:
  mov x8, #200
  svc #0
  ret
.size __crt_sys_bind, . - __crt_sys_bind

.globl __crt_sys_listen
.type __crt_sys_listen, %function
.p2align 2
__crt_sys_listen:
  mov x8, #201
  svc #0
  ret
.size __crt_sys_listen, . - __crt_sys_listen

.globl __crt_sys_getsockname
.type __crt_sys_getsockname, %function
.p2align 2
__crt_sys_getsockname:
  mov x8, #204
  svc #0
  ret
.size __crt_sys_getsockname, . - __crt_sys_getsockname

.globl __crt_sys_setsockopt
.type __crt_sys_setsockopt, %function
.p2align 2
__crt_sys_setsockopt:
  mov x8, #208
  svc #0
  ret
.size __crt_sys_setsockopt, . - __crt_sys_setsockopt

.globl __crt_sys_getsockopt
.type __crt_sys_getsockopt, %function
.p2align 2
__crt_sys_getsockopt:
  mov x8, #209
  svc #0
  ret
.size __crt_sys_getsockopt, . - __crt_sys_getsockopt

.globl __crt_sys_shutdown
.type __crt_sys_shutdown, %function
.p2align 2
__crt_sys_shutdown:
  mov x8, #210
  svc #0
  ret
.size __crt_sys_shutdown, . - __crt_sys_shutdown

.globl __crt_sys_readlink
.type __crt_sys_readlink, %function
.p2align 2
__crt_sys_readlink:
  mov x4, x2
  mov x2, x1
  mov x1, x0
  mov x0, #-100
  mov x3, x4
  mov x8, #78
  svc #0
  ret
.size __crt_sys_readlink, . - __crt_sys_readlink

.globl __crt_sys_symlink
.type __crt_sys_symlink, %function
.p2align 2
__crt_sys_symlink:
  mov x2, x1
  mov x1, #-100
  mov x8, #36
  svc #0
  ret
.size __crt_sys_symlink, . - __crt_sys_symlink

.globl __crt_sys_getdents
.type __crt_sys_getdents, %function
.p2align 2
__crt_sys_getdents:
  mov x8, #61
  svc #0
  ret
.size __crt_sys_getdents, . - __crt_sys_getdents

.globl __crt_sys_statx
.type __crt_sys_statx, %function
.p2align 2
__crt_sys_statx:
  mov x8, #291
  svc #0
  ret
.size __crt_sys_statx, . - __crt_sys_statx

.globl __crt_sys_unlink
.type __crt_sys_unlink, %function
.p2align 2
__crt_sys_unlink:
  mov x1, x0
  mov x0, #-100
  mov x2, #0
  mov x8, #35
  svc #0
  ret
.size __crt_sys_unlink, . - __crt_sys_unlink

/* link(oldpath, newpath): aarch64 dropped the legacy 2-arg link syscall
 * entirely (only the generic-syscall-ABI *at() forms exist, same reason
 * __crt_sys_symlink/__crt_sys_unlink above route through symlinkat/
 * unlinkat) -- linkat(AT_FDCWD, oldpath, AT_FDCWD, newpath, 0) is the only
 * way to do this on this arch. __crt_sys_link(oldpath, newpath) arrives as
 * (x0, x1); shuffle to (x0=AT_FDCWD, x1=oldpath, x2=AT_FDCWD, x3=newpath,
 * x4=flags) for linkat's own (olddirfd, oldpath, newdirfd, newpath, flags)
 * signature, saving x1's original value (newpath) into x3 and x0's
 * (oldpath) into x1 before either gets overwritten. */
.globl __crt_sys_link
.type __crt_sys_link, %function
.p2align 2
__crt_sys_link:
  mov x3, x1
  mov x1, x0
  mov x0, #-100
  mov x2, #-100
  mov x4, #0
  mov x8, #37
  svc #0
  ret
.size __crt_sys_link, . - __crt_sys_link

.globl __crt_sys_rename
.type __crt_sys_rename, %function
.p2align 2
__crt_sys_rename:
  mov x3, x1
  mov x2, #-100
  mov x1, x0
  mov x0, #-100
  mov x8, #38
  svc #0
  ret
.size __crt_sys_rename, . - __crt_sys_rename

.globl __crt_sys_mmap
.type __crt_sys_mmap, %function
.p2align 2
__crt_sys_mmap:
  mov x8, #222
  svc #0
  ret
.size __crt_sys_mmap, . - __crt_sys_mmap

.globl __crt_sys_mprotect
.type __crt_sys_mprotect, %function
.p2align 2
__crt_sys_mprotect:
  mov x8, #226
  svc #0
  ret
.size __crt_sys_mprotect, . - __crt_sys_mprotect

.globl __crt_sys_munmap
.type __crt_sys_munmap, %function
.p2align 2
__crt_sys_munmap:
  mov x8, #215
  svc #0
  ret
.size __crt_sys_munmap, . - __crt_sys_munmap

.globl __crt_sys_msync
.type __crt_sys_msync, %function
.p2align 2
__crt_sys_msync:
  mov x8, #227
  svc #0
  ret
.size __crt_sys_msync, . - __crt_sys_msync

.globl __crt_sys_mremap
.type __crt_sys_mremap, %function
.p2align 2
__crt_sys_mremap:
  mov x8, #216
  svc #0
  ret
.size __crt_sys_mremap, . - __crt_sys_mremap

.globl __crt_sys_mlockall
.type __crt_sys_mlockall, %function
.p2align 2
__crt_sys_mlockall:
  mov x8, #230
  svc #0
  ret
.size __crt_sys_mlockall, . - __crt_sys_mlockall

.globl __crt_sys_munlockall
.type __crt_sys_munlockall, %function
.p2align 2
__crt_sys_munlockall:
  mov x8, #231
  svc #0
  ret
.size __crt_sys_munlockall, . - __crt_sys_munlockall

.globl __crt_sys_mlock
.type __crt_sys_mlock, %function
.p2align 2
__crt_sys_mlock:
  mov x8, #228
  svc #0
  ret
.size __crt_sys_mlock, . - __crt_sys_mlock

.globl __crt_sys_mlock2
.type __crt_sys_mlock2, %function
.p2align 2
__crt_sys_mlock2:
  mov x8, #284
  svc #0
  ret
.size __crt_sys_mlock2, . - __crt_sys_mlock2

.globl __crt_sys_munlock
.type __crt_sys_munlock, %function
.p2align 2
__crt_sys_munlock:
  mov x8, #229
  svc #0
  ret
.size __crt_sys_munlock, . - __crt_sys_munlock

.globl __crt_sys_mincore
.type __crt_sys_mincore, %function
.p2align 2
__crt_sys_mincore:
  mov x8, #232
  svc #0
  ret
.size __crt_sys_mincore, . - __crt_sys_mincore

.globl __crt_sys_madvise
.type __crt_sys_madvise, %function
.p2align 2
__crt_sys_madvise:
  mov x8, #233
  svc #0
  ret
.size __crt_sys_madvise, . - __crt_sys_madvise

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

.globl __crt_sys_utimensat
.type __crt_sys_utimensat, %function
.p2align 2
__crt_sys_utimensat:
  mov x8, #88
  svc #0
  ret
.size __crt_sys_utimensat, . - __crt_sys_utimensat

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
  stp x21, x22, [sp, #-16]!
  mov x19, x1
  mov x20, x2
  mov x21, x4
  mov x22, x5
  mov x1, x0
  mov x0, x3
  mov x2, x21
  mov x3, x6
  mov x4, x22
  mov x8, #220
  svc #0
  cbz x0, 1f
  ldp x21, x22, [sp], #16
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

.globl __crt_sys_fork
.type __crt_sys_fork, %function
.p2align 2
__crt_sys_fork:
  mov x0, #17
  mov x1, #0
  mov x2, #0
  mov x3, #0
  mov x4, #0
  mov x8, #220
  svc #0
  ret
.size __crt_sys_fork, . - __crt_sys_fork

.globl __crt_sys_futex
.type __crt_sys_futex, %function
.p2align 2
__crt_sys_futex:
  mov x8, #98
  svc #0
  ret
.size __crt_sys_futex, . - __crt_sys_futex

.globl __crt_sys_getpid
.type __crt_sys_getpid, %function
.p2align 2
__crt_sys_getpid:
  mov x8, #172
  svc #0
  ret
.size __crt_sys_getpid, . - __crt_sys_getpid

.globl __crt_sys_getppid
.type __crt_sys_getppid, %function
.p2align 2
__crt_sys_getppid:
  mov x8, #173
  svc #0
  ret
.size __crt_sys_getppid, . - __crt_sys_getppid

.globl __crt_sys_setpgid
.type __crt_sys_setpgid, %function
.p2align 2
__crt_sys_setpgid:
  mov x8, #154
  svc #0
  ret
.size __crt_sys_setpgid, . - __crt_sys_setpgid

.globl __crt_sys_getpgrp
.type __crt_sys_getpgrp, %function
.p2align 2
__crt_sys_getpgrp:
  mov x0, #0
  mov x8, #155
  svc #0
  ret
.size __crt_sys_getpgrp, . - __crt_sys_getpgrp

.globl __crt_sys_setsid
.type __crt_sys_setsid, %function
.p2align 2
__crt_sys_setsid:
  mov x8, #157
  svc #0
  ret
.size __crt_sys_setsid, . - __crt_sys_setsid

.globl __crt_sys_geteuid
.type __crt_sys_geteuid, %function
.p2align 2
__crt_sys_geteuid:
  mov x8, #175
  svc #0
  ret
.size __crt_sys_geteuid, . - __crt_sys_geteuid

.globl __crt_sys_fchown
.type __crt_sys_fchown, %function
.p2align 2
__crt_sys_fchown:
  mov x8, #55
  svc #0
  ret
.size __crt_sys_fchown, . - __crt_sys_fchown

.globl __crt_sys_kill
.type __crt_sys_kill, %function
.p2align 2
__crt_sys_kill:
  mov x8, #129
  svc #0
  ret
.size __crt_sys_kill, . - __crt_sys_kill

.globl __crt_sys_execve
.type __crt_sys_execve, %function
.p2align 2
__crt_sys_execve:
  mov x8, #221
  svc #0
  ret
.size __crt_sys_execve, . - __crt_sys_execve

.globl __crt_sys_uname
.type __crt_sys_uname, %function
.p2align 2
__crt_sys_uname:
  mov x8, #160
  svc #0
  ret
.size __crt_sys_uname, . - __crt_sys_uname

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
  mov x8, #94
  svc #0
1:
  b 1b
.size __crt_sys_exit, . - __crt_sys_exit

.globl __crt_sys_rt_sigaction
.type __crt_sys_rt_sigaction, %function
.p2align 2
__crt_sys_rt_sigaction:
  mov x8, #134
  svc #0
  ret
.size __crt_sys_rt_sigaction, . - __crt_sys_rt_sigaction

.globl __crt_sys_rt_sigprocmask
.type __crt_sys_rt_sigprocmask, %function
.p2align 2
__crt_sys_rt_sigprocmask:
  mov x8, #135
  svc #0
  ret
.size __crt_sys_rt_sigprocmask, . - __crt_sys_rt_sigprocmask
