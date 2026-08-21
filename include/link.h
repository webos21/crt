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
 * - Linux: reports the main executable, built from the real AT_PHDR/
 *   AT_PHNUM values the kernel handed this process at exec() (see
 *   sys/auxv.h's getauxval()), plus every other real shared object
 *   currently mapped -- found by walking the real system dynamic
 *   linker's own struct r_debug/link_map rendezvous list (see
 *   libdl/src/arch/linux/dl_linux.c's own comment), not by this project
 *   loading anything itself: dlopen() on Linux still does not actually
 *   load new shared objects today (see docs/dynamic_loading.md's "Linux"
 *   section), but every Linux executable/DSO this project builds is
 *   *linked* against the real system dynamic linker
 *   (`-dynamic-linker /lib/ld-linux-*.so.1`), which does the actual
 *   `.so` mapping for anything named directly on the link line (libc++.
 *   so, libc++abi.so, libunwind.so, ...) -- and already maintains that
 *   same standard rendezvous structure for gdb's own benefit, so walking
 *   it needs no ELF loader of this project's own. Added 2026-08-21 (see
 *   that date's HISTORY.md entry) once the imported-libc++ shared-
 *   linkage build became the first real consumer that needed more than
 *   the main executable alone: libunwind's own exception unwinding
 *   resolves .eh_frame/.gcc_except_table for PCs inside libcxxabi.so/
 *   libunwind.so itself (where __cxa_throw/the personality routine
 *   live), not just inside the main executable's own code.
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

/* Real glibc/Bionic <link.h> provide the ElfW(type) macro (pointer-size
 * independent access to the Elf32_/Elf64_ type family: ElfW(Phdr) ->
 * Elf64_Phdr on a 64-bit target), branching on __LP64__/__ELF_NATIVE_CLASS
 * because those libcs support both ELF32 and ELF64 targets. This project
 * only ever targets ELF64 (see elf.h's own comment), so the branch collapses
 * to a single, unconditional definition. Needed by LLVM libunwind's
 * AddressSpace.hpp, which falls back to defining ElfW()/Elf_Half/Elf_Phdr/
 * Elf_Addr itself only "on systems where <link.h> doesn't provide it" (its
 * comment says FreeBSD) -- without this, that fallback's own typedefs
 * (`typedef ElfW(Half) Elf_Half;`) are circular against themselves. */
#define ElfW(type) Elf64_##type

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
