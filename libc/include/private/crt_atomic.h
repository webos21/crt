#ifndef CRT_PRIVATE_ATOMIC_H
#define CRT_PRIVATE_ATOMIC_H

#include <sched.h>

typedef struct {
  int value;
} crt_atomic_int;

#define CRT_ATOMIC_INT_INIT(value) \
  { value }

static inline int crt_atomic_load_relaxed(const crt_atomic_int* atomic) {
  return __atomic_load_n(&atomic->value, __ATOMIC_RELAXED);
}

static inline int crt_atomic_load_acquire(const crt_atomic_int* atomic) {
  return __atomic_load_n(&atomic->value, __ATOMIC_ACQUIRE);
}

static inline void crt_atomic_store_release(crt_atomic_int* atomic, int value) {
  __atomic_store_n(&atomic->value, value, __ATOMIC_RELEASE);
}

static inline int crt_atomic_exchange_acquire(crt_atomic_int* atomic, int value) {
  return __atomic_exchange_n(&atomic->value, value, __ATOMIC_ACQUIRE);
}

static inline int crt_atomic_compare_exchange_acq_rel(crt_atomic_int* atomic, int* expected, int desired) {
  return __atomic_compare_exchange_n(
      &atomic->value, expected, desired, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

static inline int crt_atomic_fetch_add_acq_rel(crt_atomic_int* atomic, int value) {
  return __atomic_fetch_add(&atomic->value, value, __ATOMIC_ACQ_REL);
}

typedef struct {
  crt_atomic_int state;
} crt_spinlock;

#define CRT_SPINLOCK_INIT \
  { CRT_ATOMIC_INT_INIT(0) }

static inline void crt_spin_lock(crt_spinlock* lock) {
  while (crt_atomic_exchange_acquire(&lock->state, 1) != 0) {
    while (crt_atomic_load_relaxed(&lock->state) != 0) {
      sched_yield();
    }
  }
}

static inline void crt_spin_unlock(crt_spinlock* lock) {
  crt_atomic_store_release(&lock->state, 0);
}

typedef struct {
  crt_atomic_int state;
} crt_once;

#define CRT_ONCE_INIT \
  { CRT_ATOMIC_INT_INIT(0) }

static inline int crt_once_begin(crt_once* once) {
  int expected = 0;

  if (crt_atomic_compare_exchange_acq_rel(&once->state, &expected, 1)) {
    return 1;
  }

  while (crt_atomic_load_acquire(&once->state) != 2) {
    sched_yield();
  }
  return 0;
}

static inline void crt_once_complete(crt_once* once) {
  crt_atomic_store_release(&once->state, 2);
}

#endif
