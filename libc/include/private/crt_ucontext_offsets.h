#ifndef CRT_PRIVATE_CRT_UCONTEXT_OFFSETS_H
#define CRT_PRIVATE_CRT_UCONTEXT_OFFSETS_H

/* Byte offsets, from the start of a Linux ucontext_t (include/ucontext.h), of the
 * slots getcontext()/setcontext()/swapcontext() and makecontext() use. Plain
 * #defines so the assembly (libc/src/arch/linux/<arch>/ucontext.S) can include
 * this file; libc/src/ucontext.c checks every one against offsetof() at compile
 * time, so a change to the structure cannot silently desynchronise them. */

#if defined(__x86_64__)
/* uc_mcontext starts at 40; gregs[i] is at 40 + 8 * i (REG_R12=4, REG_RBP=10,
 * REG_RBX=11, REG_RSP=15, REG_RIP=16). */
#define CRT_UC_RBX 128
#define CRT_UC_RBP 120
#define CRT_UC_R12 72
#define CRT_UC_R13 80
#define CRT_UC_R14 88
#define CRT_UC_R15 96
#define CRT_UC_RSP 160
#define CRT_UC_RIP 168
#if defined(CRT_TARGET_OS_WINDOWS)
/* Windows x64 also keeps rdi/rsi and xmm6-xmm15 callee-saved, and a context that gets its own stack
 * needs the TEB's NT_TIB.StackBase/StackLimit and DeallocationStack (ucontext.S, Windows): rdi and
 * rsi have their gregs slots (REG_RDI=8, REG_RSI=9), the xmm registers go to __fpregs_mem._xmm[6..15]
 * (a signal frame fills the FP state through fpregs, which is null in a saved context) and the
 * TEB fields to mcontext __reserved1[0..2]. */
#define CRT_UC_RDI 104
#define CRT_UC_RSI 112
#define CRT_UC_XMM6 560
#define CRT_UC_XMM7 576
#define CRT_UC_XMM8 592
#define CRT_UC_XMM9 608
#define CRT_UC_XMM10 624
#define CRT_UC_XMM11 640
#define CRT_UC_XMM12 656
#define CRT_UC_XMM13 672
#define CRT_UC_XMM14 688
#define CRT_UC_XMM15 704
#define CRT_UC_TEB_STACK_BASE 232
#define CRT_UC_TEB_STACK_LIMIT 240
#define CRT_UC_TEB_DEALLOCATION 248
#endif
#elif defined(__aarch64__)
/* uc_mcontext starts at 176: fault_address, then regs[0..30] from 184 (x_n at
 * 184 + 8 * n), sp at 432, pc at 440, pstate at 448, __reserved from 464. */
#define CRT_UC_X19 336
#define CRT_UC_X21 352
#define CRT_UC_X23 368
#define CRT_UC_X25 384
#define CRT_UC_X27 400
#define CRT_UC_X29 416
#define CRT_UC_X30 424
#define CRT_UC_SP 432
#define CRT_UC_PC 440
#define CRT_UC_D8 464
#endif

#endif
