#ifndef CRT_LINUX_AUXVEC_H
#define CRT_LINUX_AUXVEC_H

/* Mirrors the Linux kernel UAPI header of the same name
 * (include/uapi/linux/auxvec.h), aarch64/x86_64 values (neither
 * architecture's asm/auxvec.h adds anything beyond the generic set). Used
 * by getauxval() (libc/src/arch/linux/common/auxv.c) and, in turn, by
 * third-party code such as mbedtls's aesce.c ARMv8 crypto-extension
 * runtime detection via getauxval(AT_HWCAP)/getauxval(AT_HWCAP2). */

#define AT_NULL 0
#define AT_IGNORE 1
#define AT_EXECFD 2
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_FLAGS 8
#define AT_ENTRY 9
#define AT_NOTELF 10
#define AT_UID 11
#define AT_EUID 12
#define AT_GID 13
#define AT_EGID 14
#define AT_PLATFORM 15
#define AT_HWCAP 16
#define AT_CLKTCK 17
#define AT_SECURE 23
#define AT_BASE_PLATFORM 24
#define AT_RANDOM 25
#define AT_HWCAP2 26
#define AT_RSEQ_FEATURE_SIZE 27
#define AT_RSEQ_ALIGN 28
#define AT_HWCAP3 29
#define AT_HWCAP4 30
#define AT_EXECFN 31
#define AT_MINSIGSTKSZ 51

#endif
