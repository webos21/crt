#include <dlfcn.h>
#include <stddef.h>

#include "../../dl_internal.h"

typedef void* HMODULE;
typedef void* FARPROC;
typedef int BOOL;

__declspec(dllimport) HMODULE __stdcall LoadLibraryA(const char* lpLibFileName);
__declspec(dllimport) HMODULE __stdcall GetModuleHandleA(const char* lpModuleName);
__declspec(dllimport) FARPROC __stdcall GetProcAddress(HMODULE hModule, const char* lpProcName);
__declspec(dllimport) BOOL __stdcall FreeLibrary(HMODULE hLibModule);

void* crt_dl_backend_open(const char* filename, int flags) {
  HMODULE module;

  (void)flags;
  if (filename == 0) {
    /* Use the shared main-program sentinel here (matching the macOS/Linux
     * backends) rather than returning GetModuleHandleA(0) directly: dlclose()
     * on that raw handle would call FreeLibrary() on the process's own main
     * executable module, which does not own a LoadLibrary-style reference
     * count and must never be freed this way (dlfcn-win32, the reference
     * Windows dlfcn implementation, special-cases exactly this in its own
     * dlclose() to avoid it). crt_dl_backend_sym() already translates
     * CRT_DL_MAIN_HANDLE to GetModuleHandleA(0) for lookups, and dl.c's
     * shared dlclose() already rejects the sentinel outright. */
    return CRT_DL_MAIN_HANDLE;
  }
  module = LoadLibraryA(filename);
  if (module == 0) {
    crt_dl_set_error("dlopen", "LoadLibraryA failed");
  }
  return module;
}

void* crt_dl_backend_sym(void* handle, const char* symbol) {
  FARPROC address;

  if (handle == RTLD_DEFAULT || handle == CRT_DL_MAIN_HANDLE) {
    handle = GetModuleHandleA(0);
  }
  if (handle == 0 || handle == RTLD_NEXT) {
    crt_dl_set_error("dlsym", "unsupported handle");
    return 0;
  }
  address = GetProcAddress((HMODULE)handle, symbol);
  if (address == 0) {
    crt_dl_set_error("dlsym", "symbol not found");
  }
  return (void*)address;
}

int crt_dl_backend_close(void* handle) {
  return FreeLibrary((HMODULE)handle) ? 0 : -1;
}
