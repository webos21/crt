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

.globl __crt_sys_fcntl
.type __crt_sys_fcntl, @function
.p2align 4, 0x90
__crt_sys_fcntl:
  movq $72, %rax
  syscall
  retq
.size __crt_sys_fcntl, . - __crt_sys_fcntl

.globl __crt_sys_ioctl
.type __crt_sys_ioctl, @function
.p2align 4, 0x90
__crt_sys_ioctl:
  movq $16, %rax
  syscall
  retq
.size __crt_sys_ioctl, . - __crt_sys_ioctl

.globl __crt_sys_pread
.type __crt_sys_pread, @function
.p2align 4, 0x90
__crt_sys_pread:
  /* pread64(fd, buf, count, offset) has 4 syscall args. The x86_64 SysV C
   * ABI passes the 4th integer argument (offset) in %rcx, but the
   * `syscall` instruction clobbers %rcx (it holds the return address for
   * SYSRET) and the kernel instead expects the 4th arg in %r10 -- so it
   * must be moved explicitly, exactly as every other 4+-arg syscall in
   * this file already does (mmap, sendto, statx, wait4, ...). Without
   * this, %r10 keeps whatever it held from the caller and the kernel
   * reads a garbage offset. */
  movq %rcx, %r10
  movq $17, %rax
  syscall
  retq
.size __crt_sys_pread, . - __crt_sys_pread

.globl __crt_sys_pwrite
.type __crt_sys_pwrite, @function
.p2align 4, 0x90
__crt_sys_pwrite:
  /* Same 4th-arg (offset) %rcx -> %r10 fixup as __crt_sys_pread above. */
  movq %rcx, %r10
  movq $18, %rax
  syscall
  retq
.size __crt_sys_pwrite, . - __crt_sys_pwrite

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

.globl __crt_sys_ftruncate
.type __crt_sys_ftruncate, @function
.p2align 4, 0x90
__crt_sys_ftruncate:
  movq $77, %rax
  syscall
  retq
.size __crt_sys_ftruncate, . - __crt_sys_ftruncate

.globl __crt_sys_fsync
.type __crt_sys_fsync, @function
.p2align 4, 0x90
__crt_sys_fsync:
  movq $74, %rax
  syscall
  retq
.size __crt_sys_fsync, . - __crt_sys_fsync

.globl __crt_sys_statfs
.type __crt_sys_statfs, @function
.p2align 4, 0x90
__crt_sys_statfs:
  movq $137, %rax
  syscall
  retq
.size __crt_sys_statfs, . - __crt_sys_statfs

.globl __crt_sys_fstatfs
.type __crt_sys_fstatfs, @function
.p2align 4, 0x90
__crt_sys_fstatfs:
  movq $138, %rax
  syscall
  retq
.size __crt_sys_fstatfs, . - __crt_sys_fstatfs

.globl __crt_sys_access
.type __crt_sys_access, @function
.p2align 4, 0x90
__crt_sys_access:
  movq $21, %rax
  syscall
  retq
.size __crt_sys_access, . - __crt_sys_access

.globl __crt_sys_mkdir
.type __crt_sys_mkdir, @function
.p2align 4, 0x90
__crt_sys_mkdir:
  movq $83, %rax
  syscall
  retq
.size __crt_sys_mkdir, . - __crt_sys_mkdir

.globl __crt_sys_rmdir
.type __crt_sys_rmdir, @function
.p2align 4, 0x90
__crt_sys_rmdir:
  movq $84, %rax
  syscall
  retq
.size __crt_sys_rmdir, . - __crt_sys_rmdir

.globl __crt_sys_chdir
.type __crt_sys_chdir, @function
.p2align 4, 0x90
__crt_sys_chdir:
  movq $80, %rax
  syscall
  retq
.size __crt_sys_chdir, . - __crt_sys_chdir

.globl __crt_sys_fchdir
.type __crt_sys_fchdir, @function
.p2align 4, 0x90
__crt_sys_fchdir:
  movq $81, %rax
  syscall
  retq
