#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <time.h>

static sem_t g_sem;
static int g_observed;

static int fail(const char* message) {
  fprintf(stderr, "semaphore_test: %s\n", message);
  return 1;
}

static void* worker(void* arg) {
  (void)arg;

  if (sem_wait(&g_sem) != 0) {
    return (void*)1;
  }
  g_observed = 42;
  return (void*)0;
}

int main(void) {
  sem_t sem;
  pthread_t thread;
  void* result = (void*)99;
  int value = -1;
  struct timespec ts;

  /* Argument validation. */
  if (sem_init(0, 0, 0) == 0 || errno != EINVAL) {
    return fail("sem_init null accepted");
  }
  if (sem_wait(0) == 0 || errno != EINVAL) {
    return fail("sem_wait null accepted");
  }
  if (sem_post(0) == 0 || errno != EINVAL) {
    return fail("sem_post null accepted");
  }
  if (sem_trywait(0) == 0 || errno != EINVAL) {
    return fail("sem_trywait null accepted");
  }
  if (sem_getvalue(0, &value) == 0 || errno != EINVAL) {
    return fail("sem_getvalue null accepted");
  }
  if (sem_init(&sem, 0, (unsigned int)SEM_VALUE_MAX + 1u) == 0 || errno != EINVAL) {
    return fail("sem_init overflow accepted");
  }
  if (sem_init(&sem, 1, 0) == 0 || errno != ENOTSUP) {
    return fail("sem_init pshared should be ENOTSUP");
  }

  /* Basic init/post/wait/trywait/getvalue round trip, no other thread. */
  if (sem_init(&sem, 0, 0) != 0) {
    return fail("sem_init");
  }
  if (sem_trywait(&sem) == 0 || errno != EAGAIN) {
    return fail("sem_trywait on empty semaphore should be EAGAIN");
  }
  if (sem_post(&sem) != 0) {
    return fail("sem_post");
  }
  if (sem_getvalue(&sem, &value) != 0 || value != 1) {
    return fail("sem_getvalue after post");
  }
  if (sem_trywait(&sem) != 0) {
    return fail("sem_trywait after post");
  }
  if (sem_getvalue(&sem, &value) != 0 || value != 0) {
    return fail("sem_getvalue after trywait");
  }

  /* sem_timedwait must actually time out on an empty semaphore. */
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    return fail("clock_gettime");
  }
  ts.tv_nsec += 100000000L; /* +100ms */
  if (ts.tv_nsec >= 1000000000L) {
    ts.tv_nsec -= 1000000000L;
    ts.tv_sec += 1;
  }
  if (sem_timedwait(&sem, &ts) == 0 || errno != ETIMEDOUT) {
    return fail("sem_timedwait on empty semaphore should time out");
  }

  if (sem_destroy(&sem) != 0) {
    return fail("sem_destroy");
  }

  /* Real cross-thread wait/post: a worker blocks in sem_wait() until this
   * thread posts, proving the private futex/wait-address wakeup path
   * actually works, not just the CAS fast path above. */
  if (sem_init(&g_sem, 0, 0) != 0) {
    return fail("sem_init (cross-thread)");
  }
  if (pthread_create(&thread, 0, worker, 0) != 0) {
    return fail("pthread_create");
  }
  if (sem_post(&g_sem) != 0) {
    return fail("sem_post (cross-thread)");
  }
  if (pthread_join(thread, &result) != 0) {
    return fail("pthread_join");
  }
  if (result != (void*)0) {
    return fail("worker reported failure");
  }
  if (g_observed != 42) {
    return fail("worker did not observe the post");
  }
  if (sem_destroy(&g_sem) != 0) {
    return fail("sem_destroy (cross-thread)");
  }

  /* Named semaphores: real Bionic declares but never supports these. */
  if (sem_open("crt_semaphore_test", 0) != SEM_FAILED || errno != ENOSYS) {
    return fail("sem_open should be ENOSYS");
  }
  if (sem_close(0) == 0 || errno != ENOSYS) {
    return fail("sem_close should be ENOSYS");
  }
  if (sem_unlink("crt_semaphore_test") == 0 || errno != ENOSYS) {
    return fail("sem_unlink should be ENOSYS");
  }

  printf("semaphore_test: ok\n");
  return 0;
}
