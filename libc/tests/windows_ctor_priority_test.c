/* Clang MinGW emits priority constructors into .ctors.NNNNN, outside the
 * former plain-.ctors sentinel range. Declare in the wrong order on purpose. */
#include <stdio.h>
#include <unistd.h>

static int constructor_order;
static int destructor_order;
static int entered;
#if defined(CRT_CTOR_PRIORITY_DLL)
static int* unloaded;
#endif

__attribute__((constructor)) static void init_default(void) {
  constructor_order = constructor_order * 10 + 3;
}
__attribute__((constructor(201))) static void init_middle(void) {
  constructor_order = constructor_order * 10 + 2;
}
__attribute__((constructor(101))) static void init_first(void) {
  constructor_order = constructor_order * 10 + 1;
}

__attribute__((destructor)) static void fini_default(void) {
  destructor_order = destructor_order * 10 + 3;
}
__attribute__((destructor(201))) static void fini_middle(void) {
  destructor_order = destructor_order * 10 + 2;
}
__attribute__((destructor(101))) static void fini_last(void) {
  destructor_order = destructor_order * 10 + 1;
  if (!entered || constructor_order != 123 || destructor_order != 321) {
    _exit(1);
  }
#if defined(CRT_CTOR_PRIORITY_DLL)
  *unloaded = 1;
#else
  puts("windows_ctor_priority_test: ok");
  fflush(stdout);
#endif
}

#if defined(CRT_CTOR_PRIORITY_DLL)
__attribute__((dllexport)) int crt_ctor_priority_check(int* result) {
  unloaded = result;
#else
int main(void) {
#endif
  if (constructor_order != 123) {
    fprintf(stderr, "windows_ctor_priority_test: constructor order=%d, expected 123\n",
            constructor_order);
    return 1;
  }
  entered = 1;
  return 0;
}