.size __crt_sys_fchdir, . - __crt_sys_fchdir

.globl __crt_sys_chmod
.type __crt_sys_chmod, @function
.p2align 4, 0x90
__crt_sys_chmod:
  movq $90, %rax
  syscall
  retq
.size __crt_sys_chmod, . - __crt_sys_chmod

.globl __crt_sys_fchmod
.type __crt_sys_fchmod, @function
.p2align 4, 0x90
__crt_sys_fchmod:
  movq $91, %rax
  syscall
  retq
.size __crt_sys_fchmod, . - __crt_sys_fchmod

.globl __crt_sys_umask
.type __crt_sys_umask, @function
.p2align 4, 0x90
__crt_sys_umask:
  movq $95, %rax
  syscall
  retq
.size __crt_sys_umask, . - __crt_sys_umask

.globl __crt_sys_getcwd
.type __crt_sys_getcwd, @function
.p2align 4, 0x90
__crt_sys_getcwd:
  movq $79, %rax
  syscall
  retq
.size __crt_sys_getcwd, . - __crt_sys_getcwd

.globl __crt_sys_dup
.type __crt_sys_dup, @function
.p2align 4, 0x90
__crt_sys_dup:
  movq $32, %rax
  syscall
  retq
.size __crt_sys_dup, . - __crt_sys_dup

.globl __crt_sys_dup2
.type __crt_sys_dup2, @function
.p2align 4, 0x90
__crt_sys_dup2:
  movq $33, %rax
  syscall
  retq
.size __crt_sys_dup2, . - __crt_sys_dup2

.globl __crt_sys_pipe
.type __crt_sys_pipe, @function
.p2align 4, 0x90
__crt_sys_pipe:
  xorq %rsi, %rsi
  movq $293, %rax
  syscall
  retq
.size __crt_sys_pipe, . - __crt_sys_pipe

.globl __crt_sys_ppoll
.type __crt_sys_ppoll, @function
.p2align 4, 0x90
__crt_sys_ppoll:
  xorq %r10, %r10
  xorq %r8, %r8
  movq $271, %rax
  syscall
  retq
.size __crt_sys_ppoll, . - __crt_sys_ppoll

.globl __crt_sys_socket
.type __crt_sys_socket, @function
.p2align 4, 0x90
__crt_sys_socket:
  movq $41, %rax
  syscall
  retq
.size __crt_sys_socket, . - __crt_sys_socket

.globl __crt_sys_connect
.type __crt_sys_connect, @function
.p2align 4, 0x90
__crt_sys_connect:
  movq $42, %rax
  syscall
  retq
.size __crt_sys_connect, . - __crt_sys_connect

.globl __crt_sys_accept
.type __crt_sys_accept, @function
.p2align 4, 0x90
__crt_sys_accept:
  xorq %r10, %r10
  movq $288, %rax
  syscall
  retq
.size __crt_sys_accept, . - __crt_sys_accept

.globl __crt_sys_sendto
.type __crt_sys_sendto, @function
.p2align 4, 0x90
__crt_sys_sendto:
  movq %rcx, %r10
  movq $44, %rax
  syscall
  retq
.size __crt_sys_sendto, . - __crt_sys_sendto

.globl __crt_sys_recvfrom
.type __crt_sys_recvfrom, @function
.p2align 4, 0x90
__crt_sys_recvfrom:
  movq %rcx, %r10
  movq $45, %rax
  syscall
  retq
.size __crt_sys_recvfrom, . - __crt_sys_recvfrom

/* sendmsg(int sockfd, const struct msghdr *msg, int flags) -- 3 args, no
 * r10 shuffle needed (unlike sendto/recvfrom above, which have 6 and hit
 * the syscall convention's rcx-clobbered-by-the-syscall-instruction case).
 * Syscall numbers 46/47 per arch/x86/entry/syscalls/syscall_64.tbl, right
 * after sendto=44/recvfrom=45 above. NOT independently verified on real
 * Linux hardware from this session (Windows-only dev environment) -- see
 * HISTORY.md's 2026-08-16 entry, matching the same caveat linkat()'s own
 * trampolines carried until real hardware verification closed it. */
