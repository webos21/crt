.text

.globl __chkstk
.def __chkstk; .scl 2; .type 32; .endef
.p2align 4, 0x90
__chkstk:
  pushq %rcx
  pushq %rax
  leaq 24(%rsp), %rcx
  cmpq $4096, %rax
  jb 2f
1:
  subq $4096, %rcx
  testq %rax, (%rcx)
  subq $4096, %rax
  cmpq $4096, %rax
  ja 1b
2:
  subq %rax, %rcx
  testq %rax, (%rcx)
  popq %rax
  popq %rcx
  retq
