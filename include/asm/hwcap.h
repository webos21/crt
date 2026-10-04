#ifndef CRT_ASM_HWCAP_H
#define CRT_ASM_HWCAP_H

/* Mirrors the Linux kernel UAPI header arch/arm64/include/uapi/asm/hwcap.h (what Android Bionic
 * ships as <asm/hwcap.h> for aarch64): the bit numbers of getauxval(AT_HWCAP) and
 * getauxval(AT_HWCAP2) on AArch64. They are a stable kernel ABI. Added for Web Tranche 1, because
 * JavaScriptCore's ARM64 CPU feature detection (assembler/MacroAssemblerARM64.cpp) includes it on
 * Linux-shaped targets; on macOS getauxval() reports no entries (libc/src/arch/macos/common/auxv.c),
 * so every feature reads as absent there. Other architectures have no such header. */

#if defined(__aarch64__)

#define HWCAP_FP (1 << 0)
#define HWCAP_ASIMD (1 << 1)
#define HWCAP_EVTSTRM (1 << 2)
#define HWCAP_AES (1 << 3)
#define HWCAP_PMULL (1 << 4)
#define HWCAP_SHA1 (1 << 5)
#define HWCAP_SHA2 (1 << 6)
#define HWCAP_CRC32 (1 << 7)
#define HWCAP_ATOMICS (1 << 8)
#define HWCAP_FPHP (1 << 9)
#define HWCAP_ASIMDHP (1 << 10)
#define HWCAP_CPUID (1 << 11)
#define HWCAP_ASIMDRDM (1 << 12)
#define HWCAP_JSCVT (1 << 13)
#define HWCAP_FCMA (1 << 14)
#define HWCAP_LRCPC (1 << 15)
#define HWCAP_DCPOP (1 << 16)
#define HWCAP_SHA3 (1 << 17)
#define HWCAP_SM3 (1 << 18)
#define HWCAP_SM4 (1 << 19)
#define HWCAP_ASIMDDP (1 << 20)
#define HWCAP_SHA512 (1 << 21)
#define HWCAP_SVE (1 << 22)
#define HWCAP_ASIMDFHM (1 << 23)
#define HWCAP_DIT (1 << 24)
#define HWCAP_USCAT (1 << 25)
#define HWCAP_ILRCPC (1 << 26)
#define HWCAP_FLAGM (1 << 27)
#define HWCAP_SSBS (1 << 28)
#define HWCAP_SB (1 << 29)
#define HWCAP_PACA (1 << 30)
#define HWCAP_PACG (1UL << 31)

#define HWCAP2_DCPODP (1 << 0)
#define HWCAP2_SVE2 (1 << 1)
#define HWCAP2_SVEAES (1 << 2)
#define HWCAP2_SVEPMULL (1 << 3)
#define HWCAP2_SVEBITPERM (1 << 4)
#define HWCAP2_SVESHA3 (1 << 5)
#define HWCAP2_SVESM4 (1 << 6)
#define HWCAP2_FLAGM2 (1 << 7)
#define HWCAP2_FRINT (1 << 8)
#define HWCAP2_SVEI8MM (1 << 9)
#define HWCAP2_SVEF32MM (1 << 10)
#define HWCAP2_SVEF64MM (1 << 11)
#define HWCAP2_SVEBF16 (1 << 12)
#define HWCAP2_I8MM (1 << 13)
#define HWCAP2_BF16 (1 << 14)
#define HWCAP2_DGH (1 << 15)
#define HWCAP2_RNG (1 << 16)
#define HWCAP2_BTI (1 << 17)
#define HWCAP2_MTE (1 << 18)
#define HWCAP2_ECV (1 << 19)
#define HWCAP2_AFP (1 << 20)
#define HWCAP2_RPRES (1 << 21)
#define HWCAP2_MTE3 (1 << 22)
#define HWCAP2_SME (1 << 23)

#endif /* __aarch64__ */

#endif
