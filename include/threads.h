#ifndef CRT_THREADS_H
#define CRT_THREADS_H

/*
 * C11 <threads.h>. Real Bionic implements this as a thin wrapper over its
 * own pthreads implementation; this project does the same -- thrd_t/mtx_t/
 * cnd_t/tss_t/once_flag are direct typedefs of the matching pthread_*
 * types, and every function here forwards to (or trivially adapts) the
 * corresponding pthread_* entry point already implemented in pthread.c.
 */
#include <pthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef pthread_t thrd_t;
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t cnd_t;
typedef pthread_key_t tss_t;
typedef pthread_once_t once_flag;

typedef int (*thrd_start_t)(void*);
typedef void (*tss_dtor_t)(void*);

enum {
  mtx_plain = 0,
  mtx_recursive = 1,
  mtx_timed = 2,
};

enum {
  thrd_success = 0,
  thrd_timedout = 1,
  thrd_busy = 2,
  thrd_error = 3,
  thrd_nomem = 4,
};

/* Matches pthread_key_t's own destructor-iteration cap (CRT_PTHREAD_
 * DESTRUCTOR_ITERATIONS in pthread.c), since tss_t is pthread_key_t. */
#define TSS_DTOR_ITERATIONS 4

#define ONCE_FLAG_INIT PTHREAD_ONCE_INIT

void call_once(once_flag* flag, void (*func)(void));

int cnd_broadcast(cnd_t* cond);
void cnd_destroy(cnd_t* cond);
int cnd_init(cnd_t* cond);
int cnd_signal(cnd_t* cond);
int cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts);
int cnd_wait(cnd_t* cond, mtx_t* mtx);

void mtx_destroy(mtx_t* mtx);
int mtx_init(mtx_t* mtx, int type);
int mtx_lock(mtx_t* mtx);
int mtx_timedlock(mtx_t* mtx, const struct timespec* ts);
int mtx_trylock(mtx_t* mtx);
int mtx_unlock(mtx_t* mtx);

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg);
thrd_t thrd_current(void);
int thrd_detach(thrd_t thr);
int thrd_equal(thrd_t lhs, thrd_t rhs);
void thrd_exit(int res) __attribute__((noreturn));
int thrd_join(thrd_t thr, int* res);
int thrd_sleep(const struct timespec* duration, struct timespec* remaining);
void thrd_yield(void);

int tss_create(tss_t* key, tss_dtor_t dtor);
void tss_delete(tss_t key);
void* tss_get(tss_t key);
int tss_set(tss_t key, void* val);

#ifdef __cplusplus
}
#endif

#endif
