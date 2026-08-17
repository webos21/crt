/*
 * getcontext()/setcontext()/swapcontext() + the makecontext() trampoline,
 * Windows aarch64 (AAPCS64 -- identical register-save shape to the Linux
 * aarch64 variant of this same file; ARM64 Windows follows the same
 * AAPCS64-derived convention as Linux/macOS aarch64, unlike x86_64 where
 * Windows and SysV differ). See linux/aarch64/ucontext.S's header
 * comment for the full mcontext_t/bootstrap-frame byte-offset layout --
 * identical here, including the swapcontext() resume-stub infinite-loop
 * bug/fix that same comment documents in full (found live on real macOS
 * aarch64 hardware, reasoned to apply identically here since AAPCS64's
 * `ret`-via-link-register mechanism is the same on both, not yet
 * independently re-verified on real Windows aarch64 hardware -- see
 * HISTORY.md). makecontext() caps argc at 4 uniformly across every host
 * (see ucontext.h's own comment), matching the Windows x86_64 ABI's
 * 4-register limit even though AAPCS64 itself has 8 argument registers.
 */
.text

.globl getcontext
.def getcontext; .scl 2; .type 32; .endef
.p2align 2
getcontext:
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

.globl setcontext
.def setcontext; .scl 2; .type 32; .endef
.p2align 2
setcontext:
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
  mov w0, #0
  ret

.globl swapcontext
.def swapcontext; .scl 2; .type 32; .endef
.p2align 2
swapcontext:
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
  mov w0, #0
  ret

.globl __crt_makecontext_trampoline
.def __crt_makecontext_trampoline; .scl 2; .type 32; .endef
.p2align 2
__crt_makecontext_trampoline:
  ldr x0, [x19, #16]
  ldr x1, [x19, #24]
  ldr x2, [x19, #32]
  ldr x3, [x19, #40]
  ldr x4, [x19, #0]
  blr x4
  ldr x0, [x19, #48]
  cbz x0, 2f
  bl setcontext
2:
  mov x0, #0
  bl exit
  brk #0
