.text

.globl ___crt_sys_write
.p2align 4, 0x90
___crt_sys_write:
  movq $0x2000004, %rax
  syscall
  retq

.globl ___crt_sys_exit
.p2align 4, 0x90
___crt_sys_exit:
  movq $0x2000001, %rax
  syscall
1:
  jmp 1b
