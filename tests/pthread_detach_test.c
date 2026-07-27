#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>

static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static int detached_value;

static int fail(const char* message) {
  fprintf(stderr, "pthread_detach_test: %s\n", message);
  return 1;
}

static void* worker(void* arg) {
  int* value = (int*)arg;

  if (pthread_mutex_lock(&gate) != 0) {
    return (void*)1;
  }
  detached_value = *value;
  if (pthread_mutex_unlock(&gate) != 0) {
    return (void*)2;
  }
  return 0;
}

static int wait_for_value(int expected) {
  int i;

  for (i = 0; i < 100000; ++i) {
    if (detached_value == expected) {
      return 0;
    }
    sched_yield();
  }
  return 1;
}

int main(void) {
  pthread_attr_t attr;
  pthread_t thread;
  int first = 31;
  int second = 32;

  if (pthread_detach(0) != EINVAL) {
    return fail("pthread_detach invalid");
  }

  if (pthread_mutex_lock(&gate) != 0) {
    return fail("gate lock");
  }
  if (pthread_create(&thread, 0, worker, &first) != 0) {
    return fail("pthread_create");
  }
  if (pthread_detach(thread) != 0) {
    return fail("pthread_detach");
  }
  if (pthread_join(thread, 0) != EINVAL) {
    return fail("join detached");
  }
  if (pthread_mutex_unlock(&gate) != 0) {
    return fail("gate unlock");
  }
  if (wait_for_value(first) != 0) {
    return fail("detached worker");
  }

  if (pthread_attr_init(&attr) != 0 ||
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
    return fail("detached attr");
  }
  detached_value = 0;
  if (pthread_create(&thread, &attr, worker, &second) != 0) {
    return fail("pthread_create detached attr");
  }
  if (wait_for_value(second) != 0) {
    return fail("detached attr worker");
  }
  pthread_attr_destroy(&attr);

  printf("pthread_detach_test: ok\n");
  return 0;
}
