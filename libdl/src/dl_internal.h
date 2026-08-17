#ifndef CRT_DL_INTERNAL_H
#define CRT_DL_INTERNAL_H

#include <dlfcn.h>
#include <link.h>
#include <stddef.h>

/* Sentinel returned by dlopen(NULL) on hosts where "the main program" is not
 * naturally a real host module handle. Hosts that already have a real handle
 * for the main program (e.g. Windows GetModuleHandleA(0)) are not required to
 * use this value themselves, but must still recognize it if passed into
 * dlsym()/dlclose(). */
#define CRT_DL_MAIN_HANDLE ((void*)-3)

/* Shared dlerror() state, set by dl.c and any backend. */
void crt_dl_set_error(const char* operation, const char* detail);
void crt_dl_clear_error(void);

/* Implemented once per host under src/arch/{linux,macos,windows}/dl_*.c. */
void* crt_dl_backend_open(const char* filename, int flags);
void* crt_dl_backend_sym(void* handle, const char* symbol);
int crt_dl_backend_close(void* handle);

/* dl_iterate_phdr()'s per-host backend. See link.h's own comment: real,
 * non-fabricated ELF program-header data only exists on Linux (the main
 * executable, via AT_PHDR/AT_PHNUM), and even there only one entry, since
 * this project has no real ELF dynamic linker yet. Returns whatever the
 * last callback invocation returned, or 0 if the callback was never
 * invoked (no images to report) -- matches dl_iterate_phdr()'s own
 * contract exactly, so dl.c's dispatcher is a direct passthrough with no
 * translation needed. */
int crt_dl_backend_iterate_phdr(
    int (*callback)(struct dl_phdr_info* info, size_t size, void* data), void* data);

/* dladdr()'s per-host backend. Returns nonzero (found) with `*info` filled
 * in, or 0 (not found) -- matches dladdr()'s own contract exactly. See
 * dlfcn.h's own comment: dli_sname/dli_saddr are always left NULL/0 on
 * every host (a legitimate partial result per the real contract, not a
 * failure), only dli_fname/dli_fbase are ever real. */
int crt_dl_backend_addr_info(const void* addr, Dl_info* info);

#endif
