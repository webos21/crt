#include <dlfcn.h>
#include <stddef.h>

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
