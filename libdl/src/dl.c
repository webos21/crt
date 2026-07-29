#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CRT_DL_MAIN_HANDLE ((void*)-3)

static char crt_dl_error[160];
static int crt_dl_error_pending;

static void set_dl_error(const char* operation, const char* detail) {
  snprintf(crt_dl_error, sizeof(crt_dl_error), "%s: %s", operation, detail);
  crt_dl_error_pending = 1;
}

static void clear_dl_error(void) {
  crt_dl_error[0] = '\0';
  crt_dl_error_pending = 0;
}

#if defined(CRT_TARGET_OS_WINDOWS)
typedef void* HMODULE;
typedef void* FARPROC;
typedef int BOOL;

__declspec(dllimport) HMODULE __stdcall LoadLibraryA(const char* lpLibFileName);
__declspec(dllimport) HMODULE __stdcall GetModuleHandleA(const char* lpModuleName);
__declspec(dllimport) FARPROC __stdcall GetProcAddress(HMODULE hModule, const char* lpProcName);
__declspec(dllimport) BOOL __stdcall FreeLibrary(HMODULE hLibModule);

void* dlopen(const char* filename, int flags) {
  HMODULE module;

  (void)flags;
  clear_dl_error();
  module = filename == 0 ? GetModuleHandleA(0) : LoadLibraryA(filename);
  if (module == 0) {
    set_dl_error("dlopen", "LoadLibraryA failed");
  }
  return module;
}

void* dlsym(void* handle, const char* symbol) {
  FARPROC address;

  clear_dl_error();
  if (symbol == 0) {
    set_dl_error("dlsym", "null symbol");
    return 0;
  }
  if (handle == RTLD_DEFAULT || handle == CRT_DL_MAIN_HANDLE) {
    handle = GetModuleHandleA(0);
  }
  if (handle == 0 || handle == RTLD_NEXT) {
    set_dl_error("dlsym", "unsupported handle");
    return 0;
  }
  address = GetProcAddress((HMODULE)handle, symbol);
  if (address == 0) {
    set_dl_error("dlsym", "symbol not found");
  }
  return (void*)address;
}

int dlclose(void* handle) {
  clear_dl_error();
  if (handle == 0 || handle == RTLD_DEFAULT || handle == RTLD_NEXT || handle == CRT_DL_MAIN_HANDLE) {
    set_dl_error("dlclose", "invalid handle");
    return -1;
  }
  return FreeLibrary((HMODULE)handle) ? 0 : -1;
}

#elif defined(CRT_TARGET_OS_MACOS)
struct mach_header;
typedef void* NSSymbol;

extern unsigned int _dyld_image_count(void);
extern const struct mach_header* _dyld_get_image_header(unsigned int image_index);
extern const struct mach_header* NSAddImage(const char* image_name, unsigned long options);
extern NSSymbol NSLookupSymbolInImage(
    const struct mach_header* image,
    const char* symbol_name,
    unsigned long options);
extern void* NSAddressOfSymbol(NSSymbol symbol);

#define CRT_NSADDIMAGE_OPTION_RETURN_ON_ERROR 0x1UL
#define CRT_NSLOOKUPSYMBOLINIMAGE_OPTION_BIND 0x0UL
#define CRT_NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR 0x4UL

static void make_mach_symbol(const char* symbol, char* buffer, size_t size) {
  if (size == 0) {
    return;
  }
  buffer[0] = '_';
  if (size > 1) {
    size_t i;
    for (i = 1; i + 1 < size && symbol[i - 1] != '\0'; ++i) {
      buffer[i] = symbol[i - 1];
    }
    buffer[i] = '\0';
  }
}

static void* lookup_in_image(const struct mach_header* image, const char* symbol) {
  char mach_symbol[256];
  NSSymbol ns_symbol;

  make_mach_symbol(symbol, mach_symbol, sizeof(mach_symbol));
  ns_symbol = NSLookupSymbolInImage(
      image,
      mach_symbol,
      CRT_NSLOOKUPSYMBOLINIMAGE_OPTION_BIND |
          CRT_NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
  return ns_symbol == 0 ? 0 : NSAddressOfSymbol(ns_symbol);
}

void* dlopen(const char* filename, int flags) {
  const struct mach_header* image;

  (void)flags;
  clear_dl_error();
  if (filename == 0) {
    return CRT_DL_MAIN_HANDLE;
  }
  image = NSAddImage(filename, CRT_NSADDIMAGE_OPTION_RETURN_ON_ERROR);
  if (image == 0) {
    set_dl_error("dlopen", "NSAddImage failed");
  }
  return (void*)image;
}

void* dlsym(void* handle, const char* symbol) {
  void* address = 0;

  clear_dl_error();
  if (symbol == 0) {
    set_dl_error("dlsym", "null symbol");
    return 0;
  }
  if (handle == RTLD_NEXT) {
    set_dl_error("dlsym", "RTLD_NEXT is not implemented");
    return 0;
  }
  if (handle == RTLD_DEFAULT || handle == CRT_DL_MAIN_HANDLE) {
    unsigned int count = _dyld_image_count();
    unsigned int i;

    for (i = 0; i < count; ++i) {
      address = lookup_in_image(_dyld_get_image_header(i), symbol);
      if (address != 0) {
        return address;
      }
    }
  } else {
    address = lookup_in_image((const struct mach_header*)handle, symbol);
    if (address != 0) {
      return address;
    }
  }
  set_dl_error("dlsym", "symbol not found");
  return 0;
}

int dlclose(void* handle) {
  clear_dl_error();
  if (handle == 0 || handle == RTLD_DEFAULT || handle == RTLD_NEXT || handle == CRT_DL_MAIN_HANDLE) {
    set_dl_error("dlclose", "invalid handle");
    return -1;
  }
  return 0;
}

#else
void* dlopen(const char* filename, int flags) {
  (void)flags;
  clear_dl_error();
  if (filename == 0) {
    return CRT_DL_MAIN_HANDLE;
  }
  set_dl_error("dlopen", "host dynamic loading backend is not linked");
  return 0;
}

void* dlsym(void* handle, const char* symbol) {
  (void)handle;
  (void)symbol;
  clear_dl_error();
  set_dl_error("dlsym", "host dynamic loading backend is not linked");
  return 0;
}

int dlclose(void* handle) {
  clear_dl_error();
  if (handle == CRT_DL_MAIN_HANDLE) {
    return 0;
  }
  set_dl_error("dlclose", "host dynamic loading backend is not linked");
  return -1;
}
#endif

char* dlerror(void) {
  if (!crt_dl_error_pending) {
    return 0;
  }
  crt_dl_error_pending = 0;
  return crt_dl_error;
}
