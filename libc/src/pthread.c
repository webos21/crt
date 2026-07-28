#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>

#include <private/crt_atomic.h>
#include <private/crt_wait.h>

long __crt_sys_thread_id(void);
void __crt_sys_thread_exit(int status) __attribute__((noreturn));

#define CRT_PTHREAD_KEYS_MAX 128
#define CRT_PTHREAD_DESTRUCTOR_ITERATIONS 4
#define CRT_PTHREAD_STACK_SIZE (1024UL * 1024UL)
#define CRT_PTHREAD_ATTR_FLAG_DETACHED 0x00000001U
#define CRT_PTHREAD_ATTR_FLAG_STACK_USER 0x00000002U
#define CRT_MUTEXATTR_TYPE_MASK 0x000000ffL
#define CRT_MUTEXATTR_PSHARED_BIT 0x00000100L
#define CRT_RWLOCKATTR_PSHARED_BIT 0x00000001L
#define CRT_COND_SEQUENCE_WORD 0
#define CRT_COND_WAITERS_WORD 1
#define CRT_MUTEX_STATE_WORD 0
#define CRT_MUTEX_TYPE_WORD 1
#define CRT_MUTEX_COUNT_WORD 2
#define CRT_MUTEX_OWNER_LOW_WORD 3
#define CRT_MUTEX_OWNER_HIGH_WORD 4
#define CRT_RWLOCK_STATE_WORD 0
#define CRT_RWLOCK_WRITER_STATE (-1)

#if defined(CRT_TARGET_OS_WINDOWS)
typedef unsigned long DWORD;
typedef int BOOL;
typedef void* HANDLE;

#define CRT_WINAPI
#define CRT_TLS_OUT_OF_INDEXES ((DWORD)0xffffffffUL)
#define CRT_WAIT_OBJECT_0 0
#define CRT_INFINITE 0xffffffffUL

__declspec(dllimport) DWORD CRT_WINAPI TlsAlloc(void);
__declspec(dllimport) BOOL CRT_WINAPI TlsFree(DWORD dwTlsIndex);
__declspec(dllimport) void* CRT_WINAPI TlsGetValue(DWORD dwTlsIndex);
__declspec(dllimport) BOOL CRT_WINAPI TlsSetValue(DWORD dwTlsIndex, void* lpTlsValue);
__declspec(dllimport) HANDLE CRT_WINAPI CreateThread(
    void* lpThreadAttributes,
    size_t dwStackSize,
    DWORD (CRT_WINAPI* lpStartAddress)(void*),
    void* lpParameter,
    DWORD dwCreationFlags,
    DWORD* lpThreadId);
__declspec(dllimport) DWORD CRT_WINAPI WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
__declspec(dllimport) BOOL CRT_WINAPI CloseHandle(HANDLE hObject);
__declspec(dllimport) void CRT_WINAPI ExitThread(DWORD dwExitCode) __attribute__((noreturn));

static DWORD pthread_key_slots[CRT_PTHREAD_KEYS_MAX];
static pthread_once_t pthread_key_once = PTHREAD_ONCE_INIT;
static DWORD pthread_control_tls_slot = CRT_TLS_OUT_OF_INDEXES;
static pthread_once_t pthread_control_once = PTHREAD_ONCE_INIT;
#elif defined(CRT_TARGET_OS_LINUX)
long __crt_sys_clone_thread(
    void* stack_top,
    int (*entry)(void*),
    void* arg,
    unsigned long flags,
    int* parent_tid,
    int* child_tid,
    void* tls);
long __crt_sys_wait4(long pid, int* status, int options, void* rusage);
long __crt_sys_futex(int* addr, int op, int value, const void* timeout, int* addr2, int value3);

#define CRT_FUTEX_WAIT 0
#define CRT_LINUX_REAPER_STACK_SIZE (64UL * 1024UL)
#define CRT_CLONE_VM 0x00000100UL
#define CRT_CLONE_FS 0x00000200UL
#define CRT_CLONE_FILES 0x00000400UL
#define CRT_CLONE_SIGHAND 0x00000800UL
#define CRT_CLONE_THREAD 0x00010000UL
#define CRT_CLONE_SYSVSEM 0x00040000UL
#define CRT_CLONE_PARENT_SETTID 0x00100000UL
#define CRT_CLONE_CHILD_CLEARTID 0x00200000UL
#define CRT_CLONE_CHILD_SETTID 0x01000000UL
#define CRT_CLONE_THREAD_FLAGS \
  (CRT_CLONE_VM | CRT_CLONE_FS | CRT_CLONE_FILES | CRT_CLONE_SIGHAND | CRT_CLONE_THREAD | \
   CRT_CLONE_SYSVSEM | CRT_CLONE_PARENT_SETTID | CRT_CLONE_CHILD_CLEARTID | \
   CRT_CLONE_CHILD_SETTID)
#elif defined(CRT_TARGET_OS_MACOS)
typedef void* crt_macos_pthread_t;
typedef int (*crt_macos_pthread_create_fn)(
    crt_macos_pthread_t*,
    const void*,
    void* (*)(void*),
    void*);
typedef int (*crt_macos_pthread_join_fn)(crt_macos_pthread_t, void**);
typedef int (*crt_macos_pthread_detach_fn)(crt_macos_pthread_t);
typedef void (*crt_macos_pthread_exit_fn)(void*) __attribute__((noreturn));

#define CRT_RTLD_NEXT ((void*)-1)

void* dlsym(void* handle, const char* symbol);
#endif

