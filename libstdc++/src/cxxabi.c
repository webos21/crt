#include <stdint.h>
#include <stdlib.h>

#include <private/crt_atomic.h>

#define CRT_CXA_ATEXIT_MAX 128

#define CRT_GUARD_UNINITIALIZED 0
#define CRT_GUARD_INITIALIZING 1
#define CRT_GUARD_COMPLETE 2

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

void __cxa_pure_virtual(void) {
  abort();
}

void __cxa_deleted_virtual(void) {
  abort();
}
