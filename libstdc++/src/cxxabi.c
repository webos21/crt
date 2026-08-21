#include <stdint.h>
#include <stdlib.h>

#include <private/crt_atomic.h>

/* __cxa_atexit()/__cxa_finalize()/__dso_handle are guarded out on Linux:
 * libc/src/arch/linux/common/cxa_atexit.c now provides real ones there
 * (matching real glibc/Bionic, which put these in libc, not in libc++abi
 * -- see that file's own comment for the full reasoning). Keeping both
 * copies for Linux would be a genuine multiple-definition conflict: any
 * C++ program using function-local statics needs this file's own
 * __cxa_guard_acquire() below, which drags this whole translation unit's
 * other symbols in right alongside once the linker pulls it in. macOS
 * (gets a real __cxa_atexit from libSystem.dylib -- see libc/src/exit.c's
 * own comment) and Windows (separate, still-open crt-libcxx-build work,
 * TODO.md) are unaffected and keep this file's copy exactly as before. */
#if !defined(CRT_TARGET_OS_LINUX)
#define CRT_CXXABI_C_OWNS_CXA_ATEXIT 1
#endif

#define CRT_GUARD_UNINITIALIZED 0
#define CRT_GUARD_INITIALIZING 1
#define CRT_GUARD_COMPLETE 2

#if defined(CRT_CXXABI_C_OWNS_CXA_ATEXIT)
#define CRT_CXA_ATEXIT_MAX 128

typedef void (*crt_cxa_destructor_t)(void*);

typedef struct {
  crt_cxa_destructor_t destructor;
  void* object;
  void* dso;
  int called;
} crt_cxa_atexit_entry;

static crt_spinlock crt_cxa_atexit_lock = CRT_SPINLOCK_INIT;
static crt_cxa_atexit_entry crt_cxa_atexit_entries[CRT_CXA_ATEXIT_MAX];
static int crt_cxa_atexit_count;

void* __dso_handle = &__dso_handle;
#endif /* CRT_CXXABI_C_OWNS_CXA_ATEXIT */

int __cxa_guard_acquire(uint64_t* guard_object) {
  int* state = (int*)guard_object;

  for (;;) {
    int expected = CRT_GUARD_UNINITIALIZED;

    if (__atomic_load_n(state, __ATOMIC_ACQUIRE) == CRT_GUARD_COMPLETE) {
      return 0;
    }

    if (__atomic_compare_exchange_n(
            state,
            &expected,
            CRT_GUARD_INITIALIZING,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
      return 1;
    }

    while (__atomic_load_n(state, __ATOMIC_ACQUIRE) == CRT_GUARD_INITIALIZING) {
      sched_yield();
    }
  }
}

void __cxa_guard_release(uint64_t* guard_object) {
  int* state = (int*)guard_object;

  __atomic_store_n(state, CRT_GUARD_COMPLETE, __ATOMIC_RELEASE);
}

void __cxa_guard_abort(uint64_t* guard_object) {
  int* state = (int*)guard_object;

  __atomic_store_n(state, CRT_GUARD_UNINITIALIZED, __ATOMIC_RELEASE);
}

#if defined(CRT_CXXABI_C_OWNS_CXA_ATEXIT)
int __cxa_atexit(crt_cxa_destructor_t destructor, void* object, void* dso) {
  int result = 0;

  if (destructor == 0) {
    return -1;
  }

  crt_spin_lock(&crt_cxa_atexit_lock);
  if (crt_cxa_atexit_count >= CRT_CXA_ATEXIT_MAX) {
    result = -1;
  } else {
    crt_cxa_atexit_entry* entry = &crt_cxa_atexit_entries[crt_cxa_atexit_count++];
    entry->destructor = destructor;
    entry->object = object;
    entry->dso = dso;
    entry->called = 0;
  }
  crt_spin_unlock(&crt_cxa_atexit_lock);

  return result;
}

void __cxa_finalize(void* dso) {
  int index;

  for (;;) {
    crt_cxa_destructor_t destructor = 0;
    void* object = 0;

    crt_spin_lock(&crt_cxa_atexit_lock);
    for (index = crt_cxa_atexit_count - 1; index >= 0; --index) {
      crt_cxa_atexit_entry* entry = &crt_cxa_atexit_entries[index];
      if (!entry->called && (dso == 0 || entry->dso == dso)) {
        entry->called = 1;
        destructor = entry->destructor;
        object = entry->object;
        break;
      }
    }
    crt_spin_unlock(&crt_cxa_atexit_lock);

    if (destructor == 0) {
      return;
    }
    destructor(object);
  }
}
#endif /* CRT_CXXABI_C_OWNS_CXA_ATEXIT */

void __cxa_pure_virtual(void) {
  abort();
}

void __cxa_deleted_virtual(void) {
  abort();
}