#if !defined(CRT_TARGET_OS_WINDOWS)
static __thread void* pthread_key_values[CRT_PTHREAD_KEYS_MAX];
#endif

typedef struct crt_pthread_control {
#if defined(CRT_TARGET_OS_WINDOWS)
  HANDLE handle;
  DWORD thread_id;
#elif defined(CRT_TARGET_OS_LINUX)
  long tid;
  int tid_word;
  void* stack;
  unsigned long stack_size;
  int stack_owned;
  int reap_queued;
  struct crt_pthread_control* reap_next;
#elif defined(CRT_TARGET_OS_MACOS)
  crt_macos_pthread_t native_thread;
#endif
  void* (*start_routine)(void*);
  void* arg;
  void* result;
  int detached;
} crt_pthread_control;

#if !defined(CRT_TARGET_OS_WINDOWS)
static __thread crt_pthread_control* pthread_current_control;
#endif

static int pthread_key_used[CRT_PTHREAD_KEYS_MAX];
static __pthread_key_destructor_t pthread_key_destructors[CRT_PTHREAD_KEYS_MAX];
static crt_spinlock pthread_key_lock = CRT_SPINLOCK_INIT;

#if defined(CRT_TARGET_OS_LINUX)
static crt_spinlock linux_reap_lock = CRT_SPINLOCK_INIT;
static crt_atomic_int linux_reap_sequence = CRT_ATOMIC_INT_INIT(0);
static crt_pthread_control* linux_reap_head;
static int linux_reaper_started;
static int linux_reaper_tid;
static void* linux_reaper_stack;
#endif

static crt_atomic_int* mutex_state(pthread_mutex_t* mutex) {
  return (crt_atomic_int*)&mutex->__private[CRT_MUTEX_STATE_WORD];
}

static crt_atomic_int* cond_sequence(pthread_cond_t* cond) {
  return (crt_atomic_int*)&cond->__private[CRT_COND_SEQUENCE_WORD];
}

static crt_atomic_int* cond_waiters(pthread_cond_t* cond) {
  return (crt_atomic_int*)&cond->__private[CRT_COND_WAITERS_WORD];
}

static crt_once* once_state(pthread_once_t* once_control) {
  return (crt_once*)once_control;
}

static crt_atomic_int* rwlock_state(pthread_rwlock_t* rwlock) {
  return (crt_atomic_int*)&rwlock->__private[CRT_RWLOCK_STATE_WORD];
}

static int mutex_type(const pthread_mutex_t* mutex) {
  int type = mutex->__private[CRT_MUTEX_TYPE_WORD] & (int)CRT_MUTEXATTR_TYPE_MASK;

  if (type == PTHREAD_MUTEX_RECURSIVE || type == PTHREAD_MUTEX_ERRORCHECK) {
    return type;
  }
  return PTHREAD_MUTEX_NORMAL;
}

static pthread_t mutex_owner(const pthread_mutex_t* mutex) {
  uint64_t low = (uint32_t)mutex->__private[CRT_MUTEX_OWNER_LOW_WORD];
  uint64_t high = (uint32_t)mutex->__private[CRT_MUTEX_OWNER_HIGH_WORD];

  return (pthread_t)(intptr_t)((high << 32) | low);
}

static void set_mutex_owner(pthread_mutex_t* mutex, pthread_t owner) {
  uint64_t value = (uint64_t)(uintptr_t)owner;

  mutex->__private[CRT_MUTEX_OWNER_LOW_WORD] = (int32_t)(value & 0xffffffffU);
  mutex->__private[CRT_MUTEX_OWNER_HIGH_WORD] = (int32_t)(value >> 32);
}

static void clear_mutex_owner(pthread_mutex_t* mutex) {
  mutex->__private[CRT_MUTEX_OWNER_LOW_WORD] = 0;
  mutex->__private[CRT_MUTEX_OWNER_HIGH_WORD] = 0;
}

static int realtime_until(const struct timespec* abstime, struct timespec* remaining) {
  struct timespec now;
  time_t sec;
  long nsec;

  if (abstime == 0 || remaining == 0 || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000L) {
    return EINVAL;
  }
  if (abstime->tv_sec < 0 || (abstime->tv_sec == 0 && abstime->tv_nsec == 0)) {
    return ETIMEDOUT;
  }
  if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
    return errno;
  }

  sec = abstime->tv_sec - now.tv_sec;
  nsec = abstime->tv_nsec - now.tv_nsec;
  if (nsec < 0) {
    --sec;
    nsec += 1000000000L;
  }
  if (sec < 0 || (sec == 0 && nsec <= 0)) {
    return ETIMEDOUT;
  }
  remaining->tv_sec = sec;
  remaining->tv_nsec = nsec;
  return 0;
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
  return key >= 0 && key < CRT_PTHREAD_KEYS_MAX && pthread_key_used[key] != 0;
}

#if defined(CRT_TARGET_OS_WINDOWS)
static void init_windows_control_slot(void) {
  pthread_control_tls_slot = TlsAlloc();
}

static crt_pthread_control* pthread_get_current_control(void) {
  pthread_once(&pthread_control_once, init_windows_control_slot);
  if (pthread_control_tls_slot == CRT_TLS_OUT_OF_INDEXES) {
    return 0;
  }
  return (crt_pthread_control*)TlsGetValue(pthread_control_tls_slot);
}

static void pthread_set_current_control(crt_pthread_control* control) {
  pthread_once(&pthread_control_once, init_windows_control_slot);
  if (pthread_control_tls_slot != CRT_TLS_OUT_OF_INDEXES) {
    TlsSetValue(pthread_control_tls_slot, control);
  }
}
#else
static crt_pthread_control* pthread_get_current_control(void) {
  return pthread_current_control;
}

