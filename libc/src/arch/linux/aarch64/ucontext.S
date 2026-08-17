/*
 * getcontext()/setcontext()/swapcontext() + the makecontext() trampoline,
 * Linux aarch64 (AAPCS64). Mirrors this same directory's setjmp.S
 * register set exactly (x19-x28 callee-saved GPRs, x29/x30, sp, and the
 * AAPCS64-mandated callee-saved low halves d8-d15 of v8-v15). See
 * include/ucontext.h for the private mcontext_t byte-offset layout this
 * file and ucontext.c both rely on:
 *   0: x19 (doubles as the makecontext() bootstrap-frame pointer)
 *   8: x20  16: x21  24: x22  32: x23  40: x24  48: x25  56: x26
 *   64: x27  72: x28  80: x29 (fp)  88: x30 (lr / resume address)
 *   96: sp
 *   104: d8  112: d9  120: d10  128: d11  136: d12  144: d13  152: d14  160: d15
 *
 * makecontext() bootstrap frame layout (ucontext.c), read by the
 * trampoline below via x19 (which setcontext()/swapcontext() will have
 * just restored from mcontext offset 0 on a makecontext()-created
 * context's first activation):
 *   0: func   8: argc (unused here)   16..40: argv[0..3]   48: link
 *
 * swapcontext() used to patch the saved x30 slot (offset 88) with the
 * address of a local "1:" resume stub (`adr x2, 1f; str x2, [x0, #88]`)
 * instead of leaving the real caller's return address there, so that
 * resuming this context could zero w0 (POSIX: a saved context that gets
 * resumed appears as if the call that saved it had just returned 0)
 * before returning to the caller. That's the x86_64 sibling's own `1:`
 * trick (see linux/x86_64/ucontext.S), where it works because `retq`
 * pops the return address off the *stack* -- but AAPCS64's `ret` just
 * branches to whatever is in the x30 *register*, which does not get
 * cleared or advanced by executing it. So the stub's own `ret` at the
 * end still had x30 == the stub's own address (nothing between the jump
 * into the stub and its `ret` ever changed x30), branching straight back
 * to itself: an unconditional infinite loop, 100% CPU, no crash, no
 * output -- reproduced live on real macOS aarch64 hardware
 * (tests/ucontext_test.c hanging forever at exactly this instruction
 * pair, confirmed by attaching a debugger to the spinning process; see
 * HISTORY.md). Not something the reasoning in the original commit
 * caught, since it was never run on real aarch64 hardware.
 *
 * Fixed by not touching the x30 slot at all -- `stp x29, x30, [x0, #80]`
 * a few lines up already saves the real caller's return address there,
 * exactly like getcontext() does, which is all a plain `ret` needs to
 * resume at the right place. `mov w0, #0` is done immediately before
 * every place this file jumps *into* a saved context (setcontext()'s own
 * `ret`, and swapcontext()'s second `ret` after loading the new
 * context) instead, so the "resumed as if just returned 0" contract
 * still holds without needing a separate resume stub or any of the
 * infinite-loop hazard that came with it.
 */
.text

.globl getcontext
.type getcontext, %function
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
.size getcontext, . - getcontext

.globl setcontext
.type setcontext, %function
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
.size setcontext, . - setcontext

.globl swapcontext
.type swapcontext, %function
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
.size swapcontext, . - swapcontext

.globl __crt_makecontext_trampoline
.type __crt_makecontext_trampoline, %function
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
.size __crt_makecontext_trampoline, . - __crt_makecontext_trampoline
