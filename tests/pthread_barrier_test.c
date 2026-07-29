#include <errno.h>
#include <pthread.h>
#include <stdio.h>

#define THREAD_COUNT 4

static pthread_barrier_t barrier;
static int before_barrier;
static int after_barrier;
static int serial_count;

static int fail(const char* message) {
  fprintf(stderr, "pthread_barrier_test: %s\n", message);
  return 1;
}

static void* worker(void* arg) {
  (void)arg;

  __atomic_fetch_add(&before_barrier, 1, __ATOMIC_ACQ_REL);
  {
    int result = pthread_barrier_wait(&barrier);
    if (result == PTHREAD_BARRIER_SERIAL_THREAD) {
      __atomic_fetch_add(&serial_count, 1, __ATOMIC_ACQ_REL);
    } else if (result != 0) {
      return (void*)1;
    }
  }
  if (__atomic_load_n(&before_barrier, __ATOMIC_ACQUIRE) != THREAD_COUNT) {
    return (void*)2;
  }
  __atomic_fetch_add(&after_barrier, 1, __ATOMIC_ACQ_REL);
  return 0;
}

int main(void) {
  pthread_barrierattr_t attr;
  pthread_t threads[THREAD_COUNT];
  int pshared = -1;
  int i;

  if (pthread_barrierattr_init(0) != EINVAL ||
      pthread_barrierattr_destroy(0) != EINVAL ||
      pthread_barrierattr_getpshared(0, &pshared) != EINVAL ||
      pthread_barrierattr_setpshared(0, PTHREAD_PROCESS_PRIVATE) != EINVAL) {
    return fail("barrierattr invalid");
  }
  if (pthread_barrierattr_init(&attr) != 0 ||
      pthread_barrierattr_getpshared(&attr, &pshared) != 0 ||
      pshared != PTHREAD_PROCESS_PRIVATE ||
      pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != ENOTSUP ||
      pthread_barrierattr_setpshared(&attr, 99) != EINVAL ||
      pthread_barrierattr_destroy(&attr) != 0) {
    return fail("barrierattr pshared");
  }
  if (pthread_barrier_init(0, 0, 1) != EINVAL ||
      pthread_barrier_init(&barrier, 0, 0) != EINVAL ||
      pthread_barrier_wait(0) != EINVAL ||
      pthread_barrier_destroy(0) != EINVAL) {
    return fail("barrier invalid");
  }
  if (pthread_barrier_init(&barrier, 0, THREAD_COUNT) != 0) {
    return fail("barrier init");
  }
  if (pthread_barrier_destroy(&barrier) != 0) {
    return fail("barrier destroy empty");
  }
  if (pthread_barrier_init(&barrier, 0, THREAD_COUNT) != 0) {
    return fail("barrier reinit");
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
  if (after_barrier != THREAD_COUNT) {
    return fail("after barrier");
  }

  if (serial_count != 1) {
    return fail("barrier serial count");
  }
  if (pthread_barrier_destroy(&barrier) != 0) {
    return fail("barrier destroy");
  }

  printf("pthread_barrier_test: ok\n");
  return 0;
}
