#include <errno.h>
#include <pthread.h>
#include <stdio.h>

static int fail(const char* message) {
  fprintf(stderr, "pthread_key_test: %s\n", message);
  return 1;
}

int main(void) {
  pthread_key_t key;
  int first = 11;
  int second = 22;

  if (pthread_key_create(0, 0) != EINVAL) {
    return fail("pthread_key_create invalid");
  }
  if (pthread_key_create(&key, 0) != 0) {
    return fail("pthread_key_create");
  }
  if (pthread_getspecific(key) != 0) {
    return fail("pthread_getspecific initial");
  }
  if (pthread_setspecific(key, &first) != 0 || pthread_getspecific(key) != &first) {
    return fail("pthread_setspecific first");
  }
  if (pthread_setspecific(key, &second) != 0 || pthread_getspecific(key) != &second) {
    return fail("pthread_setspecific second");
  }
  if (pthread_setspecific(key, 0) != 0 || pthread_getspecific(key) != 0) {
    return fail("pthread_setspecific null");
  }
  if (pthread_key_delete(key) != 0) {
    return fail("pthread_key_delete");
  }
  if (pthread_key_delete(key) != EINVAL) {
    return fail("pthread_key_delete invalid");
  }
  if (pthread_setspecific(key, &first) != EINVAL || pthread_getspecific(key) != 0) {
    return fail("deleted key access");
  }

  printf("pthread_key_test: ok\n");
  return 0;
}
