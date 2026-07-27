#include <errno.h>
#include <pthread.h>
#include <stdio.h>

static pthread_once_t static_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t static_mutex = PTHREAD_MUTEX_INITIALIZER;
static int once_count;

static int fail(const char* message) {
  fprintf(stderr, "pthread_basic_test: %s\n", message);
  return 1;
}

static void init_once(void) {
  ++once_count;
}

int main(void) {
  pthread_mutex_t mutex;
  pthread_t self;

  if (pthread_mutex_init(&mutex, 0) != 0) {
    return fail("pthread_mutex_init");
  }
  if (pthread_mutex_lock(&mutex) != 0) {
    return fail("pthread_mutex_lock");
  }
  if (pthread_mutex_destroy(&mutex) != EBUSY) {
    return fail("pthread_mutex_destroy busy");
  }
  if (pthread_mutex_unlock(&mutex) != 0) {
    return fail("pthread_mutex_unlock");
  }
  if (pthread_mutex_unlock(&mutex) != EPERM) {
    return fail("pthread_mutex_unlock unlocked");
  }
  if (pthread_mutex_destroy(&mutex) != 0) {
    return fail("pthread_mutex_destroy");
  }

  if (pthread_mutex_lock(&static_mutex) != 0 || pthread_mutex_unlock(&static_mutex) != 0) {
    return fail("static mutex");
  }

  if (pthread_once(&static_once, init_once) != 0 ||
      pthread_once(&static_once, init_once) != 0 ||
      once_count != 1) {
    return fail("pthread_once");
  }
  if (pthread_once(0, init_once) != EINVAL || pthread_once(&static_once, 0) != EINVAL) {
    return fail("pthread_once invalid");
  }

  self = pthread_self();
  if (self == 0 || !pthread_equal(self, pthread_self()) || pthread_equal(self, self + 1)) {
    return fail("pthread_self");
  }

  printf("pthread_basic_test: ok\n");
  return 0;
}
