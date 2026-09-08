#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>

static pthread_key_t key;

static int fail(const char* message) {
  fprintf(stderr, "pthread_tls_test: %s\n", message);
  return 1;
}

static void* worker(void* arg) {
  int* worker_errno_slot;
  int* worker_h_errno_slot;
  (void)arg;

  errno = 456;
  h_errno = TRY_AGAIN;
  worker_errno_slot = __errno();
  worker_h_errno_slot = __get_h_errno();
  if (worker_errno_slot == 0 || *worker_errno_slot != 456 ||
      worker_h_errno_slot == 0 || *worker_h_errno_slot != TRY_AGAIN ||
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
  int* main_h_errno_slot;

  if (pthread_key_create(&key, 0) != 0) {
    return fail("pthread_key_create");
  }
  errno = 123;
  h_errno = HOST_NOT_FOUND;
  main_errno_slot = __errno();
  main_h_errno_slot = __get_h_errno();
  if (main_errno_slot == 0 || *main_errno_slot != 123 ||
      main_h_errno_slot == 0 || *main_h_errno_slot != HOST_NOT_FOUND ||
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
      __get_h_errno() != main_h_errno_slot || h_errno != HOST_NOT_FOUND ||
      pthread_getspecific(key) != (void*)0x1111) {
    return fail("main tls preserved");
  }
  if (pthread_key_delete(key) != 0) {
    return fail("pthread_key_delete");
  }

  printf("pthread_tls_test: ok\n");
  return 0;
}
