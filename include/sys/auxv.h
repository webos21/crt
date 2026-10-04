#ifndef CRT_SYS_AUXV_H
#define CRT_SYS_AUXV_H

#include <linux/auxvec.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the value of the given AT_* entry (see <linux/auxvec.h>) from the
 * ELF auxiliary vector the kernel passed this process at exec(), e.g.
 * getauxval(AT_HWCAP) for ARM/x86 runtime CPU feature detection. On
 * failure (the type is not present in the auxiliary vector) returns 0 and
 * sets errno to ENOENT, matching Android Bionic's getauxval(). Only Linux has
 * an auxiliary vector; on macOS no entry is ever present (always 0/ENOENT, see
 * libc/src/arch/macos/common/auxv.c). */
unsigned long getauxval(unsigned long type);

#ifdef __cplusplus
}
#endif

#endif
