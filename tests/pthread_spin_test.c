#include <errno.h>
#include <pthread.h>
#include <stdio.h>

#define THREAD_COUNT 4
#define ITERATIONS 1000

static pthread_spinlock_t spin;
static int counter;

static int fail(const char* message) {
  fprintf(stderr, "pthread_spin_test: %s\n", message);
  return 1;
}

static void* worker(void* arg) {
  int i;
  (void)arg;

  for (i = 0; i < ITERATIONS; ++i) {
    if (pthread_spin_lock(&spin) != 0) {
      return (void*)1;
    }
    ++counter;
    if (pthread_spin_unlock(&spin) != 0) {
      return (void*)2;
    }
  }
  return 0;
}

int main(void) {
  pthread_t threads[THREAD_COUNT];
  int i;

  if (pthread_spin_init(0, PTHREAD_PROCESS_PRIVATE) != EINVAL ||
      pthread_spin_destroy(0) != EINVAL ||
      pthread_spin_lock(0) != EINVAL ||
      pthread_spin_trylock(0) != EINVAL ||
      pthread_spin_unlock(0) != EINVAL) {
    return fail("invalid spin arguments");
  }
  if (pthread_spin_init(&spin, 99) != EINVAL ||
      pthread_spin_init(&spin, PTHREAD_PROCESS_SHARED) != ENOTSUP) {
    return fail("spin pshared");
  }
  if (pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE) != 0) {
    return fail("spin init");
  }
  if (pthread_spin_trylock(&spin) != 0 ||
      pthread_spin_trylock(&spin) != EBUSY ||
      pthread_spin_destroy(&spin) != EBUSY ||
      pthread_spin_unlock(&spin) != 0 ||
      pthread_spin_unlock(&spin) != EPERM) {
    return fail("spin basic locking");
  }

  for (i = 0; i < THREAD_COUNT; ++i) {
    if (pthread_create(&threads[i], 0, worker, 0) != 0) {
      return fail("pthread_create");
    }
  }
  for (i = 0; i < THREAD_COUNT; ++i) {
    void* result = 0;
    if (pthread_join(threads[i], &result) != 0 || result != 0) {
      return fail("pthread_join");
    }
  }
  if (counter != THREAD_COUNT * ITERATIONS) {
    return fail("counter");
  }
  if (pthread_spin_destroy(&spin) != 0) {
    return fail("spin destroy");
  }

  printf("pthread_spin_test: ok\n");
  return 0;
}