.globl __crt_sys_sendmsg
.type __crt_sys_sendmsg, @function
.p2align 4, 0x90
__crt_sys_sendmsg:
  movq $46, %rax
  syscall
  retq
.size __crt_sys_sendmsg, . - __crt_sys_sendmsg

.globl __crt_sys_recvmsg
.type __crt_sys_recvmsg, @function
.p2align 4, 0x90
__crt_sys_recvmsg:
  movq $47, %rax
  syscall
  retq
.size __crt_sys_recvmsg, . - __crt_sys_recvmsg

.globl __crt_sys_bind
.type __crt_sys_bind, @function
.p2align 4, 0x90
__crt_sys_bind:
  movq $49, %rax
  syscall
  retq
.size __crt_sys_bind, . - __crt_sys_bind

.globl __crt_sys_listen
.type __crt_sys_listen, @function
.p2align 4, 0x90
__crt_sys_listen:
  movq $50, %rax
  syscall
  retq
.size __crt_sys_listen, . - __crt_sys_listen

.globl __crt_sys_getsockname
.type __crt_sys_getsockname, @function
.p2align 4, 0x90
__crt_sys_getsockname:
  movq $51, %rax
  syscall
  retq
.size __crt_sys_getsockname, . - __crt_sys_getsockname

.globl __crt_sys_setsockopt
.type __crt_sys_setsockopt, @function
.p2align 4, 0x90
__crt_sys_setsockopt:
  movq %rcx, %r10
  movq $54, %rax
  syscall
  retq
.size __crt_sys_setsockopt, . - __crt_sys_setsockopt

.globl __crt_sys_getsockopt
.type __crt_sys_getsockopt, @function
.p2align 4, 0x90
__crt_sys_getsockopt:
  movq %rcx, %r10
  movq $55, %rax
  syscall
  retq
.size __crt_sys_getsockopt, . - __crt_sys_getsockopt

.globl __crt_sys_shutdown
.type __crt_sys_shutdown, @function
.p2align 4, 0x90
__crt_sys_shutdown:
  movq $48, %rax
  syscall
  retq
.size __crt_sys_shutdown, . - __crt_sys_shutdown

.globl __crt_sys_readlink
.type __crt_sys_readlink, @function
.p2align 4, 0x90
__crt_sys_readlink:
  movq %rdx, %rcx
  movq %rsi, %rdx
  movq %rdi, %rsi
  movq $-100, %rdi
  movq %rcx, %r10
  movq $267, %rax
  syscall
  retq
.size __crt_sys_readlink, . - __crt_sys_readlink

.globl __crt_sys_symlink
.type __crt_sys_symlink, @function
.p2align 4, 0x90
__crt_sys_symlink:
  movq %rsi, %rdx
  movq $-100, %rsi
  movq $266, %rax
  syscall
  retq
.size __crt_sys_symlink, . - __crt_sys_symlink

.globl __crt_sys_getdents
.type __crt_sys_getdents, @function
.p2align 4, 0x90
__crt_sys_getdents:
  movq $217, %rax
  syscall
  retq
.size __crt_sys_getdents, . - __crt_sys_getdents

.globl __crt_sys_statx
.type __crt_sys_statx, @function
.p2align 4, 0x90
__crt_sys_statx:
  movq %rcx, %r10
  movq $332, %rax
  syscall
  retq
.size __crt_sys_statx, . - __crt_sys_statx

.globl __crt_sys_unlink
.type __crt_sys_unlink, @function
.p2align 4, 0x90
__crt_sys_unlink:
  movq $87, %rax
  syscall
  retq
.size __crt_sys_unlink, . - __crt_sys_unlink

