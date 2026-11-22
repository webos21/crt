/* Installed-SDK Windows consumer: report which libc.dll the process really loaded. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

void *GetModuleHandleA(const char *name);
unsigned long GetModuleFileNameA(void *module, char *buffer, unsigned long size);

static int initialized;
__attribute__((constructor)) static void initialize(void) { initialized = 1; }

#ifdef CRT_TEST_PROJECT_DLL
int *crt_provider_errno(void);
void *crt_provider_allocate(void);
int crt_provider_initialized(void);
#endif

int main(void) {
  if (!initialized) return 2;
  unsigned char *allocation = calloc(32, 1);
  if (!allocation || allocation[0] || allocation[31]) return 3;
  free(allocation);
#ifdef CRT_TEST_PROJECT_DLL
  if (!crt_provider_initialized() || crt_provider_errno() != &errno) return 4;
  allocation = crt_provider_allocate();
  if (!allocation || allocation[0] != 73) return 5;
  free(allocation);
#endif
  char path[1024];
  void *module = GetModuleHandleA("libc.dll");
  if (module) {
    unsigned long length = GetModuleFileNameA(module, path, sizeof(path));
    if (length == 0 || length >= sizeof(path)) return 6;
    printf("LIBC_DLL:%s\n", path);
  } else {
    puts("LIBC_DLL:none");
  }
  puts("sdk_c_provider: ok");
  return 0;
}
