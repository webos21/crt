#include <dlfcn.h>
#include <stddef.h>

#include "../../dl_internal.h"

typedef void* HMODULE;
typedef void* FARPROC;
typedef int BOOL;
typedef unsigned long DWORD;

__declspec(dllimport) HMODULE __stdcall LoadLibraryA(const char* lpLibFileName);
__declspec(dllimport) HMODULE __stdcall GetModuleHandleA(const char* lpModuleName);
__declspec(dllimport) FARPROC __stdcall GetProcAddress(HMODULE hModule, const char* lpProcName);
__declspec(dllimport) BOOL __stdcall FreeLibrary(HMODULE hLibModule);
__declspec(dllimport) BOOL __stdcall GetModuleHandleExA(
    DWORD dwFlags, const char* lpModuleName, HMODULE* phModule);
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(
    HMODULE hModule, char* lpFilename, DWORD nSize);

#define CRT_GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 0x00000004UL

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

/* dl_iterate_phdr()'s dlpi_phdr/dlpi_phnum are ELF64_Phdr-shaped; PE has no
 * such structure at all (a real, different format -- IMAGE_NT_HEADERS/
 * IMAGE_SECTION_HEADER, not ELF program headers). Fabricating ELF-shaped
 * data from real PE data would be actively wrong, not just approximate --
 * see link.h's own comment. Honestly reports "no ELF images" by never
 * invoking the callback. */
int crt_dl_backend_iterate_phdr(
    int (*callback)(struct dl_phdr_info* info, size_t size, void* data), void* data) {
  (void)callback;
  (void)data;
  return 0;
}

/* Real Win32 answer to "which loaded module contains this address":
 * GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS) walks the
 * process's own loaded-module list for us -- no VirtualQuery()/manual PE
 * header parsing needed. An HMODULE's own value *is* the module's real
 * load base address on Windows (a documented Win32 fact, not an
 * assumption), so it doubles directly as dli_fbase. dli_sname/dli_saddr
 * stay NULL/0 -- see dlfcn.h's own comment on why that's a legitimate
 * partial result, not a stub. */
int crt_dl_backend_addr_info(const void* addr, Dl_info* info) {
  HMODULE module = 0;
  static char path[260];
  DWORD length;

  if (!GetModuleHandleExA(CRT_GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (const char*)addr, &module) ||
      module == 0) {
    return 0;
  }
  length = GetModuleFileNameA(module, path, (DWORD)sizeof(path));
  if (length == 0 || length >= sizeof(path)) {
    return 0;
  }
  info->dli_fname = path;
  info->dli_fbase = (void*)module;
  return 1;
}
