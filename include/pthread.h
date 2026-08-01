#ifndef CRT_PTHREAD_H
#define CRT_PTHREAD_H

#include <stddef.h>
#include <stdint.h>
#include <sched.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t flags;
  void* stack_base;
  size_t stack_size;
  size_t guard_size;
  int32_t sched_policy;
  int32_t sched_priority;
  char __reserved[16];
} pthread_attr_t;

typedef int pthread_key_t;

typedef struct {
  int32_t __private[4];
} pthread_barrier_t;

typedef long pthread_barrierattr_t;

typedef struct {
  int32_t __private[12];
} pthread_cond_t;

typedef long pthread_condattr_t;

typedef struct {
  int32_t __private[10];
} pthread_mutex_t;

typedef long pthread_mutexattr_t;
typedef int pthread_once_t;
typedef intptr_t pthread_t;

typedef struct {
  int32_t __private[14];
} pthread_rwlock_t;

typedef long pthread_rwlockattr_t;
typedef int pthread_spinlock_t;

typedef void (*__pthread_once_func_t)(void);
typedef void (*__pthread_key_destructor_t)(void*);

struct timespec;

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1
#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED 1
#define PTHREAD_STACK_MIN 16384
#define PTHREAD_KEYS_MAX 128
#define PTHREAD_EXPLICIT_SCHED 0
#define PTHREAD_INHERIT_SCHED 1
#define PTHREAD_SCOPE_SYSTEM 0
#define PTHREAD_SCOPE_PROCESS 1
#define PTHREAD_MUTEX_STALLED 0
#define PTHREAD_MUTEX_ROBUST 1
#define PTHREAD_CANCEL_ENABLE 0
#define PTHREAD_CANCEL_DISABLE 1
#define PTHREAD_CANCEL_DEFERRED 0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1
#define PTHREAD_CANCELED ((void*)-1)
#define PTHREAD_BARRIER_SERIAL_THREAD (-1)

#define PTHREAD_COND_CLOCK_REALTIME 0
#define PTHREAD_COND_CLOCK_MONOTONIC 1

#define PTHREAD_BARRIER_INITIALIZER { { 0 } }
#define PTHREAD_COND_INITIALIZER { { 0 } }
#define PTHREAD_MUTEX_INITIALIZER { { ((PTHREAD_MUTEX_NORMAL & 3) << 14) } }
#define PTHREAD_ONCE_INIT 0
#define PTHREAD_RWLOCK_INITIALIZER { { 0 } }

int pthread_barrier_init(
    pthread_barrier_t* barrier,
    const pthread_barrierattr_t* attr,
    unsigned int count);
int pthread_barrier_destroy(pthread_barrier_t* barrier);
int pthread_barrier_wait(pthread_barrier_t* barrier);
int pthread_barrierattr_init(pthread_barrierattr_t* attr);
int pthread_barrierattr_destroy(pthread_barrierattr_t* attr);
int pthread_barrierattr_getpshared(const pthread_barrierattr_t* attr, int* pshared);
int pthread_barrierattr_setpshared(pthread_barrierattr_t* attr, int pshared);
int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
int pthread_cond_destroy(pthread_cond_t* cond);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_broadcast(pthread_cond_t* cond);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
int pthread_cond_timedwait(
    pthread_cond_t* cond,
    pthread_mutex_t* mutex,
    const struct timespec* abstime);
int pthread_condattr_init(pthread_condattr_t* attr);
int pthread_condattr_destroy(pthread_condattr_t* attr);
int pthread_condattr_getclock(const pthread_condattr_t* attr, int* clock_id);
int pthread_condattr_setclock(pthread_condattr_t* attr, int clock_id);
int pthread_mutex_init(pthread_mutex_t* mutex, const void* attr);
int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);
int pthread_mutexattr_init(pthread_mutexattr_t* attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr);
int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type);
int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type);
int pthread_mutexattr_getpshared(const pthread_mutexattr_t* attr, int* pshared);
int pthread_mutexattr_setpshared(pthread_mutexattr_t* attr, int pshared);
int pthread_mutexattr_getrobust(const pthread_mutexattr_t* attr, int* robust);
int pthread_mutexattr_setrobust(pthread_mutexattr_t* attr, int robust);
int pthread_once(pthread_once_t* once_control, __pthread_once_func_t init_routine);
int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr);
int pthread_rwlock_destroy(pthread_rwlock_t* rwlock);
int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t* rwlock);
int pthread_rwlockattr_init(pthread_rwlockattr_t* attr);
int pthread_rwlockattr_destroy(pthread_rwlockattr_t* attr);
int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t* attr, int* pshared);
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t* attr, int pshared);
int pthread_spin_init(pthread_spinlock_t* lock, int pshared);
int pthread_spin_destroy(pthread_spinlock_t* lock);
int pthread_spin_lock(pthread_spinlock_t* lock);
int pthread_spin_trylock(pthread_spinlock_t* lock);
int pthread_spin_unlock(pthread_spinlock_t* lock);
pthread_t pthread_self(void);
int pthread_equal(pthread_t t1, pthread_t t2);
int pthread_key_create(pthread_key_t* key, __pthread_key_destructor_t destructor);
int pthread_key_delete(pthread_key_t key);
void* pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void* value);
int pthread_attr_init(pthread_attr_t* attr);
int pthread_attr_destroy(pthread_attr_t* attr);
int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* state);
int pthread_attr_setdetachstate(pthread_attr_t* attr, int state);
int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stack_size);
int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stack_size);
int pthread_attr_getguardsize(const pthread_attr_t* attr, size_t* guard_size);
int pthread_attr_setguardsize(pthread_attr_t* attr, size_t guard_size);
int pthread_attr_getstack(const pthread_attr_t* attr, void** stack_addr, size_t* stack_size);
int pthread_attr_setstack(pthread_attr_t* attr, void* stack_addr, size_t stack_size);
int pthread_attr_getinheritsched(const pthread_attr_t* attr, int* inheritsched);
int pthread_attr_setinheritsched(pthread_attr_t* attr, int inheritsched);
int pthread_attr_getschedpolicy(const pthread_attr_t* attr, int* policy);
int pthread_attr_setschedpolicy(pthread_attr_t* attr, int policy);
int pthread_attr_getschedparam(const pthread_attr_t* attr, struct sched_param* param);
int pthread_attr_setschedparam(pthread_attr_t* attr, const struct sched_param* param);
int pthread_attr_getscope(const pthread_attr_t* attr, int* scope);
int pthread_attr_setscope(pthread_attr_t* attr, int scope);
int pthread_getattr_np(pthread_t thread, pthread_attr_t* attr);
int pthread_create(
    pthread_t* thread,
    const pthread_attr_t* attr,
    void* (*start_routine)(void*),
    void* arg);
int pthread_detach(pthread_t thread);
int pthread_join(pthread_t thread, void** retval);
int pthread_getschedparam(pthread_t thread, int* policy, struct sched_param* param);
int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param* param);
int pthread_setschedprio(pthread_t thread, int priority);
pid_t pthread_gettid_np(pthread_t thread);
int pthread_getcpuclockid(pthread_t thread, clockid_t* clock_id);
int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));
int pthread_setname_np(pthread_t thread, const char* name);
int pthread_getname_np(pthread_t thread, char* buf, size_t size);
int pthread_cancel(pthread_t thread);
int pthread_setcancelstate(int state, int* oldstate);
int pthread_setcanceltype(int type, int* oldtype);
void pthread_testcancel(void);
void pthread_exit(void* retval) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif
