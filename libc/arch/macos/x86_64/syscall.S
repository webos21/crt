.text

.globl ___crt_sys_read
.p2align 4, 0x90
___crt_sys_read:
  movq $0x2000003, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_write
.p2align 4, 0x90
___crt_sys_write:
  movq $0x2000004, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_pread
.p2align 4, 0x90
___crt_sys_pread:
  movq $0x2000099, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_pwrite
.p2align 4, 0x90
___crt_sys_pwrite:
  movq $0x200009a, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_fcntl
.p2align 4, 0x90
___crt_sys_fcntl:
  movq $0x200005c, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_ioctl
.p2align 4, 0x90
___crt_sys_ioctl:
  movq $0x2000036, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_macos_sysctl
.p2align 4, 0x90
___crt_sys_macos_sysctl:
  movq %rcx, %r10
  movq $0x20000ca, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_open
.p2align 4, 0x90
___crt_sys_open:
  movq $0x2000005, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_close
.p2align 4, 0x90
___crt_sys_close:
  movq $0x2000006, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_lseek
.p2align 4, 0x90
___crt_sys_lseek:
  movq $0x20000c7, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_ftruncate
.p2align 4, 0x90
___crt_sys_ftruncate:
  movq $0x20000c9, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_fsync
.p2align 4, 0x90
___crt_sys_fsync:
  movq $0x200005f, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_macos_statfs64
.p2align 4, 0x90
___crt_sys_macos_statfs64:
  movq $0x2000159, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_macos_fstatfs64
.p2align 4, 0x90
___crt_sys_macos_fstatfs64:
  movq $0x200015a, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_access
.p2align 4, 0x90
___crt_sys_access:
  movq $0x2000021, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_mkdir
.p2align 4, 0x90
___crt_sys_mkdir:
  movq $0x2000088, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_rmdir
.p2align 4, 0x90
___crt_sys_rmdir:
  movq $0x2000089, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_chdir
.p2align 4, 0x90
___crt_sys_chdir:
  movq $0x200000c, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_chmod
.p2align 4, 0x90
___crt_sys_chmod:
  movq $0x200000f, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_fchmod
.p2align 4, 0x90
___crt_sys_fchmod:
  movq $0x200007c, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_umask
.p2align 4, 0x90
___crt_sys_umask:
  movq $0x200003c, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_macos_fcntl
.p2align 4, 0x90
___crt_sys_macos_fcntl:
  movq $0x200005c, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_dup
.p2align 4, 0x90
___crt_sys_dup:
  movq $0x2000029, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_dup2
.p2align 4, 0x90
___crt_sys_dup2:
  movq $0x200005a, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_pipe
.p2align 4, 0x90
___crt_sys_pipe:
  movq %rdi, %r8
  movq $0x200002a, %rax
  syscall
  jc 1f
  movl %eax, (%r8)
  movl %edx, 4(%r8)
  xorl %eax, %eax
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_poll
.p2align 4, 0x90
___crt_sys_poll:
  movq $0x20000e6, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_socket
.p2align 4, 0x90
___crt_sys_socket:
  movq $0x2000061, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_connect
.p2align 4, 0x90
___crt_sys_connect:
  movq $0x2000062, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_accept
.p2align 4, 0x90
___crt_sys_accept:
  movq $0x200001e, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_getsockname
.p2align 4, 0x90
___crt_sys_getsockname:
  movq $0x2000020, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_bind
.p2align 4, 0x90
___crt_sys_bind:
  movq $0x2000068, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_setsockopt
.p2align 4, 0x90
___crt_sys_setsockopt:
  movq $0x2000069, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_listen
.p2align 4, 0x90
___crt_sys_listen:
  movq $0x200006a, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_sendto
.p2align 4, 0x90
___crt_sys_sendto:
  movq %rcx, %r10
  movq $0x2000085, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_shutdown
.p2align 4, 0x90
___crt_sys_shutdown:
  movq $0x2000086, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_recvfrom
.p2align 4, 0x90
___crt_sys_recvfrom:
  movq %rcx, %r10
  movq $0x200001d, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_readlink
.p2align 4, 0x90
___crt_sys_readlink:
  movq $0x200003a, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_symlink
.p2align 4, 0x90
___crt_sys_symlink:
  movq $0x2000039, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_getdents