static void pthread_set_current_control(crt_pthread_control* control) {
  pthread_current_control = control;
}
#endif

static void pthread_run_key_destructors(void) {
  int iteration;

  for (iteration = 0; iteration < CRT_PTHREAD_DESTRUCTOR_ITERATIONS; ++iteration) {
    int called = 0;
    unsigned int i;

    for (i = 0; i < CRT_PTHREAD_KEYS_MAX; ++i) {
      __pthread_key_destructor_t destructor = 0;
      void* value = 0;

      crt_spin_lock(&pthread_key_lock);
      if (pthread_key_used[i]) {
        destructor = pthread_key_destructors[i];
#if defined(CRT_TARGET_OS_WINDOWS)
        value = TlsGetValue(pthread_key_slots[i]);
        if (value != 0 && destructor != 0) {
          TlsSetValue(pthread_key_slots[i], 0);
        }
#else
        value = pthread_key_values[i];
        if (value != 0 && destructor != 0) {
          pthread_key_values[i] = 0;
        }
#endif
      }
      crt_spin_unlock(&pthread_key_lock);

      if (value != 0 && destructor != 0) {
        destructor(value);
        called = 1;
      }
    }

    if (!called) {
      break;
    }
  }
}

#if defined(CRT_TARGET_OS_WINDOWS) || defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_MACOS)
#if defined(CRT_TARGET_OS_LINUX)
static void linux_reap_enqueue(crt_pthread_control* control);
static int linux_reaper_start(void* arg);
static int linux_reaper_ensure_started(void);
#endif

static void pthread_control_destroy(crt_pthread_control* control) {
  if (control == 0) {
    return;
  }
#if defined(CRT_TARGET_OS_LINUX)
  if (control->stack_owned && control->stack != 0) {
    munmap(control->stack, control->stack_size);
  }
#endif
  free(control);
}

static void pthread_control_destroy_from_worker(crt_pthread_control* control) {
  if (control == 0) {
    return;
  }
#if defined(CRT_TARGET_OS_LINUX)
  linux_reap_enqueue(control);
#else
  free(control);
#endif
}

