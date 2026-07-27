#include <errno.h>
#include <pthread.h>
#include <stdint.h>

#include <private/crt_atomic.h>

long __crt_sys_thread_id(void);

#define CRT_PTHREAD_KEYS_MAX 128

#if defined(CRT_TARGET_OS_WINDOWS)
typedef unsigned long DWORD;
typedef int BOOL;

#define CRT_WINAPI
#define CRT_TLS_OUT_OF_INDEXES ((DWORD)0xffffffffUL)

__declspec(dllimport) DWORD CRT_WINAPI TlsAlloc(void);
__declspec(dllimport) BOOL CRT_WINAPI TlsFree(DWORD dwTlsIndex);
__declspec(dllimport) void* CRT_WINAPI TlsGetValue(DWORD dwTlsIndex);
__declspec(dllimport) BOOL CRT_WINAPI TlsSetValue(DWORD dwTlsIndex, void* lpTlsValue);

static DWORD pthread_key_slots[CRT_PTHREAD_KEYS_MAX];
static pthread_once_t pthread_key_once = PTHREAD_ONCE_INIT;
#else
static __thread void* pthread_key_values[CRT_PTHREAD_KEYS_MAX];
#endif

static int pthread_key_used[CRT_PTHREAD_KEYS_MAX];
static crt_spinlock pthread_key_lock = CRT_SPINLOCK_INIT;

static crt_atomic_int* mutex_state(pthread_mutex_t* mutex) {
  return (crt_atomic_int*)&mutex->state;
}

static crt_once* once_state(pthread_once_t* once_control) {
  return (crt_once*)once_control;
}

#if defined(CRT_TARGET_OS_WINDOWS)
static void init_windows_key_slots(void) {
  unsigned int i;

  for (i = 0; i < CRT_PTHREAD_KEYS_MAX; ++i) {
    pthread_key_slots[i] = CRT_TLS_OUT_OF_INDEXES;
  }
}
#endif

static int pthread_key_is_valid(pthread_key_t key) {
  return key < CRT_PTHREAD_KEYS_MAX && pthread_key_used[key] != 0;
}

int pthread_mutex_init(pthread_mutex_t* mutex, const void* attr) {
  (void)attr;

  if (mutex == 0) {
    return EINVAL;
  }
  mutex->state = 0;
  return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex) {
  if (mutex == 0) {
    return EINVAL;
  }
  if (crt_atomic_load_acquire(mutex_state(mutex)) != 0) {
    return EBUSY;
  }
  return 0;
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {
  if (mutex == 0) {
    return EINVAL;
  }
  while (crt_atomic_exchange_acquire(mutex_state(mutex), 1) != 0) {
    while (crt_atomic_load_relaxed(mutex_state(mutex)) != 0) {
      sched_yield();
    }
  }
  return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
  if (mutex == 0) {
    return EINVAL;
  }
  if (crt_atomic_load_acquire(mutex_state(mutex)) == 0) {
    return EPERM;
  }
  crt_atomic_store_release(mutex_state(mutex), 0);
  return 0;
}

int pthread_once(pthread_once_t* once_control, __pthread_once_func_t init_routine) {
  if (once_control == 0 || init_routine == 0) {
    return EINVAL;
  }

  if (crt_once_begin(once_state(once_control))) {
    init_routine();
    crt_once_complete(once_state(once_control));
  }
  return 0;
}

pthread_t pthread_self(void) {
  return (pthread_t)__crt_sys_thread_id();
}

int pthread_equal(pthread_t t1, pthread_t t2) {
  return t1 == t2;
}

int pthread_key_create(pthread_key_t* key, __pthread_key_destructor_t destructor) {
  unsigned int i;
  (void)destructor;

  if (key == 0) {
    return EINVAL;
  }

#if defined(CRT_TARGET_OS_WINDOWS)
  pthread_once(&pthread_key_once, init_windows_key_slots);
#endif

  crt_spin_lock(&pthread_key_lock);
  for (i = 0; i < CRT_PTHREAD_KEYS_MAX; ++i) {
    if (pthread_key_used[i] == 0) {
#if defined(CRT_TARGET_OS_WINDOWS)
      DWORD slot = TlsAlloc();
      if (slot == CRT_TLS_OUT_OF_INDEXES) {
        crt_spin_unlock(&pthread_key_lock);
        return EAGAIN;
      }
      pthread_key_slots[i] = slot;
#endif
      pthread_key_used[i] = 1;
      *key = i;
      crt_spin_unlock(&pthread_key_lock);
      return 0;
    }
  }
  crt_spin_unlock(&pthread_key_lock);
  return EAGAIN;
}

int pthread_key_delete(pthread_key_t key) {
  int result = 0;

#if defined(CRT_TARGET_OS_WINDOWS)
  pthread_once(&pthread_key_once, init_windows_key_slots);
#endif

  crt_spin_lock(&pthread_key_lock);
  if (!pthread_key_is_valid(key)) {
    result = EINVAL;
  } else {
#if defined(CRT_TARGET_OS_WINDOWS)
    DWORD slot = pthread_key_slots[key];
    pthread_key_slots[key] = CRT_TLS_OUT_OF_INDEXES;
    if (!TlsFree(slot)) {
      result = EINVAL;
    }
#else
    pthread_key_values[key] = 0;
#endif
    pthread_key_used[key] = 0;
  }
  crt_spin_unlock(&pthread_key_lock);
  return result;
}

void* pthread_getspecific(pthread_key_t key) {
  if (!pthread_key_is_valid(key)) {
    return 0;
  }
#if defined(CRT_TARGET_OS_WINDOWS)
  return TlsGetValue(pthread_key_slots[key]);
#else
  return pthread_key_values[key];
#endif
}

int pthread_setspecific(pthread_key_t key, const void* value) {
  if (!pthread_key_is_valid(key)) {
    return EINVAL;
  }
#if defined(CRT_TARGET_OS_WINDOWS)
  if (!TlsSetValue(pthread_key_slots[key], (void*)value)) {
    return EINVAL;
  }
#else
  pthread_key_values[key] = (void*)value;
#endif
  return 0;
}
