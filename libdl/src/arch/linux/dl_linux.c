#include <dlfcn.h>
#include <elf.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/auxv.h>
#include <unistd.h>

#include "../../dl_internal.h"

/* CRT targets do not link a host libc on Linux (see AGENTS.md: no accidental
 * host libc dependency), so there is no host dlopen()/dlsym() to delegate to
 * the way the macOS backend delegates to Mach-O image APIs or the Windows
 * backend delegates to LoadLibraryA/GetProcAddress. Real dynamic loading on
 * Linux needs this project's own ELF loader, which AGENTS.md defers to the
 * later `linker/` phase. Until then this backend honestly reports "not
 * supported" instead of silently pretending to load anything. */

/* dl_iterate_phdr() reporting every loaded shared object (not just the main
 * executable -- see crt_dl_backend_iterate_phdr()'s own comment below for
 * why that used to be the whole story). This project links every Linux
 * executable/DSO against the real system dynamic linker
 * (`-dynamic-linker /lib/ld-linux-*.so.1`, see tools/crt-cc/crt-c++'s own
 * link line): it does the actual `.so` mapping, and it already maintains
 * the standard SVR4 "rendezvous" structures below for gdb's benefit --
 * real glibc's own dl_iterate_phdr() walks exactly these same structures
 * internally, and gdb/lldb rely on the identical protocol. They are a
 * stable, documented public ABI (confirmed against this host's own
 * /usr/include/link.h, Ubuntu 24.04's glibc), not an implementation
 * private detail, so declaring them ourselves here is legitimate -- this
 * project just doesn't otherwise use glibc's own <link.h>, which mixes
 * this rendezvous protocol with a large surface of unrelated glibc-
 * specific dlopen() plumbing (struct r_debug_extended, audit interfaces,
 * ld.so.cache, ...) this project has no use for. */
struct crt_link_map {
  Elf64_Addr l_addr; /* Load bias: runtime address minus link-time vaddr. */
  const char* l_name;
  const Elf64_Dyn* l_ld; /* This object's own .dynamic section. */
  struct crt_link_map* l_next;
  struct crt_link_map* l_prev;
};

struct crt_r_debug {
  int r_version;
  struct crt_link_map* r_map;
  Elf64_Addr r_brk;
  int r_state;
  Elf64_Addr r_ldbase;
};

/* `_DYNAMIC` is a linker-synthesized symbol pointing at the start of this
 * executable's own .dynamic section, present in every dynamically-linked
 * ELF image (documented ABI, see /usr/include/link.h's own comment: "This
 * symbol refers to the 'dynamic structure' ... of whatever module refers
 * to `_DYNAMIC`"). Once the real system dynamic linker has finished
 * startup (always true by the time any of this project's own code runs),
 * its DT_DEBUG entry holds the address of struct r_debug -- the same
 * technique gdb itself and every from-scratch dl_iterate_phdr()
 * implementation use to find it, deliberately not the simpler-looking
 * `extern struct r_debug _r_debug` glibc's own <link.h> also documents:
 * that symbol lives inside the separate ld.so image, not something this
 * project's own link automatically resolves against. */
extern const Elf64_Dyn _DYNAMIC[];

static const struct crt_r_debug* crt_dl_linux_find_r_debug(void) {
  const Elf64_Dyn* dyn;

  for (dyn = _DYNAMIC; dyn->d_tag != DT_NULL; ++dyn) {
    if (dyn->d_tag == DT_DEBUG) {
      return (const struct crt_r_debug*)(uintptr_t)dyn->d_un.d_ptr;
    }
  }
  return 0;
}

void* crt_dl_backend_open(const char* filename, int flags) {
  (void)flags;
  if (filename == 0) {
    return CRT_DL_MAIN_HANDLE;
  }
  crt_dl_set_error("dlopen", "ELF dynamic loading is not implemented yet");
  return 0;
}

void* crt_dl_backend_sym(void* handle, const char* symbol) {
  (void)handle;
  (void)symbol;
  crt_dl_set_error("dlsym", "ELF dynamic loading is not implemented yet");
  return 0;
}

int crt_dl_backend_close(void* handle) {
  (void)handle;
  crt_dl_set_error("dlclose", "ELF dynamic loading is not implemented yet");
  return -1;
}

/* Finds the PT_PHDR entry in `phdr` (if present) and returns the real load
 * bias: PT_PHDR's own p_vaddr is the link-time address the phdr table
 * would be at with zero bias, and `phdr_addr` is already its real runtime
 * address (from AT_PHDR), so bias = phdr_addr - p_vaddr. Falls back to 0
 * (correct for a non-PIE executable; a documented best-effort for a PIE
 * one built without a PT_PHDR segment) when absent. */
static Elf64_Addr crt_dl_linux_main_bias(const Elf64_Phdr* phdr, unsigned long phnum, Elf64_Addr phdr_addr) {
  unsigned long i;

  for (i = 0; i < phnum; ++i) {
    if (phdr[i].p_type == PT_PHDR) {
      return phdr_addr - phdr[i].p_vaddr;
    }
  }
  return 0;
}