#if defined(CRT_TARGET_OS_LINUX)
static void linux_reap_enqueue(crt_pthread_control* control) {
  int expected = 0;

  if (control == 0) {
    return;
  }
  if (!__atomic_compare_exchange_n(
          &control->reap_queued, &expected, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
    return;
  }

  crt_spin_lock(&linux_reap_lock);
  control->reap_next = linux_reap_head;
  linux_reap_head = control;
  crt_spin_unlock(&linux_reap_lock);

  crt_atomic_fetch_add_acq_rel(&linux_reap_sequence, 1);
  __crt_wake32_all(&linux_reap_sequence.value);
}

static int linux_reaper_start(void* arg) {
  (void)arg;

  for (;;) {
    int sequence;
    int* wait_tid = 0;
    int wait_tid_value = 0;
    int reaped = 0;

    crt_spin_lock(&linux_reap_lock);
    {
      crt_pthread_control** link = &linux_reap_head;
      while (*link != 0) {
        crt_pthread_control* control = *link;
        if (__atomic_load_n(&control->tid_word, __ATOMIC_ACQUIRE) == 0) {
          *link = control->reap_next;
          control->reap_next = 0;
          crt_spin_unlock(&linux_reap_lock);
          pthread_control_destroy(control);
          reaped = 1;
          crt_spin_lock(&linux_reap_lock);
          link = &linux_reap_head;
        } else {
          if (wait_tid == 0) {
            wait_tid = &control->tid_word;
            wait_tid_value = __atomic_load_n(&control->tid_word, __ATOMIC_ACQUIRE);
          }
          link = &control->reap_next;
        }
      }
      sequence = crt_atomic_load_acquire(&linux_reap_sequence);
    }
    crt_spin_unlock(&linux_reap_lock);

    if (!reaped) {
      if (wait_tid != 0 && wait_tid_value != 0) {
        __crt_sys_futex(wait_tid, CRT_FUTEX_WAIT, wait_tid_value, 0, 0, 0);
      } else {
        __crt_wait32(&linux_reap_sequence.value, sequence);
      }
    }
  }
}

static int linux_reaper_ensure_started(void) {
  int expected = 0;
  int state;
  void* stack;
  long tid;

  state = __atomic_load_n(&linux_reaper_started, __ATOMIC_ACQUIRE);
  if (state == 2) {
    return 0;
  }
  if (state < 0) {
    return EAGAIN;
  }
  if (!__atomic_compare_exchange_n(
          &linux_reaper_started, &expected, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
    while (__atomic_load_n(&linux_reaper_started, __ATOMIC_ACQUIRE) == 1) {
      sched_yield();
    }
    return __atomic_load_n(&linux_reaper_started, __ATOMIC_ACQUIRE) == 2 ? 0 : EAGAIN;
  }

  stack = mmap(0, CRT_LINUX_REAPER_STACK_SIZE, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (stack == MAP_FAILED) {
    __atomic_store_n(&linux_reaper_started, -1, __ATOMIC_RELEASE);
    return EAGAIN;
  }

  linux_reaper_stack = stack;
  tid = __crt_sys_clone_thread(
      (char*)stack + CRT_LINUX_REAPER_STACK_SIZE,
      linux_reaper_start,
      0,
      CRT_CLONE_THREAD_FLAGS,
      &linux_reaper_tid,
      &linux_reaper_tid,
      0);
  if (tid < 0) {
    munmap(stack, CRT_LINUX_REAPER_STACK_SIZE);
    linux_reaper_stack = 0;
    __atomic_store_n(&linux_reaper_started, -1, __ATOMIC_RELEASE);
    return -tid;
  }
  __atomic_store_n(&linux_reaper_started, 2, __ATOMIC_RELEASE);
  return 0;
}
#endif

static int pthread_start(void* arg) {
  crt_pthread_control* control = (crt_pthread_control*)arg;
  int detached;

  pthread_set_current_control(control);
  control->result = control->start_routine(control->arg);
  pthread_run_key_destructors();
  pthread_set_current_control(0);
  detached = __atomic_load_n(&control->detached, __ATOMIC_ACQUIRE);
  if (detached) {
    pthread_control_destroy_from_worker(control);
  }
  return 0;
}

#if defined(CRT_TARGET_OS_MACOS)
static void* pthread_macos_start(void* arg) {
  pthread_start(arg);
  return 0;
}
#endif

#if defined(CRT_TARGET_OS_WINDOWS)
static DWORD CRT_WINAPI pthread_windows_start(void* arg) {
  return (DWORD)pthread_start(arg);
}
#endif
#endif

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
  (void)attr;

  if (cond == 0) {
    return EINVAL;
  }
  cond->__private[CRT_COND_SEQUENCE_WORD] = 0;
  cond->__private[CRT_COND_WAITERS_WORD] = 0;
  return 0;
}

int pthread_cond_destroy(pthread_cond_t* cond) {
  if (cond == 0) {
    return EINVAL;
  }
  if (crt_atomic_load_acquire(cond_waiters(cond)) != 0) {
    return EBUSY;
  }
  return 0;
}

int pthread_cond_signal(pthread_cond_t* cond) {
  if (cond == 0) {
    return EINVAL;
  }
  crt_atomic_fetch_add_acq_rel(cond_sequence(cond), 1);
  return __crt_wake32_one(&cond->__private[CRT_COND_SEQUENCE_WORD]);
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
  if (cond == 0) {
    return EINVAL;
  }
  crt_atomic_fetch_add_acq_rel(cond_sequence(cond), 1);
  return __crt_wake32_all(&cond->__private[CRT_COND_SEQUENCE_WORD]);
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
  int sequence;
  int result;

  if (cond == 0 || mutex == 0) {
    return EINVAL;
  }

  sequence = crt_atomic_load_acquire(cond_sequence(cond));
  crt_atomic_fetch_add_acq_rel(cond_waiters(cond), 1);
  result = pthread_mutex_unlock(mutex);
  if (result != 0) {
    crt_atomic_fetch_add_acq_rel(cond_waiters(cond), -1);
    return result;
  }

  while (crt_atomic_load_acquire(cond_sequence(cond)) == sequence) {
    result = __crt_wait32(&cond->__private[CRT_COND_SEQUENCE_WORD], sequence);
    if (result != 0 && result != EINTR && result != EAGAIN) {
      break;
    }
  }

  crt_atomic_fetch_add_acq_rel(cond_waiters(cond), -1);
  {
    int lock_result = pthread_mutex_lock(mutex);
    return result != 0 ? result : lock_result;
  }
}

int pthread_cond_timedwait(
    pthread_cond_t* cond,
    pthread_mutex_t* mutex,
    const struct timespec* abstime) {
  int sequence;
  int result;

  if (cond == 0 || mutex == 0 || abstime == 0) {
    return EINVAL;
  }
  if (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000L) {
    return EINVAL;
  }

  sequence = crt_atomic_load_acquire(cond_sequence(cond));
  crt_atomic_fetch_add_acq_rel(cond_waiters(cond), 1);
  result = pthread_mutex_unlock(mutex);
  if (result != 0) {
    crt_atomic_fetch_add_acq_rel(cond_waiters(cond), -1);
    return result;
  }

  while (crt_atomic_load_acquire(cond_sequence(cond)) == sequence) {
    struct timespec remaining;

    result = realtime_until(abstime, &remaining);
    if (result != 0) {
      break;
    }
    result = __crt_wait32_timed(&cond->__private[CRT_COND_SEQUENCE_WORD], sequence, &remaining);
    if (result == ETIMEDOUT) {
      if (crt_atomic_load_acquire(cond_sequence(cond)) == sequence) {
        break;
      }
      result = 0;
    }
    if (result != 0 && result != EINTR && result != EAGAIN) {
      break;
    }
  }

  crt_atomic_fetch_add_acq_rel(cond_waiters(cond), -1);
  {
    int lock_result = pthread_mutex_lock(mutex);
    return result != 0 ? result : lock_result;
  }
}

int pthread_condattr_init(pthread_condattr_t* attr) {
  if (attr == 0) {
    return EINVAL;
  }
  *attr = PTHREAD_COND_CLOCK_REALTIME;
  return 0;
}

int pthread_condattr_getclock(const pthread_condattr_t* attr, int* clock_id) {
  if (attr == 0 || clock_id == 0) {
    return EINVAL;
  }
  *clock_id = (int)*attr;
  return 0;
}

int pthread_condattr_setclock(pthread_condattr_t* attr, int clock_id) {
  if (attr == 0) {
    return EINVAL;
  }
  if (clock_id != PTHREAD_COND_CLOCK_REALTIME && clock_id != PTHREAD_COND_CLOCK_MONOTONIC) {
    return EINVAL;
  }
  *attr = clock_id;
  return 0;
}

int pthread_condattr_destroy(pthread_condattr_t* attr) {
  if (attr == 0) {
    return EINVAL;
  }
  return 0;
}

int pthread_mutex_init(pthread_mutex_t* mutex, const void* attr) {
  const pthread_mutexattr_t* mutex_attr = (const pthread_mutexattr_t*)attr;
  int type = PTHREAD_MUTEX_NORMAL;

  if (mutex == 0) {
    return EINVAL;
  }
  if (mutex_attr != 0) {
    if ((*mutex_attr & CRT_MUTEXATTR_PSHARED_BIT) != 0) {
      return ENOTSUP;
    }
    type = (int)(*mutex_attr & CRT_MUTEXATTR_TYPE_MASK);
  }
  if (type != PTHREAD_MUTEX_NORMAL && type != PTHREAD_MUTEX_RECURSIVE &&
      type != PTHREAD_MUTEX_ERRORCHECK) {
    return EINVAL;
  }
  mutex->__private[CRT_MUTEX_STATE_WORD] = 0;
  mutex->__private[CRT_MUTEX_TYPE_WORD] = type;
  mutex->__private[CRT_MUTEX_COUNT_WORD] = 0;
  clear_mutex_owner(mutex);
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
  pthread_t self;
  int type;
  int expected;
  int wait_result;

  if (mutex == 0) {
    return EINVAL;
  }
  self = pthread_self();
  type = mutex_type(mutex);

  if (type != PTHREAD_MUTEX_NORMAL && mutex_owner(mutex) == self) {
    if (type == PTHREAD_MUTEX_ERRORCHECK) {
      return EDEADLK;
    }
    ++mutex->__private[CRT_MUTEX_COUNT_WORD];
    return 0;
  }

  for (;;) {
    expected = 0;
    if (crt_atomic_compare_exchange_acq_rel(mutex_state(mutex), &expected, 1)) {
      break;
    }
    while (crt_atomic_load_relaxed(mutex_state(mutex)) != 0) {
      wait_result = __crt_wait32(&mutex->__private[CRT_MUTEX_STATE_WORD], 1);
      if (wait_result != 0 && wait_result != EINTR && wait_result != EAGAIN) {
        return wait_result;
      }
    }
  }
  set_mutex_owner(mutex, self);
  mutex->__private[CRT_MUTEX_COUNT_WORD] = 1;
  return 0;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex) {
  pthread_t self;
  int expected = 0;
  int type;

  if (mutex == 0) {
    return EINVAL;
  }
  self = pthread_self();
  type = mutex_type(mutex);

  if (type != PTHREAD_MUTEX_NORMAL && mutex_owner(mutex) == self) {
    if (type == PTHREAD_MUTEX_ERRORCHECK) {
      return EBUSY;
    }
    ++mutex->__private[CRT_MUTEX_COUNT_WORD];
    return 0;
  }

  if (!crt_atomic_compare_exchange_acq_rel(mutex_state(mutex), &expected, 1)) {
    return EBUSY;
  }
  set_mutex_owner(mutex, self);
  mutex->__private[CRT_MUTEX_COUNT_WORD] = 1;
  return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
  int type;

  if (mutex == 0) {
    return EINVAL;
  }
  if (crt_atomic_load_acquire(mutex_state(mutex)) == 0) {
    return EPERM;
  }
  type = mutex_type(mutex);
  if (type != PTHREAD_MUTEX_NORMAL && mutex_owner(mutex) != pthread_self()) {
    return EPERM;
  }
  if (type == PTHREAD_MUTEX_RECURSIVE && mutex->__private[CRT_MUTEX_COUNT_WORD] > 1) {
    --mutex->__private[CRT_MUTEX_COUNT_WORD];
    return 0;
  }
  mutex->__private[CRT_MUTEX_COUNT_WORD] = 0;
  clear_mutex_owner(mutex);
  crt_atomic_store_release(mutex_state(mutex), 0);
  return __crt_wake32_one(&mutex->__private[CRT_MUTEX_STATE_WORD]);
}

int pthread_mutexattr_init(pthread_mutexattr_t* attr) {
  if (attr == 0) {
    return EINVAL;
  }
  *attr = PTHREAD_MUTEX_NORMAL;
  return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t* attr) {
  if (attr == 0) {
    return EINVAL;
  }
  return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type) {
  if (attr == 0 || type == 0) {
    return EINVAL;
  }
  *type = (int)(*attr & CRT_MUTEXATTR_TYPE_MASK);
  return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type) {
  if (attr == 0) {
    return EINVAL;
  }
  if (type != PTHREAD_MUTEX_NORMAL && type != PTHREAD_MUTEX_RECURSIVE &&
      type != PTHREAD_MUTEX_ERRORCHECK) {
    return EINVAL;
  }
  *attr = (*attr & ~CRT_MUTEXATTR_TYPE_MASK) | type;
  return 0;
}

int pthread_mutexattr_getpshared(const pthread_mutexattr_t* attr, int* pshared) {
  if (attr == 0 || pshared == 0) {
    return EINVAL;
  }
  *pshared = (*attr & CRT_MUTEXATTR_PSHARED_BIT) != 0
                 ? PTHREAD_PROCESS_SHARED
                 : PTHREAD_PROCESS_PRIVATE;
  return 0;
}

int pthread_mutexattr_setpshared(pthread_mutexattr_t* attr, int pshared) {
  if (attr == 0) {
    return EINVAL;
  }
  if (pshared == PTHREAD_PROCESS_PRIVATE) {
    *attr &= ~CRT_MUTEXATTR_PSHARED_BIT;
    return 0;
  }
  if (pshared == PTHREAD_PROCESS_SHARED) {
    return ENOTSUP;
  }
  return EINVAL;
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

int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr) {
  if (rwlock == 0) {
    return EINVAL;
  }
  if (attr != 0 && (*attr & CRT_RWLOCKATTR_PSHARED_BIT) != 0) {
    return ENOTSUP;
  }
  rwlock->__private[CRT_RWLOCK_STATE_WORD] = 0;
  return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t* rwlock) {
  if (rwlock == 0) {
    return EINVAL;
  }
  if (crt_atomic_load_acquire(rwlock_state(rwlock)) != 0) {
    return EBUSY;
  }
  return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock) {
  int state;
  int wait_result;

  if (rwlock == 0) {
    return EINVAL;
  }

  for (;;) {
    state = crt_atomic_load_acquire(rwlock_state(rwlock));
    if (state >= 0) {
      int expected = state;
      if (crt_atomic_compare_exchange_acq_rel(rwlock_state(rwlock), &expected, state + 1)) {
        return 0;
      }
      continue;
    }
    wait_result = __crt_wait32(&rwlock->__private[CRT_RWLOCK_STATE_WORD], state);
    if (wait_result != 0 && wait_result != EINTR && wait_result != EAGAIN) {
      return wait_result;
    }
  }
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock) {
  int state;
  int expected;

  if (rwlock == 0) {
    return EINVAL;
  }
  state = crt_atomic_load_acquire(rwlock_state(rwlock));
  if (state < 0) {
    return EBUSY;
  }
  expected = state;
  return crt_atomic_compare_exchange_acq_rel(rwlock_state(rwlock), &expected, state + 1)
             ? 0
             : EBUSY;
}

int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock) {
  int state;
  int wait_result;

  if (rwlock == 0) {
    return EINVAL;
  }

  for (;;) {
    state = 0;
    if (crt_atomic_compare_exchange_acq_rel(rwlock_state(rwlock), &state, CRT_RWLOCK_WRITER_STATE)) {
      return 0;
    }
    state = crt_atomic_load_acquire(rwlock_state(rwlock));
    wait_result = __crt_wait32(&rwlock->__private[CRT_RWLOCK_STATE_WORD], state);
    if (wait_result != 0 && wait_result != EINTR && wait_result != EAGAIN) {
      return wait_result;
    }
  }
}

int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock) {
  int expected = 0;

  if (rwlock == 0) {
    return EINVAL;
  }
  return crt_atomic_compare_exchange_acq_rel(rwlock_state(rwlock), &expected, CRT_RWLOCK_WRITER_STATE)
             ? 0
             : EBUSY;
}

