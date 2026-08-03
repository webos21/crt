#ifndef CRT_DL_INTERNAL_H
#define CRT_DL_INTERNAL_H

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

#endif
