.text

.globl setjmp
.type setjmp, %function
.p2align 2
setjmp:
.globl _setjmp
.type _setjmp, %function
_setjmp:
  stp x19, x20, [x0, #0]
  stp x21, x22, [x0, #16]
  stp x23, x24, [x0, #32]
  stp x25, x26, [x0, #48]
  stp x27, x28, [x0, #64]
  stp x29, x30, [x0, #80]
  mov x2, sp
  str x2, [x0, #96]
  stp d8, d9, [x0, #104]
  stp d10, d11, [x0, #120]
  stp d12, d13, [x0, #136]
  stp d14, d15, [x0, #152]
  mov w0, #0
  ret
.size setjmp, . - setjmp
.size _setjmp, . - _setjmp

.globl longjmp
.type longjmp, %function
.p2align 2
longjmp:
.globl _longjmp
.type _longjmp, %function
_longjmp:
  mov x3, x0
  mov w2, w1
  cmp w2, #0
  csel w0, w2, wzr, ne
  cinc w0, w0, eq
  ldp x19, x20, [x3, #0]
  ldp x21, x22, [x3, #16]
  ldp x23, x24, [x3, #32]
  ldp x25, x26, [x3, #48]
  ldp x27, x28, [x3, #64]
  ldp x29, x30, [x3, #80]
  ldr x2, [x3, #96]
  mov sp, x2
  ldp d8, d9, [x3, #104]
  ldp d10, d11, [x3, #120]
  ldp d12, d13, [x3, #136]
  ldp d14, d15, [x3, #152]
  ret
.size longjmp, . - longjmp
.size _longjmp, . - _longjmp