int pthread_rwlock_unlock(pthread_rwlock_t* rwlock) {
  int state;

  if (rwlock == 0) {
    return EINVAL;
  }

  state = crt_atomic_load_acquire(rwlock_state(rwlock));
  if (state == CRT_RWLOCK_WRITER_STATE) {
    crt_atomic_store_release(rwlock_state(rwlock), 0);
    return __crt_wake32_all(&rwlock->__private[CRT_RWLOCK_STATE_WORD]);
  }
  if (state > 0) {
    int previous = crt_atomic_fetch_add_acq_rel(rwlock_state(rwlock), -1);
    if (previous == 1) {
      return __crt_wake32_all(&rwlock->__private[CRT_RWLOCK_STATE_WORD]);
    }
    return 0;
  }
  return EPERM;
}

int pthread_rwlockattr_init(pthread_rwlockattr_t* attr) {
  if (attr == 0) {
    return EINVAL;
  }
  *attr = 0;
  return 0;
}

int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t* attr, int* pshared) {
  if (attr == 0 || pshared == 0) {
    return EINVAL;
  }
  *pshared = (*attr & CRT_RWLOCKATTR_PSHARED_BIT) != 0
                 ? PTHREAD_PROCESS_SHARED
                 : PTHREAD_PROCESS_PRIVATE;
  return 0;
}