/* link(oldpath, newpath): x86_64 still has the legacy 2-arg __NR_link (86),
 * right next to __NR_unlink (87) above -- no *at() shuffle needed, unlike
 * aarch64 below (see that file's own comment). __crt_sys_link(oldpath,
 * newpath) already matches link(2)'s own (rdi, rsi) argument order. */
.globl __crt_sys_link
.type __crt_sys_link, @function
.p2align 4, 0x90
__crt_sys_link:
  movq $86, %rax
  syscall
  retq
.size __crt_sys_link, . - __crt_sys_link

.globl __crt_sys_rename
.type __crt_sys_rename, @function
.p2align 4, 0x90
__crt_sys_rename:
  movq $82, %rax
  syscall
  retq
.size __crt_sys_rename, . - __crt_sys_rename

.globl __crt_sys_mmap
.type __crt_sys_mmap, @function
.p2align 4, 0x90
__crt_sys_mmap:
  movq %rcx, %r10
  movq $9, %rax
  syscall
  retq
.size __crt_sys_mmap, . - __crt_sys_mmap

.globl __crt_sys_mprotect
.type __crt_sys_mprotect, @function
.p2align 4, 0x90
__crt_sys_mprotect:
  movq $10, %rax
  syscall
  retq
.size __crt_sys_mprotect, . - __crt_sys_mprotect

.globl __crt_sys_munmap
.type __crt_sys_munmap, @function
.p2align 4, 0x90
__crt_sys_munmap:
  movq $11, %rax
  syscall
  retq
.size __crt_sys_munmap, . - __crt_sys_munmap

.globl __crt_sys_msync
.type __crt_sys_msync, @function
.p2align 4, 0x90
__crt_sys_msync:
  movq $26, %rax
  syscall
  retq
.size __crt_sys_msync, . - __crt_sys_msync

.globl __crt_sys_mremap
.type __crt_sys_mremap, @function
.p2align 4, 0x90
__crt_sys_mremap:
  movq %rcx, %r10
  movq $25, %rax
  syscall
  retq
.size __crt_sys_mremap, . - __crt_sys_mremap

.globl __crt_sys_mlockall
.type __crt_sys_mlockall, @function
.p2align 4, 0x90
__crt_sys_mlockall:
  movq $151, %rax
  syscall
  retq
.size __crt_sys_mlockall, . - __crt_sys_mlockall

.globl __crt_sys_munlockall
.type __crt_sys_munlockall, @function
.p2align 4, 0x90
__crt_sys_munlockall:
  movq $152, %rax
  syscall
  retq
.size __crt_sys_munlockall, . - __crt_sys_munlockall

.globl __crt_sys_mlock
.type __crt_sys_mlock, @function
.p2align 4, 0x90
__crt_sys_mlock:
  movq $149, %rax
  syscall
  retq
.size __crt_sys_mlock, . - __crt_sys_mlock

.globl __crt_sys_mlock2
.type __crt_sys_mlock2, @function
.p2align 4, 0x90
__crt_sys_mlock2:
  movq $325, %rax
  syscall
  retq
.size __crt_sys_mlock2, . - __crt_sys_mlock2

.globl __crt_sys_munlock
.type __crt_sys_munlock, @function
.p2align 4, 0x90
__crt_sys_munlock:
  movq $150, %rax
  syscall
  retq
.size __crt_sys_munlock, . - __crt_sys_munlock

.globl __crt_sys_mincore
.type __crt_sys_mincore, @function
.p2align 4, 0x90
__crt_sys_mincore:
  movq $27, %rax
  syscall
  retq
.size __crt_sys_mincore, . - __crt_sys_mincore

.globl __crt_sys_madvise
.type __crt_sys_madvise, @function
.p2align 4, 0x90
__crt_sys_madvise:
  movq $28, %rax
  syscall
  retq
.size __crt_sys_madvise, . - __crt_sys_madvise

.globl __crt_sys_gettimeofday
.type __crt_sys_gettimeofday, @function
.p2align 4, 0x90
__crt_sys_gettimeofday:
  xorq %rsi, %rsi
  movq $96, %rax
  syscall
  retq
