/*
 * getcontext()/setcontext()/swapcontext() + the makecontext() trampoline,
 * Windows x86_64. Mirrors this same directory's setjmp.S register set
 * exactly (rbx/rbp/rdi/rsi/r12-r15 callee-saved GPRs + rsp + rip + the
 * Windows x64 ABI's callee-saved xmm6-xmm15). See include/ucontext.h for
 * the private mcontext_t byte-offset layout this file and ucontext.c
 * both rely on:
 *   0: rbx (doubles as the makecontext() bootstrap-frame pointer)
 *   8: rbp   16: rdi   24: rsi   32: r12   40: r13   48: r14   56: r15
 *   64: rsp  72: rip (resume address)
 *   80: xmm6  96: xmm7  112: xmm8  128: xmm9  144: xmm10  160: xmm11
 *   176: xmm12  192: xmm13  208: xmm14  224: xmm15
 *   240: TEB NT_TIB.StackBase   248: TEB NT_TIB.StackLimit
 *   256: TEB DeallocationStack
 *
 * The TEB fields are the one piece of state this file's Linux/macOS
 * siblings don't need at all: setjmp()/longjmp() and plain getcontext()/
 * setcontext() only ever resume a point further up the *same, already-
 * running* stack, so the OS never needs telling anything. makecontext()
 * introduces a genuinely new stack region (uc_stack) that a thread will
 * actually start executing on, and Windows -- unlike Linux/macOS, which
 * have no per-thread stack-bounds bookkeeping a plain memory buffer needs
 * to satisfy -- tracks each thread's current stack bounds in its TEB
 * (Thread Environment Block, pointed to by the %gs segment base on x64):
 * NT_TIB.StackBase at gs:0x08, NT_TIB.StackLimit at gs:0x10, and
 * DeallocationStack (outside NT_TIB proper) at gs:0x1478 -- a stable,
 * documented layout, unchanged since Windows NT. getcontext()/
 * setcontext()/swapcontext() below all save/restore these three fields
 * alongside the register set; makecontext() (ucontext.c) seeds them from
 * uc_stack for a context's first activation.
 *
 * swapcontext()'s SP slot (offset 64) is deliberately saved *unadjusted*
 * (plain %rsp, not %rsp+8 the way getcontext()/setcontext() save it):
 * its own resume point (the "1:" label below) completes via a real `retq`
 * -- which itself pops the return address and advances %rsp by 8 -- not
 * via a raw `jmp` to a separately-saved target address the way
 * getcontext()'s resume path works. Saving the +8-adjusted value here
 * too (as an earlier version of this file did) skips past the real
 * return address entirely, so `retq` at "1:" pops whatever unrelated
 * stack slot happens to follow it instead and jumps to garbage -- a
 * real bug caught by this project's own ucontext_test.c coroutine
 * round trip, which reliably crashed (STATUS_ACCESS_VIOLATION jumping to
 * address 0) inside exactly this path.
 */
.text

.globl getcontext
.def getcontext; .scl 2; .type 32; .endef
.p2align 4, 0x90
getcontext:
  movq %rbx, 0(%rcx)
  movq %rbp, 8(%rcx)
  movq %rdi, 16(%rcx)
  movq %rsi, 24(%rcx)
  movq %r12, 32(%rcx)
  movq %r13, 40(%rcx)
  movq %r14, 48(%rcx)
  movq %r15, 56(%rcx)
  leaq 8(%rsp), %rax
  movq %rax, 64(%rcx)
  movq (%rsp), %rax
  movq %rax, 72(%rcx)
  movdqu %xmm6, 80(%rcx)
  movdqu %xmm7, 96(%rcx)
  movdqu %xmm8, 112(%rcx)
  movdqu %xmm9, 128(%rcx)
  movdqu %xmm10, 144(%rcx)
  movdqu %xmm11, 160(%rcx)
  movdqu %xmm12, 176(%rcx)
  movdqu %xmm13, 192(%rcx)
  movdqu %xmm14, 208(%rcx)
  movdqu %xmm15, 224(%rcx)
  movq %gs:0x08, %rax
  movq %rax, 240(%rcx)
  movq %gs:0x10, %rax
  movq %rax, 248(%rcx)
  movq %gs:0x1478, %rax
  movq %rax, 256(%rcx)
  xorl %eax, %eax
  retq

