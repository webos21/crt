#include <errno.h>
#include <pthread.h>
#include <stdio.h>

/*
 * PTHREAD_PROCESS_SHARED is only real, cross-process-capable on Linux
 * (non-private futex operations) and macOS (os_sync_wait_on_address's
 * documented SHARED flag -- reasoned carefully but not yet verified on
 * real Apple hardware in this Windows-only dev session; see the comment
 * above the shared __crt_wait32/__crt_wake32 variants in libc/src/wait.c).
 * WaitOnAddress/WakeByAddressSingle/WakeByAddressAll are documented by
 * Microsoft as same-process-only with no cross-process capability at all,
 * so pshared mutex/rwlock/cond/barrier objects stay ENOTSUP on Windows --
 * this mirrors the CRT_PSHARED_SUPPORTED gate in libc/src/pthread.c.
 *
 * pthread_spin_* is the one exception: it is pure __atomic_* builtins with
 * no OS wait/wake call at all, so it supports PTHREAD_PROCESS_SHARED for
 * real on every host, including Windows -- that is covered separately by
 * pthread_spin_test.c, not here.
 */
#if defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_MACOS)
#define CRT_PSHARED_SUPPORTED 1
#else
#define CRT_PSHARED_SUPPORTED 0
#endif

#define THREAD_COUNT 4
#define ITERATIONS 200

static int fail(const char* message) {
  fprintf(stderr, "pthread_process_shared_test: %s\n", message);
  return 1;
}

/* --- mutexattr / mutex --- */

#if CRT_PSHARED_SUPPORTED
static pthread_mutex_t shared_mutex;
static int shared_mutex_counter;

static void* mutex_worker(void* arg) {
  int i;
  (void)arg;

  for (i = 0; i < ITERATIONS; ++i) {
    if (pthread_mutex_lock(&shared_mutex) != 0) {
      return (void*)1;
    }
    ++shared_mutex_counter;
    if (pthread_mutex_unlock(&shared_mutex) != 0) {
      return (void*)2;
    }
  }
  return 0;
}
#endif

static int test_mutex(void) {
  pthread_mutexattr_t attr;
  int pshared = -1;

  if (pthread_mutexattr_init(&attr) != 0) {
    return fail("mutexattr_init");
  }
  if (pthread_mutexattr_getpshared(&attr, &pshared) != 0 || pshared != PTHREAD_PROCESS_PRIVATE) {
    return fail("mutexattr default pshared");
  }
  if (pthread_mutexattr_setpshared(&attr, 99) != EINVAL) {
    return fail("mutexattr setpshared invalid");
  }

#if CRT_PSHARED_SUPPORTED
  if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
    return fail("mutexattr setpshared shared");
  }
  if (pthread_mutexattr_getpshared(&attr, &pshared) != 0 || pshared != PTHREAD_PROCESS_SHARED) {
    return fail("mutexattr getpshared shared");
  }
  if (pthread_mutex_init(&shared_mutex, &attr) != 0) {
    return fail("shared mutex init");
  }
  if (pthread_mutex_lock(&shared_mutex) != 0 || pthread_mutex_trylock(&shared_mutex) != EBUSY ||
      pthread_mutex_unlock(&shared_mutex) != 0) {
    return fail("shared mutex basic lock");
  }
  {
    pthread_t threads[THREAD_COUNT];
    int i;

    shared_mutex_counter = 0;
    for (i = 0; i < THREAD_COUNT; ++i) {
      if (pthread_create(&threads[i], 0, mutex_worker, 0) != 0) {
        return fail("mutex_worker create");
      }
    }
    for (i = 0; i < THREAD_COUNT; ++i) {
      void* result = 0;
      if (pthread_join(threads[i], &result) != 0 || result != 0) {
        return fail("mutex_worker join");
      }
    }
    if (shared_mutex_counter != THREAD_COUNT * ITERATIONS) {
      return fail("shared mutex contention counter");
    }
  }
  if (pthread_mutex_destroy(&shared_mutex) != 0) {
    return fail("shared mutex destroy");
  }
  if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_PRIVATE) != 0) {
    return fail("mutexattr setpshared back to private");
  }
#else
  if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != ENOTSUP) {
    return fail("mutexattr setpshared should be ENOTSUP");
  }
#endif

  if (pthread_mutexattr_destroy(&attr) != 0) {
    return fail("mutexattr_destroy");
  }
  return 0;
}

/* --- rwlockattr / rwlock --- */

#if CRT_PSHARED_SUPPORTED
static pthread_rwlock_t shared_rwlock;
static int shared_rwlock_counter;

