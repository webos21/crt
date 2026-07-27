#include <errno.h>
#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int shared_value;

static int fail(const char* message) {
  fprintf(stderr, "pthread_thread_test: %s\n", message);
  return 1;
}

static void* worker(void* arg) {
  int* value = (int*)arg;

  if (pthread_mutex_lock(&lock) != 0) {
    return (void*)1;
  }
  shared_value = *value + 1;
  if (pthread_mutex_unlock(&lock) != 0) {
    return (void*)2;
  }

  return &shared_value;
}

int main(void) {
  pthread_t thread;
  void* result = 0;
  int input = 41;
  int create_result;

  create_result = pthread_create(&thread, 0, worker, &input);
  if (create_result != 0) {
    return fail("pthread_create");
  }
  if (pthread_join(thread, &result) != 0) {
    return fail("pthread_join");
  }
  if (result != &shared_value || shared_value != 42) {
    return fail("thread result");
  }
  if (pthread_join(0, 0) != EINVAL) {
    return fail("pthread_join invalid");
  }

  printf("pthread_thread_test: ok\n");
  return 0;
}
