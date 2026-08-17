/*
 * getcontext()/setcontext()/swapcontext() + the makecontext() trampoline,
 * macOS aarch64 (AAPCS64 -- identical register-save shape to the Linux
 * aarch64 variant of this same file, only the symbol-naming convention
 * differs, exactly like this directory's own setjmp.S vs the Linux one).
 * See linux/aarch64/ucontext.S's header comment for the full mcontext_t/
 * bootstrap-frame byte-offset layout -- identical here.
 */
.text

.globl _getcontext
.p2align 2
_getcontext:
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

.globl _setcontext
.p2align 2
_setcontext:
  ldp x19, x20, [x0, #0]
  ldp x21, x22, [x0, #16]
  ldp x23, x24, [x0, #32]
  ldp x25, x26, [x0, #48]
  ldp x27, x28, [x0, #64]
  ldp x29, x30, [x0, #80]
  ldr x2, [x0, #96]
  mov sp, x2
  ldp d8, d9, [x0, #104]
  ldp d10, d11, [x0, #120]
  ldp d12, d13, [x0, #136]
  ldp d14, d15, [x0, #152]
  ret

.globl _swapcontext
.p2align 2
_swapcontext:
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
  adr x2, 1f
  str x2, [x0, #88]
  ldp x19, x20, [x1, #0]
  ldp x21, x22, [x1, #16]
  ldp x23, x24, [x1, #32]
  ldp x25, x26, [x1, #48]
  ldp x27, x28, [x1, #64]
  ldp x29, x30, [x1, #80]
  ldr x2, [x1, #96]
  mov sp, x2
  ldp d8, d9, [x1, #104]
  ldp d10, d11, [x1, #120]
  ldp d12, d13, [x1, #136]
  ldp d14, d15, [x1, #152]
  ret
1:
  mov w0, #0
  ret

.globl ___crt_makecontext_trampoline
.p2align 2
___crt_makecontext_trampoline:
  ldr x0, [x19, #16]
  ldr x1, [x19, #24]
  ldr x2, [x19, #32]
  ldr x3, [x19, #40]
  ldr x4, [x19, #0]
  blr x4
  ldr x0, [x19, #48]
  cbz x0, 2f
  bl _setcontext
2:
  mov x0, #0
  bl _exit
  brk #0
