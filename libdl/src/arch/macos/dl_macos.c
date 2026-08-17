#include <dlfcn.h>
#include <stddef.h>

#include <private/crt_macho_symbol.h>

#include "../../dl_internal.h"

/* Real dlopen()/dlsym() support for macOS.
 *
 * The Mach-O export-trie parsing engine itself (walking LC_DYLD_EXPORTS_TRIE
 * / LC_DYLD_INFO[_ONLY], following re-exports, ...) lives in libc as
 * libc/src/arch/macos/common/macho_symbol.c, not here: libc's own signal
 * delivery backend (libc/src/arch/macos/common/signal_backend.c) needs the
 * exact same capability to resolve the *real* libSystem sigaction()/
 * sigprocmask(), and libc cannot depend on libdl (libdl already depends on
 * libc) to get it via dlopen()/dlsym(). See
 * libc/include/private/crt_macho_symbol.h and docs/dynamic_loading.md for
 * the full design writeup. This file only adds the dlopen()/dlsym()-specific
 * policy on top: which image a NULL/RTLD_DEFAULT/RTLD_NEXT handle resolves
 * to, and loading a not-yet-mapped image via NSAddImage().
 */

/* Legacy image-loading API. Kept only for the "load an image dyld has not
 * already mapped" fallback in crt_dl_backend_open(): dlopen() for a path
 * dyld already loaded (true for every system library, since those are always
 * mapped before main() runs) is resolved by name match against the loaded
 * image list instead, without going through this API at all. */
extern const void* NSAddImage(const char* image_name, unsigned long options);
#define CRT_NSADDIMAGE_OPTION_RETURN_ON_ERROR 0x1UL

void* crt_dl_backend_open(const char* filename, int flags) {
  const void* image;

  (void)flags;
  if (filename == 0) {
    return CRT_DL_MAIN_HANDLE;
  }
  image = __crt_macho_find_loaded_image(filename);
  if (image != 0) {
    return (void*)image;
  }
  image = NSAddImage(filename, CRT_NSADDIMAGE_OPTION_RETURN_ON_ERROR);
  if (image == 0) {
    crt_dl_set_error("dlopen", "image not found");
  }
  return (void*)image;
}

void* crt_dl_backend_sym(void* handle, const char* symbol) {
  void* address;

  if (handle == RTLD_NEXT) {
    crt_dl_set_error("dlsym", "RTLD_NEXT is not implemented");
    return 0;
  }
  if (handle == RTLD_DEFAULT || handle == CRT_DL_MAIN_HANDLE) {
    address = __crt_macho_find_symbol_in_any_loaded_image(symbol);
  } else {
    address = __crt_macho_find_symbol_in_image(handle, symbol);
  }
  if (address == 0) {
    crt_dl_set_error("dlsym", "symbol not found");
  }
  return address;
}

int crt_dl_backend_close(void* handle) {
  (void)handle;
  return 0;
}

/* dl_iterate_phdr()'s dlpi_phdr/dlpi_phnum are ELF64_Phdr-shaped; Mach-O
 * has no such structure at all (a real, different format -- load commands/
 * segment commands, not ELF program headers). Fabricating ELF-shaped data
 * from real Mach-O data would be actively wrong, not just approximate --
 * see link.h's own comment. Honestly reports "no ELF images" by never
 * invoking the callback. */
int crt_dl_backend_iterate_phdr(
    int (*callback)(struct dl_phdr_info* info, size_t size, void* data), void* data) {
  (void)callback;
  (void)data;
  return 0;
}

int crt_dl_backend_addr_info(const void* addr, Dl_info* info) {
  const char* path = 0;
  const void* base = 0;

  if (!__crt_macho_find_image_for_address(addr, &path, &base)) {
    return 0;
  }
  info->dli_fname = path;
  info->dli_fbase = (void*)base;
  return 1;
}
