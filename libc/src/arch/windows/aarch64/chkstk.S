.text

.globl __chkstk
.def __chkstk; .scl 2; .type 32; .endef
.p2align 2
__chkstk:
  lsl x16, x15, #4
  mov x17, sp
1:
  sub x17, x17, #4096
  subs x16, x16, #4096
  ldr xzr, [x17]
  b.gt 1b
  ret
