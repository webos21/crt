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

.globl ___crt_sys_exit
.p2align 4, 0x90
___crt_sys_exit:
  movq $0x2000001, %rax
  syscall
1:
  jmp 1b
