#include <errno.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <threads.h>

static int translate_result(int pthread_errno) {
  if (pthread_errno == 0) {
    return thrd_success;
  }
  if (pthread_errno == ETIMEDOUT) {
    return thrd_timedout;
  }
  if (pthread_errno == EBUSY) {
    return thrd_busy;
  }
  return thrd_error;
}

/* --- call_once --- */

void call_once(once_flag* flag, void (*func)(void)) {
  pthread_once(flag, func);
}

/* --- cnd_t --- */

int cnd_init(cnd_t* cond) {
  if (cond == 0) {
    return thrd_error;
  }
  return pthread_cond_init(cond, 0) == 0 ? thrd_success : thrd_error;
}

void cnd_destroy(cnd_t* cond) {
  if (cond != 0) {
    pthread_cond_destroy(cond);
  }
}

int cnd_signal(cnd_t* cond) {
  if (cond == 0) {
    return thrd_error;
  }
  return pthread_cond_signal(cond) == 0 ? thrd_success : thrd_error;
}

int cnd_broadcast(cnd_t* cond) {
  if (cond == 0) {
    return thrd_error;
  }
  return pthread_cond_broadcast(cond) == 0 ? thrd_success : thrd_error;
}

int cnd_wait(cnd_t* cond, mtx_t* mtx) {
  if (cond == 0 || mtx == 0) {
    return thrd_error;
  }
  return pthread_cond_wait(cond, mtx) == 0 ? thrd_success : thrd_error;
}

int cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts) {
  if (cond == 0 || mtx == 0 || ts == 0) {
    return thrd_error;
  }
  return translate_result(pthread_cond_timedwait(cond, mtx, ts));
}

/* --- mtx_t --- */

int mtx_init(mtx_t* mtx, int type) {
  pthread_mutexattr_t attr;
  int mutex_type = (type & mtx_recursive) ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL;
  int result;

  if (mtx == 0) {
    return thrd_error;
  }
  if (pthread_mutexattr_init(&attr) != 0) {
    return thrd_error;
  }
  pthread_mutexattr_settype(&attr, mutex_type);
  result = pthread_mutex_init(mtx, &attr);
  pthread_mutexattr_destroy(&attr);
  return result == 0 ? thrd_success : thrd_error;
}

void mtx_destroy(mtx_t* mtx) {
  if (mtx != 0) {
    pthread_mutex_destroy(mtx);
  }
}

int mtx_lock(mtx_t* mtx) {
  if (mtx == 0) {
    return thrd_error;
  }
  return pthread_mutex_lock(mtx) == 0 ? thrd_success : thrd_error;
}

int mtx_timedlock(mtx_t* mtx, const struct timespec* ts) {
  if (mtx == 0 || ts == 0) {
    return thrd_error;
  }
  return translate_result(pthread_mutex_timedlock(mtx, ts));
}

int mtx_trylock(mtx_t* mtx) {
  if (mtx == 0) {
    return thrd_error;
  }
  return translate_result(pthread_mutex_trylock(mtx));
}

int mtx_unlock(mtx_t* mtx) {
  if (mtx == 0) {
    return thrd_error;
  }
  return pthread_mutex_unlock(mtx) == 0 ? thrd_success : thrd_error;
}

/* --- thrd_t ---
 *
 * thrd_start_t (int(*)(void*)) has a different signature from pthread's
 * void*(*)(void*), so thrd_create() spawns through a small heap-allocated
 * shim that adapts one to the other and packs the int result back into a
 * void* for pthread_join()/thrd_join() to unpack.
 */

struct crt_thrd_shim_args {
  thrd_start_t func;
  void* arg;
};

static void* crt_thrd_shim(void* raw) {
  struct crt_thrd_shim_args* shim = (struct crt_thrd_shim_args*)raw;
  thrd_start_t func = shim->func;
  void* arg = shim->arg;
  int result;

  free(shim);
  result = func(arg);
  return (void*)(intptr_t)result;
}

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg) {
  struct crt_thrd_shim_args* shim;
  int result;

  if (thr == 0 || func == 0) {
    return thrd_error;
  }
  shim = (struct crt_thrd_shim_args*)malloc(sizeof(*shim));
  if (shim == 0) {
    return thrd_nomem;
  }
  shim->func = func;
  shim->arg = arg;
  result = pthread_create(thr, 0, crt_thrd_shim, shim);
  if (result != 0) {
    free(shim);
    return result == ENOMEM ? thrd_nomem : thrd_error;
  }
  return thrd_success;
}

thrd_t thrd_current(void) {
  return pthread_self();
}

int thrd_detach(thrd_t thr) {
  return pthread_detach(thr) == 0 ? thrd_success : thrd_error;
}

int thrd_equal(thrd_t lhs, thrd_t rhs) {
  return pthread_equal(lhs, rhs);
}

void thrd_exit(int res) {
  pthread_exit((void*)(intptr_t)res);
}

int thrd_join(thrd_t thr, int* res) {
  void* retval = 0;

  if (pthread_join(thr, &retval) != 0) {
    return thrd_error;
  }
  if (res != 0) {
    *res = (int)(intptr_t)retval;
  }
  return thrd_success;
}

int thrd_sleep(const struct timespec* duration, struct timespec* remaining) {
  if (duration == 0) {
    return -2;
  }
  if (nanosleep(duration, remaining) == 0) {
    return 0;
  }
  return errno == EINTR ? -1 : -2;
}

void thrd_yield(void) {
  sched_yield();
}

/* --- tss_t --- */

int tss_create(tss_t* key, tss_dtor_t dtor) {
  if (key == 0) {
    return thrd_error;
  }
  return pthread_key_create(key, dtor) == 0 ? thrd_success : thrd_error;
}

void tss_delete(tss_t key) {
  pthread_key_delete(key);
}

void* tss_get(tss_t key) {
  return pthread_getspecific(key);
}

int tss_set(tss_t key, void* val) {
  return pthread_setspecific(key, val) == 0 ? thrd_success : thrd_error;
}
