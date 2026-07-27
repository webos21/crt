#include <pthread.h>
#include <stdio.h>

#include <private/crt_wait.h>

static int value;

static int fail(const char* message) {
  fprintf(stderr, "wait_test: %s\n", message);
  return 1;
}

static void* worker(void* arg) {
  (void)arg;

  while (__atomic_load_n(&value, __ATOMIC_ACQUIRE) == 0) {
    if (__crt_wait32(&value, 0) != 0) {
      return (void*)1;
    }
  }
  return &value;
}

int main(void) {
  pthread_t thread;
  void* result = 0;

  if (__crt_wait32(0, 0) == 0 ||
      __crt_wake32_one(0) == 0 ||
      __crt_wake32_all(0) == 0) {
    return fail("invalid wait args");
  }
  if (pthread_create(&thread, 0, worker, 0) != 0) {
    return fail("pthread_create");
  }
  __atomic_store_n(&value, 1, __ATOMIC_RELEASE);
  if (__crt_wake32_one(&value) != 0) {
    return fail("wake one");
  }
  if (pthread_join(thread, &result) != 0 || result != &value) {
    return fail("pthread_join");
  }
  if (__crt_wake32_all(&value) != 0) {
    return fail("wake all");
  }

  printf("wait_test: ok\n");
  return 0;
}
