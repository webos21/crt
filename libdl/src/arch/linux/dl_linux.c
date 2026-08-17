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

/* Real AT_PHDR/AT_PHNUM data from the kernel-provided ELF auxiliary
 * vector -- see link.h's own comment for why this is a real, single-entry
 * (the main executable only) answer rather than a full dynamic-linker
 * implementation. */
int crt_dl_backend_iterate_phdr(
    int (*callback)(struct dl_phdr_info* info, size_t size, void* data), void* data) {
  unsigned long phdr_addr = getauxval(AT_PHDR);
  unsigned long phnum = getauxval(AT_PHNUM);
  const Elf64_Phdr* phdr;
  struct dl_phdr_info info;

  if (phdr_addr == 0 || phnum == 0) {
    return 0;
  }
  phdr = (const Elf64_Phdr*)(uintptr_t)phdr_addr;

  info.dlpi_addr = crt_dl_linux_main_bias(phdr, phnum, (Elf64_Addr)phdr_addr);
  info.dlpi_name = ""; /* matches glibc's own convention for the main executable */
  info.dlpi_phdr = phdr;
  info.dlpi_phnum = (Elf64_Half)phnum;
  return callback(&info, sizeof(info), data);
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
