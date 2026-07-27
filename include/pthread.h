#ifndef CRT_PTHREAD_H
#define CRT_PTHREAD_H

#include <stddef.h>
#include <stdint.h>

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

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1
#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED 1
#define PTHREAD_STACK_MIN 16384

#define PTHREAD_COND_INITIALIZER { { 0 } }
#define PTHREAD_MUTEX_INITIALIZER { { ((PTHREAD_MUTEX_NORMAL & 3) << 14) } }
#define PTHREAD_ONCE_INIT 0
#define PTHREAD_RWLOCK_INITIALIZER { { 0 } }

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
int pthread_cond_destroy(pthread_cond_t* cond);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_broadcast(pthread_cond_t* cond);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
int pthread_condattr_init(pthread_condattr_t* attr);
int pthread_condattr_destroy(pthread_condattr_t* attr);
int pthread_mutex_init(pthread_mutex_t* mutex, const void* attr);
int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);
int pthread_mutexattr_init(pthread_mutexattr_t* attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr);
int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type);
int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type);
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
int pthread_create(
    pthread_t* thread,
    const pthread_attr_t* attr,
    void* (*start_routine)(void*),
    void* arg);
int pthread_detach(pthread_t thread);
int pthread_join(pthread_t thread, void** retval);
void pthread_exit(void* retval) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif
