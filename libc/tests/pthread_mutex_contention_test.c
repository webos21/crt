#include <pthread.h>
#include <stdio.h>

#define THREAD_COUNT 4
#define ITERATIONS 1000

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int counter;

static int fail(const char* message) {
  fprintf(stderr, "pthread_mutex_contention_test: %s\n", message);
  return 1;
}

static void* worker(void* arg) {
  int i;
  (void)arg;

  for (i = 0; i < ITERATIONS; ++i) {
    if (pthread_mutex_lock(&mutex) != 0) {
      return (void*)1;
    }
    ++counter;
    if (pthread_mutex_unlock(&mutex) != 0) {
      return (void*)2;
    }
  }

  return 0;
}

int main(void) {
  pthread_t threads[THREAD_COUNT];
  int i;

  for (i = 0; i < THREAD_COUNT; ++i) {
    if (pthread_create(&threads[i], 0, worker, 0) != 0) {
      return fail("pthread_create");
    }
  }
  for (i = 0; i < THREAD_COUNT; ++i) {
    void* result = 0;
    if (pthread_join(threads[i], &result) != 0) {
      return fail("pthread_join");
    }
    if (result != 0) {
      return fail("worker mutex operation");
    }
  }
  if (counter != THREAD_COUNT * ITERATIONS) {
    return fail("counter mismatch");
  }

  printf("pthread_mutex_contention_test: ok\n");
  return 0;
}
