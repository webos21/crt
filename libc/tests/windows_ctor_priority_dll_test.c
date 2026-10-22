#include <dlfcn.h>
#include <stdio.h>

int main(void) {
  int unloaded = 0;
  int (*check)(int*);
  void* library = dlopen(CRT_PRIORITY_DLL_PATH, RTLD_NOW);

  if (library == 0) {
    fprintf(stderr, "windows_ctor_priority_dll_test: %s\n", dlerror());
    return 1;
  }
  check = (int (*)(int*))dlsym(library, "crt_ctor_priority_check");
  if (check == 0 || check(&unloaded) != 0) return 1;
  if (dlclose(library) != 0 || unloaded != 1) {
    fputs("windows_ctor_priority_dll_test: destructor order/notification\n", stderr);
    return 1;
  }
  puts("windows_ctor_priority_dll_test: ok");
  return 0;
}
