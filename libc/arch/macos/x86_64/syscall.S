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

.globl ___crt_sys_exit
.p2align 4, 0x90
___crt_sys_exit:
  movq $0x2000001, %rax
  syscall
1:
  jmp 1b
