#include <errno.h>
#include <pthread.h>
#include <stdio.h>

static int fail(const char* message) {
  fprintf(stderr, "pthread_mutexattr_test: %s\n", message);
  return 1;
}

int main(void) {
  pthread_mutexattr_t attr;
  pthread_mutex_t mutex;
  int type = -1;

  if (pthread_mutex_trylock(0) != EINVAL) {
    return fail("pthread_mutex_trylock invalid");
  }
  if (pthread_mutexattr_init(0) != EINVAL) {
    return fail("pthread_mutexattr_init invalid");
  }
  if (pthread_mutexattr_init(&attr) != 0) {
    return fail("pthread_mutexattr_init");
  }
  if (pthread_mutexattr_gettype(&attr, &type) != 0 || type != PTHREAD_MUTEX_NORMAL) {
    return fail("default mutex type");
  }
  if (pthread_mutexattr_settype(&attr, 99) != EINVAL) {
    return fail("invalid mutex type");
  }

  if (pthread_mutex_init(&mutex, &attr) != 0) {
    return fail("normal mutex init");
  }
  if (pthread_mutex_trylock(&mutex) != 0) {
    return fail("normal trylock");
  }
  if (pthread_mutex_trylock(&mutex) != EBUSY) {
    return fail("normal trylock busy");
  }
  if (pthread_mutex_unlock(&mutex) != 0 || pthread_mutex_destroy(&mutex) != 0) {
    return fail("normal mutex cleanup");
  }

  if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0 ||
      pthread_mutexattr_gettype(&attr, &type) != 0 ||
      type != PTHREAD_MUTEX_RECURSIVE) {
    return fail("recursive attr type");
  }
  if (pthread_mutex_init(&mutex, &attr) != 0) {
    return fail("recursive mutex init");
  }
  if (pthread_mutex_lock(&mutex) != 0 || pthread_mutex_trylock(&mutex) != 0 ||
      pthread_mutex_unlock(&mutex) != 0 || pthread_mutex_unlock(&mutex) != 0) {
    return fail("recursive mutex");
  }
  if (pthread_mutex_destroy(&mutex) != 0) {
    return fail("recursive destroy");
  }

  if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK) != 0 ||
      pthread_mutex_init(&mutex, &attr) != 0) {
    return fail("errorcheck init");
  }
  if (pthread_mutex_lock(&mutex) != 0 || pthread_mutex_lock(&mutex) != EDEADLK ||
      pthread_mutex_trylock(&mutex) != EBUSY || pthread_mutex_unlock(&mutex) != 0) {
    return fail("errorcheck mutex");
  }
  if (pthread_mutex_destroy(&mutex) != 0) {
    return fail("errorcheck destroy");
  }

  if (pthread_mutexattr_destroy(&attr) != 0 || pthread_mutexattr_destroy(0) != EINVAL) {
    return fail("pthread_mutexattr_destroy");
  }

  printf("pthread_mutexattr_test: ok\n");
  return 0;
}