.globl setcontext
.def setcontext; .scl 2; .type 32; .endef
.p2align 4, 0x90
setcontext:
  movq 0(%rcx), %rbx
  movq 8(%rcx), %rbp
  movq 16(%rcx), %rdi
  movq 24(%rcx), %rsi
  movq 32(%rcx), %r12
  movq 40(%rcx), %r13
  movq 48(%rcx), %r14
  movq 56(%rcx), %r15
  movq 64(%rcx), %rsp
  movdqu 80(%rcx), %xmm6
  movdqu 96(%rcx), %xmm7
  movdqu 112(%rcx), %xmm8
  movdqu 128(%rcx), %xmm9
  movdqu 144(%rcx), %xmm10
  movdqu 160(%rcx), %xmm11
  movdqu 176(%rcx), %xmm12
  movdqu 192(%rcx), %xmm13
  movdqu 208(%rcx), %xmm14
  movdqu 224(%rcx), %xmm15
  movq 240(%rcx), %rax
  movq %rax, %gs:0x08
  movq 248(%rcx), %rax
  movq %rax, %gs:0x10
  movq 256(%rcx), %rax
  movq %rax, %gs:0x1478
  jmp *72(%rcx)

.globl swapcontext
.def swapcontext; .scl 2; .type 32; .endef
.p2align 4, 0x90
swapcontext:
  movq %rbx, 0(%rcx)
  movq %rbp, 8(%rcx)
  movq %rdi, 16(%rcx)
  movq %rsi, 24(%rcx)
  movq %r12, 32(%rcx)
  movq %r13, 40(%rcx)
  movq %r14, 48(%rcx)
  movq %r15, 56(%rcx)
  movq %rsp, 64(%rcx)
  movdqu %xmm6, 80(%rcx)
  movdqu %xmm7, 96(%rcx)
  movdqu %xmm8, 112(%rcx)
  movdqu %xmm9, 128(%rcx)
  movdqu %xmm10, 144(%rcx)
  movdqu %xmm11, 160(%rcx)
  movdqu %xmm12, 176(%rcx)
  movdqu %xmm13, 192(%rcx)
  movdqu %xmm14, 208(%rcx)
  movdqu %xmm15, 224(%rcx)
  movq %gs:0x08, %rax
  movq %rax, 240(%rcx)
  movq %gs:0x10, %rax
  movq %rax, 248(%rcx)
  movq %gs:0x1478, %rax
  movq %rax, 256(%rcx)
  leaq 1f(%rip), %rax
  movq %rax, 72(%rcx)
  movq 0(%rdx), %rbx
  movq 8(%rdx), %rbp
  movq 16(%rdx), %rdi
  movq 24(%rdx), %rsi
  movq 32(%rdx), %r12
  movq 40(%rdx), %r13
  movq 48(%rdx), %r14
  movq 56(%rdx), %r15
  movq 64(%rdx), %rsp
  movdqu 80(%rdx), %xmm6
  movdqu 96(%rdx), %xmm7
  movdqu 112(%rdx), %xmm8
  movdqu 128(%rdx), %xmm9
  movdqu 144(%rdx), %xmm10
  movdqu 160(%rdx), %xmm11
  movdqu 176(%rdx), %xmm12
  movdqu 192(%rdx), %xmm13
  movdqu 208(%rdx), %xmm14
  movdqu 224(%rdx), %xmm15
  movq 240(%rdx), %rax
  movq %rax, %gs:0x08
  movq 248(%rdx), %rax
  movq %rax, %gs:0x10
  movq 256(%rdx), %rax
  movq %rax, %gs:0x1478
  jmp *72(%rdx)
1:
  xorl %eax, %eax
  retq

.globl __crt_makecontext_trampoline
.def __crt_makecontext_trampoline; .scl 2; .type 32; .endef
.p2align 4, 0x90
__crt_makecontext_trampoline:
  subq $32, %rsp
  movq 16(%rbx), %rcx
  movq 24(%rbx), %rdx
  movq 32(%rbx), %r8
  movq 40(%rbx), %r9
  call *0(%rbx)
  movq 48(%rbx), %rcx
  testq %rcx, %rcx
  jz 2f
  call setcontext
2:
  xorl %ecx, %ecx
  call exit
  ud2