int pthread_rwlockattr_setpshared(pthread_rwlockattr_t* attr, int pshared) {
  if (attr == 0) {
    return EINVAL;
  }
  if (pshared == PTHREAD_PROCESS_PRIVATE) {
    *attr &= ~CRT_RWLOCKATTR_PSHARED_BIT;
    return 0;
  }
  if (pshared == PTHREAD_PROCESS_SHARED) {
    return ENOTSUP;
  }
  return EINVAL;
}

int pthread_rwlockattr_destroy(pthread_rwlockattr_t* attr) {
  if (attr == 0) {
    return EINVAL;
  }
  return 0;
}

int pthread_spin_init(pthread_spinlock_t* lock, int pshared) {
  if (lock == 0) {
    return EINVAL;
  }
  if (pshared == PTHREAD_PROCESS_SHARED) {
    return ENOTSUP;
  }
  if (pshared != PTHREAD_PROCESS_PRIVATE) {
    return EINVAL;
  }
  *lock = 0;
  return 0;
}

int pthread_spin_destroy(pthread_spinlock_t* lock) {
  if (lock == 0) {
    return EINVAL;
  }
  if (__atomic_load_n(lock, __ATOMIC_ACQUIRE) != 0) {
    return EBUSY;
  }
  return 0;
}

int pthread_spin_lock(pthread_spinlock_t* lock) {
  if (lock == 0) {
    return EINVAL;
  }
  while (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE) != 0) {
    while (__atomic_load_n(lock, __ATOMIC_RELAXED) != 0) {
      sched_yield();
    }
  }
  return 0;
}

