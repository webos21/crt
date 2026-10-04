#include <errno.h>

/* getauxval() on macOS: there is no ELF auxiliary vector, so no entry is ever present. This is
 * Bionic's documented answer for a missing entry (0 with errno ENOENT) and it makes callers such as
 * JavaScriptCore's ARM64 feature detection (AT_HWCAP/AT_HWCAP2) fall back to the baseline ISA.
 * A real mapping from `sysctl hw.optional.*` to HWCAP bits can replace this when a consumer needs
 * the optional instructions. See include/sys/auxv.h. */
unsigned long getauxval(unsigned long type) {
  (void)type;
  errno = ENOENT;
  return 0;
}
