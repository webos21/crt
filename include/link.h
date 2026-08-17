#ifndef CRT_LINK_H
#define CRT_LINK_H

/* dl_iterate_phdr() -- see docs/dynamic_loading.md and
 * docs/bionic_libc_gaps.md/HISTORY.md's 2026-08-17 entry.
 *
 * Matches real Bionic's own minimal struct dl_phdr_info (4 fields; the
 * dlpi_adds/dlpi_subs/dlpi_tls_modid/dlpi_tls_data fields glibc adds on top
 * are its own extension, not part of the POSIX/Bionic shape, so they're
 * intentionally not carried here). Real behavior only exists where this
 * project actually has ELF images to report:
 *
 * - Linux: reports exactly one entry, the main executable, built from the
 *   real AT_PHDR/AT_PHNUM values the kernel handed this process at exec()
 *   (see sys/auxv.h's getauxval()) -- accurate, not synthesized, but only
 *   one entry because this project has no real ELF dynamic linker yet
 *   (see docs/dynamic_loading.md's "Linux" section: dlopen() on Linux does
 *   not actually load shared objects today, so there is nothing else to
 *   report).
 * - macOS/Windows: dl_phdr_info's dlpi_phdr/dlpi_phnum fields are
 *   fundamentally ELF64_Phdr-shaped; Mach-O and PE have no such structure
 *   at all, and fabricating ELF-shaped data from a different real format
 *   would be actively wrong for any real caller that walks the phdr array
 *   expecting real ELF semantics. dl_iterate_phdr() on those hosts calls
 *   the callback zero times and returns 0 -- a legitimate, honest "no ELF
 *   images to report", not an error (dl_iterate_phdr() has no error
 *   return in its own contract; 0 is what a callback-never-invoked call
 *   already means on every host, including real Linux glibc when nothing
 *   matches a filter). */

#include <elf.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dl_phdr_info {
  Elf64_Addr dlpi_addr;
  const char* dlpi_name;
  const Elf64_Phdr* dlpi_phdr;
  Elf64_Half dlpi_phnum;
};

int dl_iterate_phdr(
    int (*callback)(struct dl_phdr_info* info, size_t size, void* data), void* data);

#ifdef __cplusplus
}
#endif

#endif
