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

.globl ___crt_sys_gettimeofday
.p2align 4, 0x90
___crt_sys_gettimeofday:
  xorq %rsi, %rsi
  movq $0x2000074, %rax
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