.size __crt_sys_gettimeofday, . - __crt_sys_gettimeofday

.globl __crt_sys_clock_gettime
.type __crt_sys_clock_gettime, @function
.p2align 4, 0x90
__crt_sys_clock_gettime:
  movq $228, %rax
  syscall
  retq
.size __crt_sys_clock_gettime, . - __crt_sys_clock_gettime

.globl __crt_sys_nanosleep
.type __crt_sys_nanosleep, @function
.p2align 4, 0x90
__crt_sys_nanosleep:
  movq $35, %rax
  syscall
  retq
.size __crt_sys_nanosleep, . - __crt_sys_nanosleep

.globl __crt_sys_utimensat
.type __crt_sys_utimensat, @function
.p2align 4, 0x90
__crt_sys_utimensat:
  movq %rcx, %r10
  movq $280, %rax
  syscall
  retq
.size __crt_sys_utimensat, . - __crt_sys_utimensat

.globl __crt_sys_sched_yield
.type __crt_sys_sched_yield, @function
.p2align 4, 0x90
__crt_sys_sched_yield:
  movq $24, %rax
  syscall
  retq
.size __crt_sys_sched_yield, . - __crt_sys_sched_yield

.globl __crt_sys_thread_id
.type __crt_sys_thread_id, @function
.p2align 4, 0x90
__crt_sys_thread_id:
  movq $186, %rax
  syscall
  retq
.size __crt_sys_thread_id, . - __crt_sys_thread_id

.globl __crt_sys_clone_thread
.type __crt_sys_clone_thread, @function
.p2align 4, 0x90
__crt_sys_clone_thread:
  pushq %r12
  pushq %r13
  pushq %r14
  movq %rdi, %r11
  movq %rsi, %r12
  movq %rdx, %r13
  movq %rcx, %rdi
  movq %r11, %rsi
  movq %r8, %rdx
  movq %r9, %r10
  movq 32(%rsp), %r8
  movq $56, %rax
  syscall
  testq %rax, %rax
  je 1f
  popq %r14
  popq %r13
  popq %r12
  retq
1:
  movq %r13, %rdi
  callq *%r12
  movq %rax, %rdi
  movq $60, %rax
  syscall
2:
  jmp 2b
.size __crt_sys_clone_thread, . - __crt_sys_clone_thread

.globl __crt_sys_wait4
.type __crt_sys_wait4, @function
.p2align 4, 0x90
__crt_sys_wait4:
  movq %rcx, %r10
  movq $61, %rax
  syscall
  retq
.size __crt_sys_wait4, . - __crt_sys_wait4

.globl __crt_sys_fork
.type __crt_sys_fork, @function
.p2align 4, 0x90
__crt_sys_fork:
  movq $57, %rax
  syscall
  retq
.size __crt_sys_fork, . - __crt_sys_fork

.globl __crt_sys_futex
.type __crt_sys_futex, @function
.p2align 4, 0x90
__crt_sys_futex:
  movq %rcx, %r10
  movq $202, %rax
  syscall
  retq
.size __crt_sys_futex, . - __crt_sys_futex

.globl __crt_sys_getpid
.type __crt_sys_getpid, @function
.p2align 4, 0x90
__crt_sys_getpid:
  movq $39, %rax
  syscall
  retq
.size __crt_sys_getpid, . - __crt_sys_getpid

.globl __crt_sys_getppid
.type __crt_sys_getppid, @function
.p2align 4, 0x90
__crt_sys_getppid:
  movq $110, %rax
  syscall
  retq
.size __crt_sys_getppid, . - __crt_sys_getppid

.globl __crt_sys_setpgid
.type __crt_sys_setpgid, @function
.p2align 4, 0x90
__crt_sys_setpgid:
  movq $109, %rax
  syscall
  retq
.size __crt_sys_setpgid, . - __crt_sys_setpgid

