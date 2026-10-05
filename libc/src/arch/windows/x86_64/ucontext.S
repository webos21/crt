/*
 * getcontext()/setcontext()/swapcontext() + the makecontext() trampoline,
 * Windows x86_64. Mirrors this same directory's setjmp.S register set
 * exactly (rbx/rbp/rdi/rsi/r12-r15 callee-saved GPRs + rsp + rip + the
 * Windows x64 ABI's callee-saved xmm6-xmm15).
 *
 * The context is the Linux/x86_64 (Bionic/kernel) ucontext_t, the same
 * structure CRT's signal delivery hands to an SA_SIGINFO handler on this
 * host (Web Tranche 1 Windows signal gate); include/ucontext.h has it and
 * private/crt_ucontext_offsets.h names every byte offset used here (ucontext.c
 * checks each against offsetof() at compile time). The register slots are the
 * signal frame's own (gregs[]); the ones a signal frame does not need keep
 * their own places:
 *   rbx, rbp, rdi, rsi, r12-r15, rsp, rip in their gregs[] slots
 *   xmm6..xmm15 in __fpregs_mem._xmm[6..15]
 *   TEB NT_TIB.StackBase / StackLimit / DeallocationStack in mcontext
 *   __reserved1[0..2]
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
 * swapcontext()'s SP slot is deliberately saved *unadjusted*
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
#define CRT_TARGET_OS_WINDOWS 1
#include <private/crt_ucontext_offsets.h>

.text

.globl getcontext
.def getcontext; .scl 2; .type 32; .endef
.p2align 4, 0x90
getcontext:
  movq %rbx, CRT_UC_RBX(%rcx)
  movq %rbp, CRT_UC_RBP(%rcx)
  movq %rdi, CRT_UC_RDI(%rcx)
  movq %rsi, CRT_UC_RSI(%rcx)
  movq %r12, CRT_UC_R12(%rcx)
  movq %r13, CRT_UC_R13(%rcx)
  movq %r14, CRT_UC_R14(%rcx)
  movq %r15, CRT_UC_R15(%rcx)
  leaq 8(%rsp), %rax
  movq %rax, CRT_UC_RSP(%rcx)
  movq (%rsp), %rax
  movq %rax, CRT_UC_RIP(%rcx)
  movdqu %xmm6, CRT_UC_XMM6(%rcx)
  movdqu %xmm7, CRT_UC_XMM7(%rcx)
  movdqu %xmm8, CRT_UC_XMM8(%rcx)
  movdqu %xmm9, CRT_UC_XMM9(%rcx)
  movdqu %xmm10, CRT_UC_XMM10(%rcx)
  movdqu %xmm11, CRT_UC_XMM11(%rcx)
  movdqu %xmm12, CRT_UC_XMM12(%rcx)
  movdqu %xmm13, CRT_UC_XMM13(%rcx)
  movdqu %xmm14, CRT_UC_XMM14(%rcx)
  movdqu %xmm15, CRT_UC_XMM15(%rcx)
  movq %gs:0x08, %rax
  movq %rax, CRT_UC_TEB_STACK_BASE(%rcx)
  movq %gs:0x10, %rax
  movq %rax, CRT_UC_TEB_STACK_LIMIT(%rcx)
  movq %gs:0x1478, %rax
  movq %rax, CRT_UC_TEB_DEALLOCATION(%rcx)
  xorl %eax, %eax
  retq

.globl setcontext
.def setcontext; .scl 2; .type 32; .endef
.p2align 4, 0x90
setcontext:
  movq CRT_UC_RBX(%rcx), %rbx
  movq CRT_UC_RBP(%rcx), %rbp
  movq CRT_UC_RDI(%rcx), %rdi
  movq CRT_UC_RSI(%rcx), %rsi
  movq CRT_UC_R12(%rcx), %r12
  movq CRT_UC_R13(%rcx), %r13
  movq CRT_UC_R14(%rcx), %r14
  movq CRT_UC_R15(%rcx), %r15
  movq CRT_UC_RSP(%rcx), %rsp
  movdqu CRT_UC_XMM6(%rcx), %xmm6
  movdqu CRT_UC_XMM7(%rcx), %xmm7
  movdqu CRT_UC_XMM8(%rcx), %xmm8
  movdqu CRT_UC_XMM9(%rcx), %xmm9
  movdqu CRT_UC_XMM10(%rcx), %xmm10
  movdqu CRT_UC_XMM11(%rcx), %xmm11
  movdqu CRT_UC_XMM12(%rcx), %xmm12
  movdqu CRT_UC_XMM13(%rcx), %xmm13
  movdqu CRT_UC_XMM14(%rcx), %xmm14
  movdqu CRT_UC_XMM15(%rcx), %xmm15
  movq CRT_UC_TEB_STACK_BASE(%rcx), %rax
  movq %rax, %gs:0x08
  movq CRT_UC_TEB_STACK_LIMIT(%rcx), %rax
  movq %rax, %gs:0x10
  movq CRT_UC_TEB_DEALLOCATION(%rcx), %rax
  movq %rax, %gs:0x1478
  jmp *CRT_UC_RIP(%rcx)

.globl swapcontext
.def swapcontext; .scl 2; .type 32; .endef
.p2align 4, 0x90
swapcontext:
  movq %rbx, CRT_UC_RBX(%rcx)
  movq %rbp, CRT_UC_RBP(%rcx)
  movq %rdi, CRT_UC_RDI(%rcx)
  movq %rsi, CRT_UC_RSI(%rcx)
  movq %r12, CRT_UC_R12(%rcx)
  movq %r13, CRT_UC_R13(%rcx)
  movq %r14, CRT_UC_R14(%rcx)
  movq %r15, CRT_UC_R15(%rcx)
  movq %rsp, CRT_UC_RSP(%rcx)
  movdqu %xmm6, CRT_UC_XMM6(%rcx)
  movdqu %xmm7, CRT_UC_XMM7(%rcx)
  movdqu %xmm8, CRT_UC_XMM8(%rcx)
  movdqu %xmm9, CRT_UC_XMM9(%rcx)
  movdqu %xmm10, CRT_UC_XMM10(%rcx)
  movdqu %xmm11, CRT_UC_XMM11(%rcx)
  movdqu %xmm12, CRT_UC_XMM12(%rcx)
  movdqu %xmm13, CRT_UC_XMM13(%rcx)
  movdqu %xmm14, CRT_UC_XMM14(%rcx)
  movdqu %xmm15, CRT_UC_XMM15(%rcx)
  movq %gs:0x08, %rax
  movq %rax, CRT_UC_TEB_STACK_BASE(%rcx)
  movq %gs:0x10, %rax
  movq %rax, CRT_UC_TEB_STACK_LIMIT(%rcx)
  movq %gs:0x1478, %rax
  movq %rax, CRT_UC_TEB_DEALLOCATION(%rcx)
  leaq 1f(%rip), %rax
  movq %rax, CRT_UC_RIP(%rcx)
  movq CRT_UC_RBX(%rdx), %rbx
  movq CRT_UC_RBP(%rdx), %rbp
  movq CRT_UC_RDI(%rdx), %rdi
  movq CRT_UC_RSI(%rdx), %rsi
  movq CRT_UC_R12(%rdx), %r12
  movq CRT_UC_R13(%rdx), %r13
  movq CRT_UC_R14(%rdx), %r14
  movq CRT_UC_R15(%rdx), %r15
  movq CRT_UC_RSP(%rdx), %rsp
  movdqu CRT_UC_XMM6(%rdx), %xmm6
  movdqu CRT_UC_XMM7(%rdx), %xmm7
  movdqu CRT_UC_XMM8(%rdx), %xmm8
  movdqu CRT_UC_XMM9(%rdx), %xmm9
  movdqu CRT_UC_XMM10(%rdx), %xmm10
  movdqu CRT_UC_XMM11(%rdx), %xmm11
  movdqu CRT_UC_XMM12(%rdx), %xmm12
  movdqu CRT_UC_XMM13(%rdx), %xmm13
  movdqu CRT_UC_XMM14(%rdx), %xmm14
  movdqu CRT_UC_XMM15(%rdx), %xmm15
  movq CRT_UC_TEB_STACK_BASE(%rdx), %rax
  movq %rax, %gs:0x08
  movq CRT_UC_TEB_STACK_LIMIT(%rdx), %rax
  movq %rax, %gs:0x10
  movq CRT_UC_TEB_DEALLOCATION(%rdx), %rax
  movq %rax, %gs:0x1478
  jmp *CRT_UC_RIP(%rdx)
1:
  xorl %eax, %eax
  retq

/* The makecontext() bootstrap frame (libc/src/ucontext.c, struct crt_makecontext_frame) is
 * addressed through %rbx: func at 0, argc 8, argv[0..3] at 16..40, link at 48. */
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