int pthread_spin_trylock(pthread_spinlock_t* lock) {
  int expected = 0;

  if (lock == 0) {
    return EINVAL;
  }
  return __atomic_compare_exchange_n(
             lock, &expected, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)
             ? 0
             : EBUSY;
}

int pthread_spin_unlock(pthread_spinlock_t* lock) {
  if (lock == 0) {
    return EINVAL;
  }
  if (__atomic_load_n(lock, __ATOMIC_ACQUIRE) == 0) {
    return EPERM;
  }
  __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
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
      pthread_key_destructors[i] = destructor;
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
    pthread_key_destructors[key] = 0;
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

int pthread_attr_init(pthread_attr_t* attr) {
  if (attr == 0) {
    return EINVAL;
  }
  attr->flags = 0;
  attr->stack_base = 0;
  attr->stack_size = CRT_PTHREAD_STACK_SIZE;
  attr->guard_size = 0;
  attr->sched_policy = 0;
  attr->sched_priority = 0;
  return 0;
}

int pthread_attr_destroy(pthread_attr_t* attr) {
  if (attr == 0) {
    return EINVAL;
  }
  return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* state) {
  if (attr == 0 || state == 0) {
    return EINVAL;
  }
  *state = (attr->flags & CRT_PTHREAD_ATTR_FLAG_DETACHED) != 0
               ? PTHREAD_CREATE_DETACHED
               : PTHREAD_CREATE_JOINABLE;
  return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t* attr, int state) {
  if (attr == 0) {
    return EINVAL;
  }
  if (state == PTHREAD_CREATE_DETACHED) {
    attr->flags |= CRT_PTHREAD_ATTR_FLAG_DETACHED;
    return 0;
  }
  if (state == PTHREAD_CREATE_JOINABLE) {
    attr->flags &= ~CRT_PTHREAD_ATTR_FLAG_DETACHED;
    return 0;
  }
  return EINVAL;
}

int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stack_size) {
  if (attr == 0 || stack_size == 0) {
    return EINVAL;
  }
  *stack_size = attr->stack_size;
  return 0;
}

int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stack_size) {
  if (attr == 0 || stack_size < PTHREAD_STACK_MIN) {
    return EINVAL;
  }
  attr->stack_size = stack_size;
  return 0;
}

int pthread_attr_getguardsize(const pthread_attr_t* attr, size_t* guard_size) {
  if (attr == 0 || guard_size == 0) {
    return EINVAL;
  }
  *guard_size = attr->guard_size;
  return 0;
}

int pthread_attr_setguardsize(pthread_attr_t* attr, size_t guard_size) {
  if (attr == 0) {
    return EINVAL;
  }
  attr->guard_size = guard_size;
  return 0;
}

int pthread_attr_getstack(const pthread_attr_t* attr, void** stack_addr, size_t* stack_size) {
  if (attr == 0 || stack_addr == 0 || stack_size == 0) {
    return EINVAL;
  }
  *stack_addr = attr->stack_base;
  *stack_size = attr->stack_size;
  return 0;
}

int pthread_attr_setstack(pthread_attr_t* attr, void* stack_addr, size_t stack_size) {
  if (attr == 0 || stack_addr == 0 || stack_size < PTHREAD_STACK_MIN) {
    return EINVAL;
  }
  attr->stack_base = stack_addr;
  attr->stack_size = stack_size;
  attr->flags |= CRT_PTHREAD_ATTR_FLAG_STACK_USER;
  return 0;
}

