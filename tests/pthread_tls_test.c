#include <errno.h>
#include <pthread.h>
#include <stdio.h>

static pthread_key_t key;

static int fail(const char* message) {
  fprintf(stderr, "pthread_tls_test: %s\n", message);
  return 1;
}

static void* worker(void* arg) {
  int* worker_errno_slot;
  (void)arg;

  errno = 456;
  worker_errno_slot = __errno();
  if (worker_errno_slot == 0 || *worker_errno_slot != 456 ||
      pthread_setspecific(key, (void*)0x2222) != 0 ||
      pthread_getspecific(key) != (void*)0x2222) {
    return (void*)1;
  }
  return 0;
}

int main(void) {
  pthread_t thread;
  void* result = 0;
  int* main_errno_slot;

  if (pthread_key_create(&key, 0) != 0) {
    return fail("pthread_key_create");
  }
  errno = 123;
  main_errno_slot = __errno();
  if (main_errno_slot == 0 || *main_errno_slot != 123 ||
      pthread_setspecific(key, (void*)0x1111) != 0 ||
      pthread_getspecific(key) != (void*)0x1111) {
    return fail("main tls setup");
  }
  if (pthread_create(&thread, 0, worker, 0) != 0) {
    return fail("pthread_create");
  }
  if (pthread_join(thread, &result) != 0 || result != 0) {
    return fail("pthread_join");
  }
  if (__errno() != main_errno_slot || errno != 123 ||
      pthread_getspecific(key) != (void*)0x1111) {
    return fail("main tls preserved");
  }
  if (pthread_key_delete(key) != 0) {
    return fail("pthread_key_delete");
  }

  printf("pthread_tls_test: ok\n");
  return 0;
}
