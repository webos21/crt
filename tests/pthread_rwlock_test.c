#include <errno.h>
#include <pthread.h>
#include <stdio.h>

static int fail(const char* message) {
  fprintf(stderr, "pthread_rwlock_test: %s\n", message);
  return 1;
}

int main(void) {
  pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
  pthread_rwlock_t dynamic_rwlock;
  pthread_rwlockattr_t attr;

  if (pthread_rwlock_init(0, 0) != EINVAL ||
      pthread_rwlock_destroy(0) != EINVAL ||
      pthread_rwlock_rdlock(0) != EINVAL ||
      pthread_rwlock_tryrdlock(0) != EINVAL ||
      pthread_rwlock_wrlock(0) != EINVAL ||
      pthread_rwlock_trywrlock(0) != EINVAL ||
      pthread_rwlock_unlock(0) != EINVAL) {
    return fail("invalid rwlock arguments");
  }

  if (pthread_rwlockattr_init(0) != EINVAL ||
      pthread_rwlockattr_init(&attr) != 0 ||
      pthread_rwlockattr_destroy(&attr) != 0 ||
      pthread_rwlockattr_destroy(0) != EINVAL) {
    return fail("rwlock attr");
  }

  if (pthread_rwlock_init(&dynamic_rwlock, &attr) != 0 ||
      pthread_rwlock_destroy(&dynamic_rwlock) != 0) {
    return fail("dynamic rwlock");
  }

  if (pthread_rwlock_rdlock(&rwlock) != 0 ||
      pthread_rwlock_tryrdlock(&rwlock) != 0 ||
      pthread_rwlock_trywrlock(&rwlock) != EBUSY ||
      pthread_rwlock_destroy(&rwlock) != EBUSY ||
      pthread_rwlock_unlock(&rwlock) != 0 ||
      pthread_rwlock_unlock(&rwlock) != 0) {
    return fail("reader lock");
  }

  if (pthread_rwlock_wrlock(&rwlock) != 0 ||
      pthread_rwlock_tryrdlock(&rwlock) != EBUSY ||
      pthread_rwlock_trywrlock(&rwlock) != EBUSY ||
      pthread_rwlock_destroy(&rwlock) != EBUSY ||
      pthread_rwlock_unlock(&rwlock) != 0 ||
      pthread_rwlock_unlock(&rwlock) != EPERM) {
    return fail("writer lock");
  }

  if (pthread_rwlock_destroy(&rwlock) != 0) {
    return fail("destroy unlocked");
  }

  printf("pthread_rwlock_test: ok\n");
  return 0;
}
