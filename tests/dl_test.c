#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static int fail(const char* message) {
  fprintf(stderr, "dl_test: %s\n", message);
  return 1;
}

static int has_error(void) {
  char* error = dlerror();
  return error != 0 && error[0] != '\0';
}

int main(void) {
  void* handle;
  void* missing;
  char* error;

  if (dlerror() != 0) {
    return fail("initial dlerror");
  }

  handle = dlopen(0, RTLD_NOW | RTLD_LOCAL);
  if (handle == 0) {
    return fail("dlopen self");
  }
  if (dlerror() != 0) {
    return fail("dlopen self error");
  }

  missing = dlsym(handle, "crt_symbol_that_should_not_exist");
  if (missing != 0 || !has_error()) {
    return fail("dlsym missing");
  }
  if (dlerror() != 0) {
    return fail("dlerror one shot");
  }

  if (dlopen("crt_library_that_should_not_exist.so", RTLD_NOW) != 0 || !has_error()) {
    return fail("dlopen missing");
  }

#if defined(CRT_TARGET_OS_WINDOWS)
  handle = dlopen("kernel32.dll", RTLD_NOW);
  if (handle == 0) {
    return fail("dlopen kernel32");
  }
  if (dlsym(handle, "GetCurrentProcessId") == 0) {
    return fail("dlsym kernel32");
  }
  if (dlclose(handle) != 0) {
    return fail("dlclose kernel32");
  }
#elif defined(CRT_TARGET_OS_MACOS)
  handle = dlopen("/usr/lib/libSystem.B.dylib", RTLD_NOW);
  if (handle == 0) {
    return fail("dlopen libSystem");
  }
  if (dlsym(handle, "getpid") == 0) {
    return fail("dlsym libSystem");
  }
  if (dlclose(handle) != 0) {
    return fail("dlclose libSystem");
  }
#else
  if (dlclose(handle) != 0) {
    return fail("dlclose self");
  }
#endif

  error = dlerror();
  if (error != 0 && strstr(error, "dlclose") != 0) {
    return fail("unexpected final dlclose error");
  }

  printf("dl_test: ok\n");
  return 0;
}