/* Real AT_PHDR/AT_PHNUM data from the kernel-provided ELF auxiliary vector
 * for the main executable, matching glibc's own dl_iterate_phdr(): the
 * kernel hands the executable's own phdr address/count to every process
 * directly at exec() time, which is more reliable than computing it from
 * the executable's own link_map entry (link_map only gives a load bias
 * and requires trusting that the ELF header is both mapped and un-
 * corrupted at that address -- true in practice, but the kernel-supplied
 * auxv answer needs no such assumption). Then walks every OTHER loaded
 * shared object via the real system dynamic linker's own struct
 * r_debug/link_map rendezvous list (see the top of this file for why that
 * walk is legitimate) -- until this second half was added, this function
 * reported the main executable only, which real Bionic-worthy Linux work
 * has never needed until the imported-libc++ shared-linkage path: libc++/
 * libc++abi/libunwind's own exception unwinding needs to resolve
 * .eh_frame/.gcc_except_table for PCs that live inside THOSE .so's, not
 * just the main executable's own code (see HISTORY.md's 2026-08-21 entry
 * for the crash this caused before this walk existed, root-caused via
 * gdb). The main executable's own link_map entry is skipped during the
 * walk (compared by `l_ld == _DYNAMIC`, not by list position -- the ELF
 * TIS spec places it first, but comparing the actual .dynamic section
 * pointer is a stronger check that does not depend on that ordering
 * holding) since it was already reported precisely via auxv above;
 * reporting it twice would not be incorrect (callers must already tolerate
 * being called more than once per real image, e.g. after dlopen()) but is
 * pointless extra work every single call. */
int crt_dl_backend_iterate_phdr(
    int (*callback)(struct dl_phdr_info* info, size_t size, void* data), void* data) {
  unsigned long phdr_addr = getauxval(AT_PHDR);
  unsigned long phnum = getauxval(AT_PHNUM);
  const Elf64_Phdr* phdr;
  struct dl_phdr_info info;
  const struct crt_r_debug* debug;
  const struct crt_link_map* map;
  int result;

  if (phdr_addr == 0 || phnum == 0) {
    return 0;
  }
  phdr = (const Elf64_Phdr*)(uintptr_t)phdr_addr;

  info.dlpi_addr = crt_dl_linux_main_bias(phdr, phnum, (Elf64_Addr)phdr_addr);
  info.dlpi_name = ""; /* matches glibc's own convention for the main executable */
  info.dlpi_phdr = phdr;
  info.dlpi_phnum = (Elf64_Half)phnum;
  result = callback(&info, sizeof(info), data);
  if (result != 0) {
    return result;
  }

  debug = crt_dl_linux_find_r_debug();
  if (debug == 0) {
    return 0;
  }
  for (map = debug->r_map; map != 0; map = map->l_next) {
    const Elf64_Ehdr* ehdr;

    if (map->l_ld == _DYNAMIC) {
      continue; /* the main executable's own entry, already reported above */
    }
    /* The ELF header always lives at the very start of an image's first
     * PT_LOAD segment, so it is guaranteed mapped and readable for any
     * object the real dynamic linker actually loaded -- e_phoff/e_phnum
     * from it locate that same object's own program header table, which
     * link_map itself does not carry directly. PN_XNUM (e_phnum stored in
     * the section header table instead, for objects with >= 0xffff
     * program headers) is not handled: no image this project links comes
     * remotely close to that many segments, and dl_phdr_info's own
     * dlpi_phnum field is a 16-bit Elf64_Half regardless. */
    ehdr = (const Elf64_Ehdr*)(uintptr_t)map->l_addr;
    if (ehdr->e_phnum == 0) {
      continue;
    }

    info.dlpi_addr = map->l_addr;
    info.dlpi_name = map->l_name != 0 ? map->l_name : "";
    info.dlpi_phdr = (const Elf64_Phdr*)(map->l_addr + ehdr->e_phoff);
    info.dlpi_phnum = ehdr->e_phnum;
    result = callback(&info, sizeof(info), data);
    if (result != 0) {
      return result;
    }
  }
  return 0;
}

/* Checks whether `addr` falls inside one of the main executable's own
 * PT_LOAD segments (the only "loaded image" this project can see without
 * a real ELF dynamic linker -- same reasoning as
 * crt_dl_backend_iterate_phdr() above) and, if so, reports the
 * executable's own real path via /proc/self/exe. */
int crt_dl_backend_addr_info(const void* addr, Dl_info* info) {
  unsigned long phdr_addr = getauxval(AT_PHDR);
  unsigned long phnum = getauxval(AT_PHNUM);
  const Elf64_Phdr* phdr;
  Elf64_Addr bias;
  Elf64_Addr target = (Elf64_Addr)(uintptr_t)addr;
  unsigned long i;
  int found = 0;
  static char path[4096];
  ssize_t length;

  if (phdr_addr == 0 || phnum == 0) {
    return 0;
  }
  phdr = (const Elf64_Phdr*)(uintptr_t)phdr_addr;
  bias = crt_dl_linux_main_bias(phdr, phnum, (Elf64_Addr)phdr_addr);

  for (i = 0; i < phnum; ++i) {
    if (phdr[i].p_type == PT_LOAD) {
      Elf64_Addr start = bias + phdr[i].p_vaddr;
      Elf64_Addr end = start + phdr[i].p_memsz;

      if (target >= start && target < end) {
        found = 1;
        break;
      }
    }
  }
  if (!found) {
    return 0;
  }

  length = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (length <= 0) {
    return 0;
  }
  path[length] = '\0';
  info->dli_fname = path;
  info->dli_fbase = (void*)(uintptr_t)bias;
  return 1;
}
