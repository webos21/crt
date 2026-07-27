#include <pthread.h>
#include <stdio.h>

#define THREAD_COUNT 8

static pthread_once_t once = PTHREAD_ONCE_INIT;
static int init_count;
static int initialized_value;

static int fail(const char* message) {
  fprintf(stderr, "pthread_once_contention_test: %s\n", message);
  return 1;
}

static void init_once(void) {
  volatile int delay;

  ++init_count;
  for (delay = 0; delay < 200000; ++delay) {
  }
  initialized_value = 1234;
}

static void* worker(void* arg) {
  (void)arg;

  if (pthread_once(&once, init_once) != 0) {
    return (void*)1;
  }
  if (initialized_value != 1234) {
    return (void*)2;
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
      return fail("worker once result");
    }
  }
  if (init_count != 1 || initialized_value != 1234) {
    return fail("init count");
  }

  printf("pthread_once_contention_test: ok\n");
  return 0;
}
