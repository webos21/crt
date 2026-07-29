#include <stdint.h>
#include <stdio.h>

typedef void (*destructor_t)(void*);

extern void* __dso_handle;
int __cxa_guard_acquire(uint64_t* guard_object);
void __cxa_guard_release(uint64_t* guard_object);
void __cxa_guard_abort(uint64_t* guard_object);
int __cxa_atexit(destructor_t destructor, void* object, void* dso);
void __cxa_finalize(void* dso);
void _Init_thread_header(volatile int* guard);
void _Init_thread_footer(volatile int* guard);
void _Init_thread_abort(volatile int* guard);

static int fail(const char* message) {
  fprintf(stderr, "cxx_runtime_test: %s\n", message);
  return 1;
}

static void add_value(void* object) {
  int* value = (int*)object;
  *value += 1;
}

static void add_large_value(void* object) {
  int* value = (int*)object;
  *value += 10;
}

int main(void) {
  uint64_t guard = 0;
  int first = 0;
  int second = 0;
  int other = 0;
  char local_dso;

  if (__cxa_guard_acquire(&guard) != 1) {
    return fail("guard first acquire");
  }
  __cxa_guard_release(&guard);
  if ((guard & 0xffu) == 0) {
    return fail("guard complete byte");
  }
  if (__cxa_guard_acquire(&guard) != 0) {
    return fail("guard second acquire");
  }

  guard = 0;
  if (__cxa_guard_acquire(&guard) != 1) {
    return fail("guard abort acquire");
  }
  __cxa_guard_abort(&guard);
  if (__cxa_guard_acquire(&guard) != 1) {
    return fail("guard reacquire after abort");
  }
  __cxa_guard_release(&guard);

  if (__cxa_atexit(add_value, &first, &__dso_handle) != 0 ||
      __cxa_atexit(add_large_value, &second, &local_dso) != 0 ||
      __cxa_atexit(add_value, &other, 0) != 0) {
    return fail("cxa atexit register");
  }

  __cxa_finalize(&local_dso);
  if (first != 0 || second != 10 || other != 0) {
    return fail("cxa finalize dso");
  }
  __cxa_finalize(&local_dso);
  if (second != 10) {
    return fail("cxa finalize dso once");
  }
  __cxa_finalize(0);
  if (first != 1 || second != 10 || other != 1) {
    return fail("cxa finalize all");
  }

  {
    volatile int msvc_guard = 0;
    _Init_thread_header(&msvc_guard);
    if (msvc_guard != -1) {
      return fail("msvc guard acquire");
    }
    _Init_thread_footer(&msvc_guard);
    if (msvc_guard <= 0) {
      return fail("msvc guard release");
    }
    _Init_thread_header(&msvc_guard);
    if (msvc_guard <= 0) {
      return fail("msvc guard already initialized");
    }
    _Init_thread_abort(&msvc_guard);
    if (msvc_guard != 0) {
      return fail("msvc guard abort");
    }
  }

  printf("cxx_runtime_test: ok\n");
  return 0;
}