.globl __crt_sys_getpgrp
.type __crt_sys_getpgrp, @function
.p2align 4, 0x90
__crt_sys_getpgrp:
  xorq %rdi, %rdi
  movq $121, %rax
  syscall
  retq
.size __crt_sys_getpgrp, . - __crt_sys_getpgrp

.globl __crt_sys_setsid
.type __crt_sys_setsid, @function
.p2align 4, 0x90
__crt_sys_setsid:
  movq $112, %rax
  syscall
  retq
.size __crt_sys_setsid, . - __crt_sys_setsid

.globl __crt_sys_geteuid
.type __crt_sys_geteuid, @function
.p2align 4, 0x90
__crt_sys_geteuid:
  movq $107, %rax
  syscall
  retq
.size __crt_sys_geteuid, . - __crt_sys_geteuid

.globl __crt_sys_fchown
.type __crt_sys_fchown, @function
.p2align 4, 0x90
__crt_sys_fchown:
  movq $93, %rax
  syscall
  retq
.size __crt_sys_fchown, . - __crt_sys_fchown

.globl __crt_sys_kill
.type __crt_sys_kill, @function
.p2align 4, 0x90
__crt_sys_kill:
  movq $62, %rax
  syscall
  retq
.size __crt_sys_kill, . - __crt_sys_kill

.globl __crt_sys_execve
.type __crt_sys_execve, @function
.p2align 4, 0x90
__crt_sys_execve:
  movq $59, %rax
  syscall
  retq
.size __crt_sys_execve, . - __crt_sys_execve

.globl __crt_sys_uname
.type __crt_sys_uname, @function
.p2align 4, 0x90
__crt_sys_uname:
  movq $63, %rax
  syscall
  retq
.size __crt_sys_uname, . - __crt_sys_uname

.globl __crt_sys_thread_exit
.type __crt_sys_thread_exit, @function
.p2align 4, 0x90
__crt_sys_thread_exit:
  movq $60, %rax
  syscall
1:
  jmp 1b
.size __crt_sys_thread_exit, . - __crt_sys_thread_exit

.globl __crt_sys_exit
.type __crt_sys_exit, @function
.p2align 4, 0x90
__crt_sys_exit:
  movq $231, %rax
  syscall
1:
  jmp 1b
.size __crt_sys_exit, . - __crt_sys_exit

.globl __crt_sys_rt_sigaction
.type __crt_sys_rt_sigaction, @function
.p2align 4, 0x90
__crt_sys_rt_sigaction:
  movq %rcx, %r10
  movq $13, %rax
  syscall
  retq
.size __crt_sys_rt_sigaction, . - __crt_sys_rt_sigaction

.globl __crt_sys_rt_sigprocmask
.type __crt_sys_rt_sigprocmask, @function
.p2align 4, 0x90
__crt_sys_rt_sigprocmask:
  movq %rcx, %r10
  movq $14, %rax
  syscall
  retq
.size __crt_sys_rt_sigprocmask, . - __crt_sys_rt_sigprocmask

/* Installed as sa_restorer for every real signal this backend hands to the
 * raw rt_sigaction(2) syscall (see libc/src/arch/linux/common/
 * signal_backend.c). x86_64's rt_sigaction requires SA_RESTORER plus a real,
 * executable return address: the kernel jumps here after running the signal
 * handler, with a signal frame already built on the stack, and
 * rt_sigreturn(2) never returns normally -- so there is no `retq` here.
 * aarch64 needs no equivalent; the kernel supplies its own default restorer
 * from the vDSO (see Android Bionic's arch-arm64 sigaction.cpp comment).
 * Stripped of Bionic's CFI/unwind directives, which are debugger-quality-only
 * and not functionally required. */
.globl __crt_signal_restore_rt
.type __crt_signal_restore_rt, @function
.p2align 4, 0x90
__crt_signal_restore_rt:
  movq $15, %rax
  syscall
.size __crt_signal_restore_rt, . - __crt_signal_restore_rt
