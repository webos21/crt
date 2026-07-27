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
  int32_t __private[10];
} pthread_mutex_t;

typedef int pthread_once_t;
typedef intptr_t pthread_t;

typedef void (*__pthread_once_func_t)(void);
typedef void (*__pthread_key_destructor_t)(void*);

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1
#define PTHREAD_STACK_MIN 16384

#define PTHREAD_MUTEX_INITIALIZER { { ((PTHREAD_MUTEX_NORMAL & 3) << 14) } }
#define PTHREAD_ONCE_INIT 0

int pthread_mutex_init(pthread_mutex_t* mutex, const void* attr);
int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);
int pthread_once(pthread_once_t* once_control, __pthread_once_func_t init_routine);
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
int pthread_join(pthread_t thread, void** retval);
void pthread_exit(void* retval) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif
