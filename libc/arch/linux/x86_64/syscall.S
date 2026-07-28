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

.globl __crt_sys_munmap
.type __crt_sys_munmap, @function
.p2align 4, 0x90
__crt_sys_munmap:
  movq $11, %rax
  syscall
  retq
.size __crt_sys_munmap, . - __crt_sys_munmap

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

.globl __crt_sys_kill
.type __crt_sys_kill, @function
.p2align 4, 0x90
__crt_sys_kill:
  movq $62, %rax
  syscall
  retq
.size __crt_sys_kill, . - __crt_sys_kill

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
