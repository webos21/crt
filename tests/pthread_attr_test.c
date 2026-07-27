#include <errno.h>
#include <pthread.h>
#include <stdio.h>

static int thread_value;

static int fail(const char* message) {
  fprintf(stderr, "pthread_attr_test: %s\n", message);
  return 1;
}

static void* worker(void* arg) {
  thread_value = *(int*)arg;
  return &thread_value;
}

int main(void) {
  pthread_attr_t attr;
  pthread_t thread;
  void* result = 0;
  size_t stack_size = 0;
  int detach_state = -1;
  int input = 77;

  if (pthread_attr_init(0) != EINVAL) {
    return fail("pthread_attr_init invalid");
  }
  if (pthread_attr_init(&attr) != 0) {
    return fail("pthread_attr_init");
  }
  if (pthread_attr_getdetachstate(&attr, &detach_state) != 0 ||
      detach_state != PTHREAD_CREATE_JOINABLE) {
    return fail("default detach state");
  }
  if (pthread_attr_getstacksize(&attr, &stack_size) != 0 ||
      stack_size < PTHREAD_STACK_MIN) {
    return fail("default stack size");
  }
  if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0 ||
      pthread_attr_getdetachstate(&attr, &detach_state) != 0 ||
      detach_state != PTHREAD_CREATE_DETACHED) {
    return fail("set detached");
  }
  if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0 ||
      pthread_attr_getdetachstate(&attr, &detach_state) != 0 ||
      detach_state != PTHREAD_CREATE_JOINABLE) {
    return fail("set joinable");
  }
  if (pthread_attr_setdetachstate(&attr, 99) != EINVAL) {
    return fail("invalid detach state");
  }
  if (pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN - 1) != EINVAL) {
    return fail("invalid stack size");
  }
  if (pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN * 2) != 0 ||
      pthread_attr_getstacksize(&attr, &stack_size) != 0 ||
      stack_size != PTHREAD_STACK_MIN * 2) {
    return fail("set stack size");
  }
  if (pthread_create(&thread, &attr, worker, &input) != 0) {
    return fail("pthread_create with attr");
  }
  if (pthread_join(thread, &result) != 0 || result != &thread_value || thread_value != 77) {
    return fail("pthread_join with attr");
  }
  if (pthread_attr_destroy(&attr) != 0 || pthread_attr_destroy(0) != EINVAL) {
    return fail("pthread_attr_destroy");
  }

  printf("pthread_attr_test: ok\n");
  return 0;
}
