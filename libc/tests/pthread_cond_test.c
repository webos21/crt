#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int ready;
static int go_value;
static int observed_value;

static int fail(const char* message) {
  fprintf(stderr, "pthread_cond_test: %s\n", message);
  return 1;
}

static void* worker(void* arg) {
  (void)arg;

  if (pthread_mutex_lock(&mutex) != 0) {
    return (void*)1;
  }
  ready = 1;
  pthread_cond_signal(&cond);

  while (go_value == 0) {
    if (pthread_cond_wait(&cond, &mutex) != 0) {
      pthread_mutex_unlock(&mutex);
      return (void*)2;
    }
  }
  observed_value = go_value;
  if (pthread_mutex_unlock(&mutex) != 0) {
    return (void*)3;
  }
  return &observed_value;
}

int main(void) {
  pthread_cond_t local_cond;
  pthread_condattr_t cond_attr;
  struct timespec timeout;
  pthread_t thread;
  void* result = 0;

  if (pthread_cond_init(0, 0) != EINVAL ||
      pthread_cond_destroy(0) != EINVAL ||
      pthread_cond_signal(0) != EINVAL ||
      pthread_cond_broadcast(0) != EINVAL ||
      pthread_cond_wait(0, &mutex) != EINVAL ||
      pthread_cond_wait(&cond, 0) != EINVAL ||
      pthread_cond_timedwait(0, &mutex, 0) != EINVAL ||
      pthread_cond_timedwait(&cond, 0, 0) != EINVAL) {
    return fail("invalid cond args");
  }
  timeout.tv_sec = 0;
  timeout.tv_nsec = 1000000000L;
  if (pthread_cond_timedwait(&cond, &mutex, &timeout) != EINVAL) {
    return fail("invalid cond timeout");
  }
  if (pthread_condattr_init(0) != EINVAL ||
      pthread_condattr_init(&cond_attr) != 0 ||
      pthread_condattr_destroy(&cond_attr) != 0 ||
      pthread_condattr_destroy(0) != EINVAL) {
    return fail("cond attr");
  }
  if (pthread_cond_init(&local_cond, &cond_attr) != 0 ||
      pthread_cond_destroy(&local_cond) != 0) {
    return fail("local cond lifecycle");
  }

  if (clock_gettime(CLOCK_REALTIME, &timeout) != 0) {
    return fail("clock_gettime");
  }
  if (pthread_mutex_lock(&mutex) != 0) {
    return fail("timed wait lock");
  }
  timeout.tv_nsec += 1000000L;
  if (timeout.tv_nsec >= 1000000000L) {
    ++timeout.tv_sec;
    timeout.tv_nsec -= 1000000000L;
  }
  if (pthread_cond_timedwait(&cond, &mutex, &timeout) != ETIMEDOUT) {
    pthread_mutex_unlock(&mutex);
    return fail("cond timed wait timeout");
  }
  if (pthread_mutex_unlock(&mutex) != 0) {
    return fail("timed wait unlock");
  }

  // Real, end-to-end coverage for PTHREAD_COND_CLOCK_MONOTONIC -- a real,
  // confirmed-for-real bug (2026-09-03, libcrtgfx's own crtgfx_gpu_fence)
  // found this exact combination silently broken: pthread_condattr_
  // setclock(PTHREAD_COND_CLOCK_MONOTONIC) stored the requested clock,
  // but pthread_cond_timedwait() never consulted it and always treated
  // `abstime` as a real CLOCK_REALTIME deadline -- a real CLOCK_MONOTONIC
  // deadline (a small "time since boot" number) read back as already far
  // in CLOCK_REALTIME's own past, so timedwait() returned ETIMEDOUT
  // instantly instead of actually waiting. The only real way to catch
  // that regression is to measure real elapsed wall time across a real
  // timedwait() call, not just check the return code -- an instant,
  // wrong ETIMEDOUT and a real, correct, ~1ms-later ETIMEDOUT both
  // satisfy "returns ETIMEDOUT" equally.
  {
    pthread_cond_t monotonic_cond;
    pthread_condattr_t monotonic_attr;
    struct timespec deadline;
    struct timespec before;
    struct timespec after;
    int clock_id = -1;
    long elapsed_ns;

    if (pthread_condattr_init(&monotonic_attr) != 0) {
      return fail("monotonic cond attr init");
    }
    if (pthread_condattr_setclock(&monotonic_attr, PTHREAD_COND_CLOCK_MONOTONIC) != 0) {
      return fail("monotonic cond attr setclock");
    }
    if (pthread_condattr_getclock(&monotonic_attr, &clock_id) != 0 ||
        clock_id != PTHREAD_COND_CLOCK_MONOTONIC) {
      return fail("monotonic cond attr getclock");
    }
    if (pthread_cond_init(&monotonic_cond, &monotonic_attr) != 0) {
      return fail("monotonic cond init");
    }
    if (pthread_condattr_destroy(&monotonic_attr) != 0) {
      return fail("monotonic cond attr destroy");
    }

    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
      return fail("monotonic clock_gettime");
    }
    deadline.tv_nsec += 20000000L; // 20ms -- generous relative to real scheduler jitter
    if (deadline.tv_nsec >= 1000000000L) {
      ++deadline.tv_sec;
      deadline.tv_nsec -= 1000000000L;
    }

    if (pthread_mutex_lock(&mutex) != 0) {
      return fail("monotonic timed wait lock");
    }
    if (clock_gettime(CLOCK_MONOTONIC, &before) != 0) {
      pthread_mutex_unlock(&mutex);
      return fail("monotonic before clock_gettime");
    }
    if (pthread_cond_timedwait(&monotonic_cond, &mutex, &deadline) != ETIMEDOUT) {
      pthread_mutex_unlock(&mutex);
      return fail("monotonic cond timed wait timeout");
    }
    if (clock_gettime(CLOCK_MONOTONIC, &after) != 0) {
      pthread_mutex_unlock(&mutex);
      return fail("monotonic after clock_gettime");
    }
    if (pthread_mutex_unlock(&mutex) != 0) {
      return fail("monotonic timed wait unlock");
    }

    elapsed_ns = (long)(after.tv_sec - before.tv_sec) * 1000000000L + (after.tv_nsec - before.tv_nsec);
    // A real, deliberately loose lower bound (5ms against a real 20ms
    // request): the bug this guards against made this call return in
    // well under 1ms (an already-past deadline read back as expired
    // instantly), not merely "a bit early". A real, correct wait can
    // still wake a little before the nominal deadline due to real
    // scheduler/timer-resolution slack.
    if (elapsed_ns < 5000000L) {
      return fail("monotonic cond timed wait returned suspiciously fast -- PTHREAD_COND_CLOCK_MONOTONIC regression");
    }
    if (pthread_cond_destroy(&monotonic_cond) != 0) {
      return fail("monotonic cond destroy");
    }
  }

  if (pthread_mutex_lock(&mutex) != 0) {
    return fail("main lock");
  }
  if (pthread_create(&thread, 0, worker, 0) != 0) {
    pthread_mutex_unlock(&mutex);
    return fail("pthread_create");
  }
  while (!ready) {
    if (pthread_cond_wait(&cond, &mutex) != 0) {
      pthread_mutex_unlock(&mutex);
      return fail("wait ready");
    }
  }
  go_value = 123;
  if (pthread_cond_broadcast(&cond) != 0) {
    pthread_mutex_unlock(&mutex);
    return fail("broadcast");
  }
  if (pthread_mutex_unlock(&mutex) != 0) {
    return fail("main unlock");
  }
  if (pthread_join(thread, &result) != 0) {
    return fail("pthread_join");
  }
  if (result != &observed_value || observed_value != 123) {
    return fail("observed value");
  }
  if (pthread_cond_destroy(&cond) != 0) {
    return fail("static cond destroy");
  }

  printf("pthread_cond_test: ok\n");
  return 0;
}
