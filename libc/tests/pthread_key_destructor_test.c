#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

static pthread_key_t key;
static int destructor_calls;
static int first_result = 7;
static int exit_result = 9;

static int fail(const char* message) {
  fprintf(stderr, "pthread_key_destructor_test: %s\n", message);
  return 1;
}

static void destructor(void* value) {
  intptr_t pass = (intptr_t)value;

  __atomic_fetch_add(&destructor_calls, 1, __ATOMIC_RELAXED);
  if (pass < 3) {
    pthread_setspecific(key, (void*)(pass + 1));
  }
}

static void* return_worker(void* arg) {
  (void)arg;

  if (pthread_setspecific(key, (void*)1) != 0) {
    return 0;
  }
  return &first_result;
}

static void* exit_worker(void* arg) {
  (void)arg;

  if (pthread_setspecific(key, (void*)1) != 0) {
    pthread_exit(0);
  }
  pthread_exit(&exit_result);
}

int main(void) {
  pthread_t thread;
  void* result = 0;

  if (pthread_key_create(&key, destructor) != 0) {
    return fail("pthread_key_create");
  }

  if (pthread_create(&thread, 0, return_worker, 0) != 0 ||
      pthread_join(thread, &result) != 0 ||
      result != &first_result) {
    return fail("return worker");
  }
  if (__atomic_load_n(&destructor_calls, __ATOMIC_RELAXED) != 3) {
    return fail("return destructors");
  }

  if (pthread_create(&thread, 0, exit_worker, 0) != 0 ||
      pthread_join(thread, &result) != 0 ||
      result != &exit_result) {
    return fail("exit worker");
  }
  if (__atomic_load_n(&destructor_calls, __ATOMIC_RELAXED) != 6) {
    return fail("exit destructors");
  }

  if (pthread_key_delete(key) != 0) {
    return fail("pthread_key_delete");
  }

  printf("pthread_key_destructor_test: ok\n");
  return 0;
}
