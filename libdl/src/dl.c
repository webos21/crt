#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>

#include "dl_internal.h"

static char crt_dl_error[160];
static int crt_dl_error_pending;

void crt_dl_set_error(const char* operation, const char* detail) {
  snprintf(crt_dl_error, sizeof(crt_dl_error), "%s: %s", operation, detail);
  crt_dl_error_pending = 1;
}

void crt_dl_clear_error(void) {
  crt_dl_error[0] = '\0';
  crt_dl_error_pending = 0;
}

void* dlopen(const char* filename, int flags) {
  crt_dl_clear_error();
  return crt_dl_backend_open(filename, flags);
}

void* dlsym(void* handle, const char* symbol) {
  crt_dl_clear_error();
  if (symbol == 0) {
    crt_dl_set_error("dlsym", "null symbol");
    return 0;
  }
  return crt_dl_backend_sym(handle, symbol);
}

int dlclose(void* handle) {
  crt_dl_clear_error();
  if (handle == 0 || handle == RTLD_DEFAULT || handle == RTLD_NEXT) {
    crt_dl_set_error("dlclose", "invalid handle");
    return -1;
  }
  if (handle == CRT_DL_MAIN_HANDLE) {
    /* dlopen(NULL) does not correspond to a loadable/unloadable resource
     * (no backend actually called its "load a library" primitive to obtain
     * it), so dlclose() on it is a harmless no-op success, matching real
     * dlopen()/dlclose() behavior on Linux and macOS. This also keeps
     * crt_dl_backend_close() from ever seeing the sentinel: on Windows in
     * particular, crt_dl_backend_open() returns this same sentinel instead
     * of the real main-module HMODULE specifically so FreeLibrary() is never
     * called on the process's own main executable module. */
    return 0;
  }
  return crt_dl_backend_close(handle);
}

char* dlerror(void) {
  if (!crt_dl_error_pending) {
    return 0;
  }
  crt_dl_error_pending = 0;
  return crt_dl_error;
}

int dl_iterate_phdr(
    int (*callback)(struct dl_phdr_info* info, size_t size, void* data), void* data) {
  if (callback == 0) {
    return 0;
  }
  return crt_dl_backend_iterate_phdr(callback, data);
}

int dladdr(const void* addr, Dl_info* info) {
  if (addr == 0 || info == 0) {
    return 0;
  }
  info->dli_fname = 0;
  info->dli_fbase = 0;
  info->dli_sname = 0;
  info->dli_saddr = 0;
  return crt_dl_backend_addr_info(addr, info);
}
