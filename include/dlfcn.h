#ifndef CRT_DLFCN_H
#define CRT_DLFCN_H

#ifdef __cplusplus
extern "C" {
#endif

#define RTLD_LAZY 0x00001
#define RTLD_NOW 0x00002
#define RTLD_LOCAL 0x00000
#define RTLD_GLOBAL 0x00100

#define RTLD_DEFAULT ((void*)0)
#define RTLD_NEXT ((void*)-1)

void* dlopen(const char* filename, int flags);
void* dlsym(void* handle, const char* symbol);
int dlclose(void* handle);
char* dlerror(void);

/* dladdr() -- see docs/dynamic_loading.md and docs/bionic_libc_gaps.md/
 * HISTORY.md's 2026-08-17 entry. Real per-host implementation (Windows:
 * VirtualQuery()+GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS);
 * macOS: dyld's own loaded-image list, the same infrastructure libdl's
 * dlopen()/dlsym() backend already uses; Linux: the main executable's own
 * AT_PHDR-derived PT_LOAD ranges, the same real auxv data dl_iterate_phdr()
 * uses -- see link.h's own comment for why that's a real, if narrow,
 * answer rather than a stub, given this project has no real ELF dynamic
 * linker yet). `dli_sname`/`dli_saddr` (the nearest-symbol part of the
 * real contract) are always left `NULL`/`0` on every host -- POSIX/Bionic
 * both document that as a legitimate result when no matching symbol is
 * found, not a failure, and this project does not yet parse any host's
 * symbol table for reverse address->name lookup. `dli_fname`/`dli_fbase`
 * (which object contains this address) are real wherever the address
 * actually falls inside a loaded image this project can see; `dladdr()`
 * returns 0 (not found) otherwise, matching the real contract. */
typedef struct {
  const char* dli_fname;
  void* dli_fbase;
  const char* dli_sname;
  void* dli_saddr;
} Dl_info;

int dladdr(const void* addr, Dl_info* info);

#ifdef __cplusplus
}
#endif

#endif
