#include <stdint.h>
#include <stdio.h>
#include <threads.h>
#include <time.h>

static int fail(const char* message) {
  fprintf(stderr, "threads_test: %s\n", message);
  return 1;
}

static once_flag once = ONCE_FLAG_INIT;
static int once_count;

static void once_init(void) {
  ++once_count;
}

static int thread_body(void* arg) {
  int* counter = (int*)arg;
  *counter += 41;
  return 7;
}

static mtx_t shared_mutex;
static int shared_counter;

static int mutex_worker(void* arg) {
  int i;
  (void)arg;

  for (i = 0; i < 500; ++i) {
    if (mtx_lock(&shared_mutex) != thrd_success) {
      return 1;
    }
    ++shared_counter;
    if (mtx_unlock(&shared_mutex) != thrd_success) {
      return 2;
    }
  }
  return 0;
}

static mtx_t cond_mutex;
static cnd_t cond;
static int cond_ready;

static int cond_waiter(void* arg) {
  (void)arg;

  if (mtx_lock(&cond_mutex) != thrd_success) {
    return 1;
  }
  while (!cond_ready) {
    if (cnd_wait(&cond, &cond_mutex) != thrd_success) {
      mtx_unlock(&cond_mutex);
      return 2;
    }
  }
  mtx_unlock(&cond_mutex);
  return 0;
}

static tss_t key;
static int destructor_calls;

static void tss_destructor(void* value) {
  (void)value;
  ++destructor_calls;
}

static int tss_worker(void* arg) {
  (void)arg;
  if (tss_set(key, (void*)(intptr_t)0x1234) != thrd_success) {
    return 1;
  }
  if (tss_get(key) != (void*)(intptr_t)0x1234) {
    return 2;
  }
  return 0;
}

int main(void) {
  thrd_t thread;
  int arg = 1;
  int result = 0;
  struct timespec ts;

  /* --- call_once --- */
  call_once(&once, once_init);
  call_once(&once, once_init);
  if (once_count != 1) {
    return fail("call_once");
  }

  /* --- thrd_create/thrd_join, int(*)(void*) return value round trip --- */
  if (thrd_create(&thread, thread_body, &arg) != thrd_success) {
    return fail("thrd_create");
  }
  if (thrd_join(thread, &result) != thrd_success || result != 7 || arg != 42) {
    return fail("thrd_join");
  }
  if (thrd_equal(thrd_current(), thrd_current()) == 0) {
    return fail("thrd_equal");
  }

  /* --- mtx_t: plain lock/unlock, trylock, timedlock, and real
   * cross-thread contention (forces the actual wait/wake path). --- */
  if (mtx_init(&shared_mutex, mtx_plain) != thrd_success) {
    return fail("mtx_init");
  }
  if (mtx_lock(&shared_mutex) != thrd_success) {
    return fail("mtx_lock");
  }
  if (mtx_trylock(&shared_mutex) != thrd_busy) {
    return fail("mtx_trylock busy");
  }
  if (mtx_unlock(&shared_mutex) != thrd_success) {
    return fail("mtx_unlock");
  }
  if (mtx_trylock(&shared_mutex) != thrd_success || mtx_unlock(&shared_mutex) != thrd_success) {
    return fail("mtx_trylock success");
  }
  if (mtx_lock(&shared_mutex) != thrd_success) {
    return fail("mtx_lock for timedlock busy check");
  }
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    return fail("clock_gettime");
  }
  ts.tv_nsec += 20000000L; /* 20ms */
  if (ts.tv_nsec >= 1000000000L) {
    ts.tv_nsec -= 1000000000L;
    ts.tv_sec += 1;
  }
  if (mtx_timedlock(&shared_mutex, &ts) != thrd_timedout) {
    return fail("mtx_timedlock timeout");
  }
  if (mtx_unlock(&shared_mutex) != thrd_success) {
    return fail("mtx_unlock after timedlock check");
  }
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    return fail("clock_gettime 2");
  }
  ts.tv_sec += 1;
  if (mtx_timedlock(&shared_mutex, &ts) != thrd_success) {
    return fail("mtx_timedlock success");
  }
  if (mtx_unlock(&shared_mutex) != thrd_success) {
    return fail("mtx_unlock after timedlock success");
  }
  {
    thrd_t threads[4];
    int i;

    shared_counter = 0;
    for (i = 0; i < 4; ++i) {
      if (thrd_create(&threads[i], mutex_worker, 0) != thrd_success) {
        return fail("mutex_worker create");
      }
    }
    for (i = 0; i < 4; ++i) {
      int worker_result = -1;
      if (thrd_join(threads[i], &worker_result) != thrd_success || worker_result != 0) {
        return fail("mutex_worker join");
      }
    }
    if (shared_counter != 4 * 500) {
      return fail("mutex contention counter");
    }
  }
  mtx_destroy(&shared_mutex);

  /* --- cnd_t: real cross-thread signal/wait --- */
  if (mtx_init(&cond_mutex, mtx_plain) != thrd_success || cnd_init(&cond) != thrd_success) {
    return fail("cond setup");
  }
  cond_ready = 0;
  if (thrd_create(&thread, cond_waiter, 0) != thrd_success) {
    return fail("cond_waiter create");
  }
  if (mtx_lock(&cond_mutex) != thrd_success) {
    return fail("cond signal lock");
  }
  cond_ready = 1;
  if (cnd_signal(&cond) != thrd_success) {
    return fail("cnd_signal");
  }
  if (mtx_unlock(&cond_mutex) != thrd_success) {
    return fail("cond signal unlock");
  }
  if (thrd_join(thread, &result) != thrd_success || result != 0) {
    return fail("cond_waiter join");
  }
  cnd_destroy(&cond);
  mtx_destroy(&cond_mutex);

  /* --- tss_t --- */
  destructor_calls = 0;
  if (tss_create(&key, tss_destructor) != thrd_success) {
    return fail("tss_create");
  }
  if (thrd_create(&thread, tss_worker, 0) != thrd_success) {
    return fail("tss_worker create");
  }
  if (thrd_join(thread, &result) != thrd_success || result != 0) {
    return fail("tss_worker join");
  }
  if (destructor_calls != 1) {
    return fail("tss destructor");
  }
  tss_delete(key);

  /* --- thrd_sleep/thrd_yield --- */
  {
    struct timespec duration;
    struct timespec remaining;

    duration.tv_sec = 0;
    duration.tv_nsec = 1000000L; /* 1ms */
    if (thrd_sleep(&duration, &remaining) != 0) {
      return fail("thrd_sleep");
    }
  }
  thrd_yield();

  printf("threads_test: ok\n");
  return 0;
}
