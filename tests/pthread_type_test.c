#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

static int fail(const char* message) {
  fprintf(stderr, "pthread_type_test: %s\n", message);
  return 1;
}

int main(void) {
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_once_t once = PTHREAD_ONCE_INIT;

  if (sizeof(pthread_t) != sizeof(void*)) {
    return fail("pthread_t size");
  }
  if (sizeof(pthread_key_t) != sizeof(int)) {
    return fail("pthread_key_t size");
  }
  if (sizeof(pthread_once_t) != sizeof(int)) {
    return fail("pthread_once_t size");
  }
  if (sizeof(pthread_mutex_t) != sizeof(int32_t) * 10) {
    return fail("pthread_mutex_t size");
  }
  if (mutex.__private[0] != ((PTHREAD_MUTEX_NORMAL & 3) << 14)) {
    return fail("pthread mutex initializer");
  }
  if (once != 0) {
    return fail("pthread once initializer");
  }

  printf("pthread_type_test: ok\n");
  return 0;
}