int pthread_create(
    pthread_t* thread,
    const pthread_attr_t* attr,
    void* (*start_routine)(void*),
    void* arg) {
  crt_pthread_control* control;

  if (thread == 0 || start_routine == 0) {
    return EINVAL;
  }

  control = (crt_pthread_control*)calloc(1, sizeof(crt_pthread_control));
  if (control == 0) {
    return EAGAIN;
  }
  control->start_routine = start_routine;
  control->arg = arg;
  control->detached =
      attr != 0 && (attr->flags & CRT_PTHREAD_ATTR_FLAG_DETACHED) != 0;

#if defined(CRT_TARGET_OS_WINDOWS)
  control->handle = CreateThread(
      0, attr != 0 ? attr->stack_size : 0, pthread_windows_start, control, 0, &control->thread_id);
  if (control->handle == 0) {
    free(control);
    return EAGAIN;
  }
  if (control->detached) {
    CloseHandle(control->handle);
    control->handle = 0;
  }
  *thread = (pthread_t)(uintptr_t)control;
  return 0;
#elif defined(CRT_TARGET_OS_LINUX)
  if (control->detached) {
    int reaper_result = linux_reaper_ensure_started();
    if (reaper_result != 0) {
      free(control);
      return reaper_result;
    }
  }
  control->stack_size = attr != 0 ? attr->stack_size : CRT_PTHREAD_STACK_SIZE;
  if (attr != 0 && (attr->flags & CRT_PTHREAD_ATTR_FLAG_STACK_USER) != 0) {
    control->stack = attr->stack_base;
    control->stack_owned = 0;
  } else {
    control->stack = mmap(0, control->stack_size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (control->stack == MAP_FAILED) {
      free(control);
      return EAGAIN;
    }
    control->stack_owned = 1;
  }
  control->tid = __crt_sys_clone_thread(
      (char*)control->stack + control->stack_size,
      pthread_start,
      control,
      CRT_CLONE_THREAD_FLAGS,
      &control->tid_word,
      &control->tid_word,
      0);
  if (control->tid < 0) {
    if (control->stack_owned) {
      munmap(control->stack, control->stack_size);
    }
    free(control);
    return -control->tid;
  }
  *thread = (pthread_t)(uintptr_t)control;
  return 0;
#elif defined(CRT_TARGET_OS_MACOS)
  {
    crt_macos_pthread_create_fn create_fn =
        (crt_macos_pthread_create_fn)dlsym(CRT_RTLD_NEXT, "pthread_create");
    int result;

    if (create_fn == 0) {
      free(control);
      return ENOSYS;
    }
    result = create_fn(&control->native_thread, 0, pthread_macos_start, control);
    if (result != 0) {
      free(control);
      return result;
    }
    if (control->detached) {
      crt_macos_pthread_detach_fn detach_fn =
          (crt_macos_pthread_detach_fn)dlsym(CRT_RTLD_NEXT, "pthread_detach");
      if (detach_fn == 0) {
        return ENOSYS;
      }
      result = detach_fn(control->native_thread);
      if (result != 0) {
        return result;
      }
    }
  }
  *thread = (pthread_t)(uintptr_t)control;
  return 0;
#else
  free(control);
  return ENOSYS;
#endif
}

int pthread_detach(pthread_t thread) {
  crt_pthread_control* control = (crt_pthread_control*)(uintptr_t)thread;
  int expected = 0;

  if (control == 0) {
    return EINVAL;
  }
#if defined(CRT_TARGET_OS_LINUX)
  {
    int reaper_result = linux_reaper_ensure_started();
    if (reaper_result != 0) {
      return reaper_result;
    }
  }
#endif
  if (!__atomic_compare_exchange_n(
          &control->detached, &expected, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
    return EINVAL;
  }

#if defined(CRT_TARGET_OS_WINDOWS)
  if (control->handle != 0) {
    CloseHandle(control->handle);
    control->handle = 0;
  }
  return 0;
#elif defined(CRT_TARGET_OS_MACOS)
  {
    crt_macos_pthread_detach_fn detach_fn =
        (crt_macos_pthread_detach_fn)dlsym(CRT_RTLD_NEXT, "pthread_detach");
    if (detach_fn == 0) {
      return ENOSYS;
    }
    return detach_fn(control->native_thread);
  }
#else
#if defined(CRT_TARGET_OS_LINUX)
  if (__atomic_load_n(&control->tid_word, __ATOMIC_ACQUIRE) == 0) {
    linux_reap_enqueue(control);
  }
#endif
  return 0;
#endif
}

int pthread_join(pthread_t thread, void** retval) {
  crt_pthread_control* control = (crt_pthread_control*)(uintptr_t)thread;

#if !defined(CRT_TARGET_OS_WINDOWS) && !defined(CRT_TARGET_OS_LINUX)
  (void)retval;
#endif

  if (control == 0) {
    return EINVAL;
  }
  if (__atomic_load_n(&control->detached, __ATOMIC_ACQUIRE)) {
    return EINVAL;
  }

#if defined(CRT_TARGET_OS_WINDOWS)
  if (WaitForSingleObject(control->handle, CRT_INFINITE) != CRT_WAIT_OBJECT_0) {
    return EINVAL;
  }
  if (retval != 0) {
    *retval = control->result;
  }
  CloseHandle(control->handle);
  pthread_control_destroy(control);
  return 0;
#elif defined(CRT_TARGET_OS_LINUX)
  while (__atomic_load_n(&control->tid_word, __ATOMIC_ACQUIRE) != 0) {
    int tid = __atomic_load_n(&control->tid_word, __ATOMIC_ACQUIRE);
    long wait_result = __crt_sys_futex(&control->tid_word, CRT_FUTEX_WAIT, tid, 0, 0, 0);
    if (wait_result < 0 && wait_result != -EINTR && wait_result != -EAGAIN) {
      return (int)-wait_result;
    }
  }
  if (retval != 0) {
    *retval = control->result;
  }
  pthread_control_destroy(control);
  return 0;
#elif defined(CRT_TARGET_OS_MACOS)
  {
    crt_macos_pthread_join_fn join_fn =
        (crt_macos_pthread_join_fn)dlsym(CRT_RTLD_NEXT, "pthread_join");
    void* native_result = 0;
    int result;

    if (join_fn == 0) {
      return ENOSYS;
    }
    result = join_fn(control->native_thread, &native_result);
    if (result != 0) {
      return result;
    }
    (void)native_result;
  }
  if (retval != 0) {
    *retval = control->result;
  }
  pthread_control_destroy(control);
  return 0;
#else
  return ENOSYS;
#endif
}

void pthread_exit(void* retval) {
  crt_pthread_control* control = pthread_get_current_control();
  int detached = 0;

  if (control != 0) {
    control->result = retval;
    detached = __atomic_load_n(&control->detached, __ATOMIC_ACQUIRE);
  }
  pthread_run_key_destructors();
  pthread_set_current_control(0);
  if (detached) {
    pthread_control_destroy_from_worker(control);
  }
#if defined(CRT_TARGET_OS_WINDOWS)
  ExitThread(0);
#elif defined(CRT_TARGET_OS_MACOS)
  {
    crt_macos_pthread_exit_fn exit_fn =
        (crt_macos_pthread_exit_fn)dlsym(CRT_RTLD_NEXT, "pthread_exit");
    if (exit_fn != 0) {
      exit_fn(retval);
    }
  }
  __crt_sys_thread_exit(0);
#else
  __crt_sys_thread_exit(0);
#endif
}