static void* rwlock_writer(void* arg) {
  int i;
  (void)arg;

  for (i = 0; i < ITERATIONS; ++i) {
    if (pthread_rwlock_wrlock(&shared_rwlock) != 0) {
      return (void*)1;
    }
    ++shared_rwlock_counter;
    if (pthread_rwlock_unlock(&shared_rwlock) != 0) {
      return (void*)2;
    }
  }
  return 0;
}
#endif

static int test_rwlock(void) {
  pthread_rwlockattr_t attr;
  int pshared = -1;

  if (pthread_rwlockattr_init(&attr) != 0) {
    return fail("rwlockattr_init");
  }
  if (pthread_rwlockattr_getpshared(&attr, &pshared) != 0 || pshared != PTHREAD_PROCESS_PRIVATE) {
    return fail("rwlockattr default pshared");
  }
  if (pthread_rwlockattr_setpshared(&attr, 99) != EINVAL) {
    return fail("rwlockattr setpshared invalid");
  }

#if CRT_PSHARED_SUPPORTED
  if (pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
    return fail("rwlockattr setpshared shared");
  }
  if (pthread_rwlockattr_getpshared(&attr, &pshared) != 0 || pshared != PTHREAD_PROCESS_SHARED) {
    return fail("rwlockattr getpshared shared");
  }
  if (pthread_rwlock_init(&shared_rwlock, &attr) != 0) {
    return fail("shared rwlock init");
  }
  if (pthread_rwlock_rdlock(&shared_rwlock) != 0 || pthread_rwlock_unlock(&shared_rwlock) != 0) {
    return fail("shared rwlock basic rdlock");
  }
  {
    pthread_t threads[THREAD_COUNT];
    int i;

    shared_rwlock_counter = 0;
    for (i = 0; i < THREAD_COUNT; ++i) {
      if (pthread_create(&threads[i], 0, rwlock_writer, 0) != 0) {
        return fail("rwlock_writer create");
      }
    }
    for (i = 0; i < THREAD_COUNT; ++i) {
      void* result = 0;
      if (pthread_join(threads[i], &result) != 0 || result != 0) {
        return fail("rwlock_writer join");
      }
    }
    if (shared_rwlock_counter != THREAD_COUNT * ITERATIONS) {
      return fail("shared rwlock contention counter");
    }
  }
  if (pthread_rwlock_destroy(&shared_rwlock) != 0) {
    return fail("shared rwlock destroy");
  }
#else
  if (pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != ENOTSUP) {
    return fail("rwlockattr setpshared should be ENOTSUP");
  }
#endif

  if (pthread_rwlockattr_destroy(&attr) != 0) {
    return fail("rwlockattr_destroy");
  }
  return 0;
}

/* --- barrierattr / barrier --- */

#if CRT_PSHARED_SUPPORTED
static pthread_barrier_t shared_barrier;
static int shared_barrier_serial_count;
static pthread_mutex_t serial_count_mutex = PTHREAD_MUTEX_INITIALIZER;

static void* barrier_worker(void* arg) {
  int result;
  (void)arg;

  result = pthread_barrier_wait(&shared_barrier);
  if (result != 0 && result != PTHREAD_BARRIER_SERIAL_THREAD) {
    return (void*)1;
  }
  if (result == PTHREAD_BARRIER_SERIAL_THREAD) {
    pthread_mutex_lock(&serial_count_mutex);
    ++shared_barrier_serial_count;
    pthread_mutex_unlock(&serial_count_mutex);
  }
  return 0;
}
#endif

static int test_barrier(void) {
  pthread_barrierattr_t attr;
  int pshared = -1;

  if (pthread_barrierattr_init(&attr) != 0) {
    return fail("barrierattr_init");
  }
  if (pthread_barrierattr_getpshared(&attr, &pshared) != 0 || pshared != PTHREAD_PROCESS_PRIVATE) {
    return fail("barrierattr default pshared");
  }
  if (pthread_barrierattr_setpshared(&attr, 99) != EINVAL) {
    return fail("barrierattr setpshared invalid");
  }

#if CRT_PSHARED_SUPPORTED
  if (pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
    return fail("barrierattr setpshared shared");
  }
  if (pthread_barrierattr_getpshared(&attr, &pshared) != 0 || pshared != PTHREAD_PROCESS_SHARED) {
    return fail("barrierattr getpshared shared");
  }
  if (pthread_barrier_init(&shared_barrier, &attr, THREAD_COUNT) != 0) {
    return fail("shared barrier init");
  }
  {
    pthread_t threads[THREAD_COUNT];
    int i;

    shared_barrier_serial_count = 0;
    for (i = 0; i < THREAD_COUNT; ++i) {
      if (pthread_create(&threads[i], 0, barrier_worker, 0) != 0) {
        return fail("barrier_worker create");
      }
    }
    for (i = 0; i < THREAD_COUNT; ++i) {
      void* result = 0;
      if (pthread_join(threads[i], &result) != 0 || result != 0) {
        return fail("barrier_worker join");
      }
    }
    if (shared_barrier_serial_count != 1) {
      return fail("shared barrier serial count");
    }
  }
  if (pthread_barrier_destroy(&shared_barrier) != 0) {
    return fail("shared barrier destroy");
  }
#else
  if (pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != ENOTSUP) {
    return fail("barrierattr setpshared should be ENOTSUP");
  }
#endif

  if (pthread_barrierattr_destroy(&attr) != 0) {
    return fail("barrierattr_destroy");
  }
  return 0;
}