.p2align 4, 0x90
___crt_sys_getdents:
  subq $8, %rsp
  leaq (%rsp), %rcx
  movq $0x2000158, %rax
  syscall
  addq $8, %rsp
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_macos_stat64
.p2align 4, 0x90
___crt_sys_macos_stat64:
  movq $0x2000152, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_macos_fstat64
.p2align 4, 0x90
___crt_sys_macos_fstat64:
  movq $0x2000153, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_macos_lstat64
.p2align 4, 0x90
___crt_sys_macos_lstat64:
  movq $0x2000154, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_unlink
.p2align 4, 0x90
___crt_sys_unlink:
  movq $0x200000a, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_rename
.p2align 4, 0x90
___crt_sys_rename:
  movq $0x2000080, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_mmap
.p2align 4, 0x90
___crt_sys_mmap:
  movq %rcx, %r10
  movq $0x20000c5, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_mprotect
.p2align 4, 0x90
___crt_sys_mprotect:
  movq $0x200004a, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_munmap
.p2align 4, 0x90
___crt_sys_munmap:
  movq $0x2000049, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_msync
.p2align 4, 0x90
___crt_sys_msync:
  movq $0x2000041, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_mremap
.p2align 4, 0x90
___crt_sys_mremap:
  movq $-38, %rax
  retq

.globl ___crt_sys_mlockall
.p2align 4, 0x90
___crt_sys_mlockall:
  movq $-38, %rax
  retq

.globl ___crt_sys_munlockall
.p2align 4, 0x90
___crt_sys_munlockall:
  movq $-38, %rax
  retq

.globl ___crt_sys_mlock
.p2align 4, 0x90
___crt_sys_mlock:
  movq $0x20000cb, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_mlock2
.p2align 4, 0x90
___crt_sys_mlock2:
  movq $-38, %rax
  retq

.globl ___crt_sys_munlock
.p2align 4, 0x90
___crt_sys_munlock:
  movq $0x20000cc, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_mincore
.p2align 4, 0x90
___crt_sys_mincore:
  movq $0x200004e, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_madvise
.p2align 4, 0x90
___crt_sys_madvise:
  movq $0x200004b, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_sleep_ms
.p2align 4, 0x90
___crt_sys_sleep_ms:
  movq %rdi, %rdx
  xorq %rdi, %rdi
  xorq %rsi, %rsi
  movq $0x20000e6, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_utimes
.p2align 4, 0x90
___crt_sys_utimes:
  movq $0x200008a, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_futimes
.p2align 4, 0x90
___crt_sys_futimes:
  movq $0x200008b, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_sched_yield
.p2align 4, 0x90
___crt_sys_sched_yield:
  xorq %rdi, %rdi
  xorq %rsi, %rsi
  xorq %rdx, %rdx
  movq $0x20000e6, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_thread_id
.p2align 4, 0x90
___crt_sys_thread_id:
  movq $0x2000174, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_getpid
.p2align 4, 0x90
___crt_sys_getpid:
  movq $0x2000014, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_getppid
.p2align 4, 0x90
___crt_sys_getppid:
  movq $0x2000027, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_setpgid
.p2align 4, 0x90
___crt_sys_setpgid:
  movq $0x2000052, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_getpgrp
.p2align 4, 0x90
___crt_sys_getpgrp:
  movq $0x2000051, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_setsid
.p2align 4, 0x90
___crt_sys_setsid:
  movq $0x2000093, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_fork
.p2align 4, 0x90
___crt_sys_fork:
  movq $0x2000002, %rax
  syscall
  jc 1f
  testq %rdx, %rdx
  jz 2f
  xorq %rax, %rax
2:
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_execve
.p2align 4, 0x90
___crt_sys_execve:
  movq $0x200003b, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_wait4
.p2align 4, 0x90
___crt_sys_wait4:
  movq %rcx, %r10
  movq $0x2000007, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_geteuid
.p2align 4, 0x90
___crt_sys_geteuid:
  movq $0x2000019, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_fchown
.p2align 4, 0x90
___crt_sys_fchown:
  movq $0x200007b, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_kill
.p2align 4, 0x90
___crt_sys_kill:
  movq $0x2000025, %rax
  syscall
  jc 1f
  retq
1:
  negq %rax
  retq

.globl ___crt_sys_thread_exit
.p2align 4, 0x90
___crt_sys_thread_exit:
  movq $0x2000001, %rax
  syscall
1:
  jmp 1b

.globl ___crt_sys_exit
.p2align 4, 0x90
___crt_sys_exit:
  movq $0x2000001, %rax
  syscall
1:
  jmp 1b
