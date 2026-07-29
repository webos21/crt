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

#ifdef __cplusplus
}
#endif

#endif