/* --- condattr / cond --- */

#if CRT_PSHARED_SUPPORTED
static pthread_cond_t shared_cond;
static pthread_mutex_t shared_cond_mutex;
static int shared_cond_ready;

static void* cond_waiter(void* arg) {
  (void)arg;

  if (pthread_mutex_lock(&shared_cond_mutex) != 0) {
    return (void*)1;
  }
  while (!shared_cond_ready) {
    if (pthread_cond_wait(&shared_cond, &shared_cond_mutex) != 0) {
      pthread_mutex_unlock(&shared_cond_mutex);
      return (void*)2;
    }
  }
  pthread_mutex_unlock(&shared_cond_mutex);
  return 0;
}
#endif

static int test_cond(void) {
  pthread_condattr_t attr;
  int pshared = -1;
  int clock_id = -1;

  if (pthread_condattr_init(&attr) != 0) {
    return fail("condattr_init");
  }
  if (pthread_condattr_getpshared(&attr, &pshared) != 0 || pshared != PTHREAD_PROCESS_PRIVATE) {
    return fail("condattr default pshared");
  }
  /* Setting the clock must not disturb the (still-private) pshared bit. */
  if (pthread_condattr_setclock(&attr, PTHREAD_COND_CLOCK_MONOTONIC) != 0) {
    return fail("condattr setclock");
  }
  if (pthread_condattr_getpshared(&attr, &pshared) != 0 || pshared != PTHREAD_PROCESS_PRIVATE) {
    return fail("condattr pshared survives setclock");
  }
  if (pthread_condattr_getclock(&attr, &clock_id) != 0 || clock_id != PTHREAD_COND_CLOCK_MONOTONIC) {
    return fail("condattr getclock after setclock");
  }
  if (pthread_condattr_setpshared(&attr, 99) != EINVAL) {
    return fail("condattr setpshared invalid");
  }

#if CRT_PSHARED_SUPPORTED
  if (pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
    return fail("condattr setpshared shared");
  }
  /* Setting pshared must not disturb the clock id set above. */
  if (pthread_condattr_getclock(&attr, &clock_id) != 0 || clock_id != PTHREAD_COND_CLOCK_MONOTONIC) {
    return fail("condattr clock survives setpshared");
  }
  if (pthread_condattr_getpshared(&attr, &pshared) != 0 || pshared != PTHREAD_PROCESS_SHARED) {
    return fail("condattr getpshared shared");
  }
  if (pthread_cond_init(&shared_cond, &attr) != 0) {
    return fail("shared cond init");
  }
  if (pthread_mutex_init(&shared_cond_mutex, 0) != 0) {
    return fail("shared cond mutex init");
  }
  shared_cond_ready = 0;
  {
    pthread_t waiter;
    void* result = 0;

    if (pthread_create(&waiter, 0, cond_waiter, 0) != 0) {
      return fail("cond_waiter create");
    }
    if (pthread_mutex_lock(&shared_cond_mutex) != 0) {
      return fail("cond signal lock");
    }
    shared_cond_ready = 1;
    if (pthread_cond_signal(&shared_cond) != 0) {
      return fail("cond signal");
    }
    if (pthread_mutex_unlock(&shared_cond_mutex) != 0) {
      return fail("cond signal unlock");
    }
    if (pthread_join(waiter, &result) != 0 || result != 0) {
      return fail("cond_waiter join");
    }
  }
  if (pthread_mutex_destroy(&shared_cond_mutex) != 0) {
    return fail("shared cond mutex destroy");
  }
  if (pthread_cond_destroy(&shared_cond) != 0) {
    return fail("shared cond destroy");
  }
#else
  if (pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != ENOTSUP) {
    return fail("condattr setpshared should be ENOTSUP");
  }
#endif

  if (pthread_condattr_destroy(&attr) != 0) {
    return fail("condattr_destroy");
  }
  return 0;
}

int main(void) {
  if (test_mutex() != 0) {
    return 1;
  }
  if (test_rwlock() != 0) {
    return 1;
  }
  if (test_barrier() != 0) {
    return 1;
  }
  if (test_cond() != 0) {
    return 1;
  }

  printf("pthread_process_shared_test: ok\n");
  return 0;
}
