#ifndef CRT_PTHREAD_H
#define CRT_PTHREAD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long pthread_t;
typedef unsigned int pthread_key_t;

typedef struct {
  int state;
} pthread_mutex_t;

typedef struct {
  int state;
} pthread_once_t;

typedef void (*__pthread_once_func_t)(void);
typedef void (*__pthread_key_destructor_t)(void*);

#define PTHREAD_MUTEX_INITIALIZER \
  { 0 }
#define PTHREAD_ONCE_INIT \
  { 0 }

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

#ifdef __cplusplus
}
#endif

#endif
