#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#include <private/crt_atomic.h>
#include <private/crt_tls.h>
#include <private/crt_wait.h>
#if defined(CRT_TARGET_OS_MACOS)
#include <private/crt_macho_symbol.h>
#endif

long __crt_sys_thread_id(void);
void __crt_sys_thread_exit(int status) __attribute__((noreturn));

#define CRT_PTHREAD_KEYS_MAX 128
#define CRT_PTHREAD_DESTRUCTOR_ITERATIONS 4
#define CRT_PTHREAD_STACK_SIZE (1024UL * 1024UL)
#define CRT_PTHREAD_GUARD_SIZE 4096UL
#define CRT_PTHREAD_NAME_MAX 16
#define CRT_PTHREAD_ATTR_FLAG_DETACHED 0x00000001U
#define CRT_PTHREAD_ATTR_FLAG_STACK_USER 0x00000002U
#define CRT_PTHREAD_ATTR_FLAG_EXPLICIT_SCHED 0x00000004U
#define CRT_MUTEXATTR_TYPE_MASK 0x000000ffL
#define CRT_MUTEXATTR_PSHARED_BIT 0x00000100L
#define CRT_MUTEXATTR_ROBUST_BIT 0x00000200L
#define CRT_RWLOCKATTR_PSHARED_BIT 0x00000001L
#define CRT_BARRIERATTR_PSHARED_BIT 0x00000001L
#define CRT_CONDATTR_CLOCK_MASK 0x000000ffL
#define CRT_CONDATTR_PSHARED_BIT 0x00000100L
#define CRT_COND_SEQUENCE_WORD 0
#define CRT_COND_WAITERS_WORD 1
#define CRT_COND_SHARED_WORD 2
/* A real, confirmed-for-real gap found and fixed 2026-09-03 (libcrtgfx's
 * own crtgfx_gpu_fence, built the same day, needed a real monotonic-
 * clock cond timeout and found this instead): pthread_condattr_setclock()
 * always correctly stored the requested clock in the condattr's own
 * bits, and pthread_condattr_getclock() always correctly read it back --
 * but nothing downstream ever consulted it. pthread_cond_init() never
 * captured which clock a cond var was actually created with anywhere in
 * its own real storage, and pthread_cond_timedwait() unconditionally
 * treated every `abstime` as a real CLOCK_REALTIME deadline regardless,
 * silently making PTHREAD_COND_CLOCK_MONOTONIC a real no-op: a caller
 * doing the textbook-correct pthread_condattr_setclock(&attr,
 * PTHREAD_COND_CLOCK_MONOTONIC) -> pthread_cond_init(&cond, &attr) ->
 * clock_gettime(CLOCK_MONOTONIC, &ts) + offset -> pthread_cond_timedwait()
 * sequence got a deadline expressed in CLOCK_MONOTONIC's own epoch
 * (typically "time since boot", a small number) misread as a
 * CLOCK_REALTIME one (seconds since 1970, a huge number) -- already far
 * in the real past, so pthread_cond_timedwait() returned ETIMEDOUT
 * instantly every real time instead of actually waiting. Confirmed for
 * real on native Windows via a real, failing crtgfx_gpu_test before this
 * fix landed. This word is the real fix: cond_clock_id() (below) reads
 * it back, cond_timedwait's own real wait loop uses it instead of always
 * calling realtime_until(). */
#define CRT_COND_CLOCK_WORD 3
#define CRT_BARRIER_COUNT_WORD 0
#define CRT_BARRIER_WAITERS_WORD 1
#define CRT_BARRIER_GENERATION_WORD 2
#define CRT_BARRIER_SHARED_WORD 3
#define CRT_MUTEX_STATE_WORD 0
#define CRT_MUTEX_TYPE_WORD 1
#define CRT_MUTEX_COUNT_WORD 2
#define CRT_MUTEX_OWNER_LOW_WORD 3
#define CRT_MUTEX_OWNER_HIGH_WORD 4
#define CRT_MUTEX_SHARED_WORD 5
#define CRT_RWLOCK_STATE_WORD 0
#define CRT_RWLOCK_SHARED_WORD 1
#define CRT_RWLOCK_WRITER_STATE (-1)

/*
 * Real, cross-process PTHREAD_PROCESS_SHARED support is only tractable on
 * Linux (non-private futex ops) and macOS (os_sync_wait_on_address's
 * SHARED flag, reasoned but unverified -- see wait.c). WaitOnAddress and
 * friends on Windows are documented same-process-only with no way to opt
 * into cross-process waiting, so pshared objects stay ENOTSUP there --
 * an honest architectural limitation, not a missing implementation.
 */
#if defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_MACOS)
#define CRT_PSHARED_SUPPORTED 1
#else
#define CRT_PSHARED_SUPPORTED 0
#endif

#if defined(CRT_TARGET_OS_WINDOWS)
typedef unsigned long DWORD;
typedef int BOOL;
typedef void* HANDLE;

#define CRT_WINAPI
#define CRT_TLS_OUT_OF_INDEXES ((DWORD)0xffffffffUL)
#define CRT_WAIT_OBJECT_0 0
#define CRT_INFINITE 0xffffffffUL

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
typedef int (*crt_macos_pthread_attr_init_fn)(void*);
typedef int (*crt_macos_pthread_attr_destroy_fn)(void*);
typedef int (*crt_macos_pthread_attr_setstack_fn)(void*, void*, size_t);
typedef int (*crt_macos_pthread_attr_setstacksize_fn)(void*, size_t);
typedef int (*crt_macos_pthread_attr_setguardsize_fn)(void*, size_t);
typedef int (*crt_macos_pthread_join_fn)(crt_macos_pthread_t, void**);
typedef int (*crt_macos_pthread_detach_fn)(crt_macos_pthread_t);
typedef void (*crt_macos_pthread_exit_fn)(void*) __attribute__((noreturn));

#define CRT_RTLD_NEXT ((void*)-1)

void* dlsym(void* handle, const char* symbol);

/* Every real pthread_* entry point this file calls through to (thread
 * creation itself has no raw Darwin syscall this project can drive
 * directly the way Linux's clone(2)-based path above does, so macOS
 * threads are real Apple pthreads underneath, created via Apple's own
 * libsystem_pthread.dylib) -- looked up by exact dylib path with
 * __crt_macho_find_symbol_in_loaded_image(), the same real-Darwin-
 * symbol pattern already used for getaddrinfo()/getifaddrs() (see
 * socket.c's macos_host_resolve_hostname(), ifaddrs.c), NOT
 * dlsym(RTLD_NEXT, ...): this project's own RTLD_NEXT is explicitly
 * unimplemented (libdl/src/arch/macos/dl_macos.c's
 * crt_dl_backend_sym()) and libdl.a's own dlsym() -- not Apple's --
 * is what every crt-cc-linked binary actually gets (tools/crt-cc
 * force-links libdl.a ahead of -lSystem for every macOS target), so
 * dlsym(CRT_RTLD_NEXT, "pthread_create") always failed and every
 * pthread_create() on macOS always returned ENOSYS. Found while
 * chasing curl's own threaded-resolver hang: its DNS worker thread
 * never spawned at all, and Curl_thrdq_send() discards
 * Curl_thrdpool_signal()'s error, so the query just sat on the queue
 * forever and curl_multi_wakeup() never fired -- see HISTORY.md's
 * dated entry. The dlsym(RTLD_NEXT, ...) call stays as a second
 * attempt only for a build where the exact dylib path ever moves. */
static void* crt_macos_pthread_symbol(const char* name) {
  void* sym = __crt_macho_find_symbol_in_loaded_image(
      "/usr/lib/system/libsystem_pthread.dylib", name);
  if (sym == 0) {
    sym = dlsym(CRT_RTLD_NEXT, name);
  }
  return sym;
}
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
  void* mapping;
  unsigned long mapping_size;
  int stack_owned;
  int reap_queued;
  struct crt_pthread_control* reap_next;
#elif defined(CRT_TARGET_OS_MACOS)
  crt_macos_pthread_t native_thread;
#endif
  crt_thread_context context;
  pthread_attr_t attr;
  void* (*start_routine)(void*);
  void* arg;
  void* result;
  int detached;
} crt_pthread_control;

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

static crt_atomic_int* barrier_waiters(pthread_barrier_t* barrier) {
  return (crt_atomic_int*)&barrier->__private[CRT_BARRIER_WAITERS_WORD];
}

static crt_atomic_int* barrier_generation(pthread_barrier_t* barrier) {
  return (crt_atomic_int*)&barrier->__private[CRT_BARRIER_GENERATION_WORD];
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

static int mutex_shared(const pthread_mutex_t* mutex) {
  return mutex->__private[CRT_MUTEX_SHARED_WORD] != 0;
}

static int rwlock_shared(const pthread_rwlock_t* rwlock) {
  return rwlock->__private[CRT_RWLOCK_SHARED_WORD] != 0;
}

static int cond_shared(const pthread_cond_t* cond) {
  return cond->__private[CRT_COND_SHARED_WORD] != 0;
}

/* Real clock this cond var was actually configured with (CRT_COND_CLOCK_
 * WORD, see that define's own comment) -- CLOCK_MONOTONIC only when
 * pthread_cond_init() was given a real attr with PTHREAD_COND_CLOCK_
 * MONOTONIC; CLOCK_REALTIME otherwise, matching a statically-initialized
 * PTHREAD_COND_INITIALIZER cond var's own real, zero-filled storage and
 * pthread_condattr_init()'s own real PTHREAD_COND_CLOCK_REALTIME
 * default. */
static clockid_t cond_clock_id(const pthread_cond_t* cond) {
  return (cond->__private[CRT_COND_CLOCK_WORD] == PTHREAD_COND_CLOCK_MONOTONIC) ? CLOCK_MONOTONIC : CLOCK_REALTIME;
}

static int barrier_shared(const pthread_barrier_t* barrier) {
  return barrier->__private[CRT_BARRIER_SHARED_WORD] != 0;
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

/* Real, clock-parameterized deadline-to-remaining-time computation --
 * shared by realtime_until() (below, always CLOCK_REALTIME -- POSIX
 * gives pthread_mutex_timedlock() no clock-attribute concept at all, so
 * its own real caller must keep using this exact real behavior
 * unconditionally) and pthread_cond_timedwait()'s own real wait loop
 * (which, unlike a mutex, must honor a real cond var's own configured
 * clock -- see CRT_COND_CLOCK_WORD's own comment above for the real gap
 * this exists to close). */
static int clock_until(clockid_t clock_id, const struct timespec* abstime, struct timespec* remaining) {
  struct timespec now;
  time_t sec;
  long nsec;

  if (abstime == 0 || remaining == 0 || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000L) {
    return EINVAL;
  }
  if (abstime->tv_sec < 0 || (abstime->tv_sec == 0 && abstime->tv_nsec == 0)) {
    return ETIMEDOUT;
  }
  if (clock_gettime(clock_id, &now) != 0) {
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

static int realtime_until(const struct timespec* abstime, struct timespec* remaining) {
  return clock_until(CLOCK_REALTIME, abstime, remaining);
}

static int pthread_key_is_valid(pthread_key_t key) {
  return key >= 0 && key < CRT_PTHREAD_KEYS_MAX && pthread_key_used[key] != 0;
}

static int pthread_is_current_thread(pthread_t thread) {
  return thread != 0 && thread == pthread_self();
}

static crt_pthread_control* pthread_get_current_control(void) {
  return (crt_pthread_control*)__crt_thread_control();
}

static crt_pthread_control* pthread_control_from_thread(pthread_t thread) {
  if (thread == 0) {
    return 0;
  }
  if (pthread_is_current_thread(thread)) {
    return pthread_get_current_control();
  }
  return (crt_pthread_control*)(uintptr_t)thread;
}

void __crt_pthread_after_fork_child(void) {
  pthread_key_lock.state.value = 0;
#if defined(CRT_TARGET_OS_LINUX)
  linux_reap_lock.state.value = 0;
  linux_reap_sequence.value = 0;
  linux_reap_head = 0;
  linux_reaper_started = 0;
  linux_reaper_tid = 0;
  linux_reaper_stack = 0;
#endif
}

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
        {
          void** values = __crt_thread_key_values();
          value = values[i];
          if (value != 0 && destructor != 0) {
            values[i] = 0;
          }
        }
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
  __crt_thread_clear_current(&control->context);
  if (control->stack_owned && control->mapping != 0) {
    munmap(control->mapping, control->mapping_size);
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

  __crt_thread_set_current(&control->context);
  control->result = control->start_routine(control->arg);
  pthread_run_key_destructors();
  __crt_thread_clear_current(&control->context);
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

int pthread_barrier_init(
    pthread_barrier_t* barrier,
    const pthread_barrierattr_t* attr,
    unsigned int count) {
  if (barrier == 0 || count == 0) {
    return EINVAL;
  }
  barrier->__private[CRT_BARRIER_COUNT_WORD] = (int32_t)count;
  barrier->__private[CRT_BARRIER_WAITERS_WORD] = 0;
  barrier->__private[CRT_BARRIER_GENERATION_WORD] = 0;
  barrier->__private[CRT_BARRIER_SHARED_WORD] =
      (attr != 0 && (*attr & CRT_BARRIERATTR_PSHARED_BIT) != 0) ? 1 : 0;
  return 0;
}

int pthread_barrier_destroy(pthread_barrier_t* barrier) {
  if (barrier == 0) {
    return EINVAL;
  }
  if (crt_atomic_load_acquire(barrier_waiters(barrier)) != 0) {
    return EBUSY;
  }
  return 0;
}

int pthread_barrier_wait(pthread_barrier_t* barrier) {
  int count;
  int generation;
  int waiters;

  if (barrier == 0) {
    return EINVAL;
  }
  count = barrier->__private[CRT_BARRIER_COUNT_WORD];
  if (count <= 0) {
    return EINVAL;
  }
  generation = crt_atomic_load_acquire(barrier_generation(barrier));
  waiters = crt_atomic_fetch_add_acq_rel(barrier_waiters(barrier), 1) + 1;
  if (waiters == count) {
    int shared = barrier_shared(barrier);

    crt_atomic_store_release(barrier_waiters(barrier), 0);
    crt_atomic_fetch_add_acq_rel(barrier_generation(barrier), 1);
    if (shared) {
      __crt_wake32_all_shared(&barrier->__private[CRT_BARRIER_GENERATION_WORD]);
    } else {
      __crt_wake32_all(&barrier->__private[CRT_BARRIER_GENERATION_WORD]);
    }
    return PTHREAD_BARRIER_SERIAL_THREAD;
  }

  while (crt_atomic_load_acquire(barrier_generation(barrier)) == generation) {
    int result = barrier_shared(barrier)
                      ? __crt_wait32_shared(&barrier->__private[CRT_BARRIER_GENERATION_WORD], generation)
                      : __crt_wait32(&barrier->__private[CRT_BARRIER_GENERATION_WORD], generation);
    if (result != 0 && result != EINTR && result != EAGAIN) {
      return result;
    }
  }
  return 0;
}

int pthread_barrierattr_init(pthread_barrierattr_t* attr) {
  if (attr == 0) {
    return EINVAL;
  }
  *attr = 0;
  return 0;
}

int pthread_barrierattr_destroy(pthread_barrierattr_t* attr) {
  if (attr == 0) {
    return EINVAL;
  }
  return 0;
}

int pthread_barrierattr_getpshared(const pthread_barrierattr_t* attr, int* pshared) {
  if (attr == 0 || pshared == 0) {
    return EINVAL;
  }
  *pshared = (*attr & CRT_BARRIERATTR_PSHARED_BIT) != 0
                 ? PTHREAD_PROCESS_SHARED
                 : PTHREAD_PROCESS_PRIVATE;
  return 0;
}

int pthread_barrierattr_setpshared(pthread_barrierattr_t* attr, int pshared) {
  if (attr == 0) {
    return EINVAL;
  }
  if (pshared == PTHREAD_PROCESS_PRIVATE) {
    *attr &= ~CRT_BARRIERATTR_PSHARED_BIT;
    return 0;
  }
  if (pshared == PTHREAD_PROCESS_SHARED) {
#if CRT_PSHARED_SUPPORTED
    *attr |= CRT_BARRIERATTR_PSHARED_BIT;
    return 0;
#else
    return ENOTSUP;
#endif
  }
  return EINVAL;
}

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
  if (cond == 0) {
    return EINVAL;
  }
  cond->__private[CRT_COND_SEQUENCE_WORD] = 0;
  cond->__private[CRT_COND_WAITERS_WORD] = 0;
  cond->__private[CRT_COND_SHARED_WORD] =
      (attr != 0 && (*attr & CRT_CONDATTR_PSHARED_BIT) != 0) ? 1 : 0;
  /* Real fix, see CRT_COND_CLOCK_WORD's own comment above: actually
   * capture attr's own real configured clock (PTHREAD_COND_CLOCK_
   * REALTIME when attr is null, matching pthread_condattr_init()'s own
   * real default) instead of silently discarding it. */
  cond->__private[CRT_COND_CLOCK_WORD] =
      (attr != 0) ? (int32_t)(*attr & CRT_CONDATTR_CLOCK_MASK) : PTHREAD_COND_CLOCK_REALTIME;
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
  return cond_shared(cond) ? __crt_wake32_one_shared(&cond->__private[CRT_COND_SEQUENCE_WORD])
                            : __crt_wake32_one(&cond->__private[CRT_COND_SEQUENCE_WORD]);
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
  if (cond == 0) {
    return EINVAL;
  }
  crt_atomic_fetch_add_acq_rel(cond_sequence(cond), 1);
  return cond_shared(cond) ? __crt_wake32_all_shared(&cond->__private[CRT_COND_SEQUENCE_WORD])
                            : __crt_wake32_all(&cond->__private[CRT_COND_SEQUENCE_WORD]);
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
    result = cond_shared(cond)
                 ? __crt_wait32_shared(&cond->__private[CRT_COND_SEQUENCE_WORD], sequence)
                 : __crt_wait32(&cond->__private[CRT_COND_SEQUENCE_WORD], sequence);
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

    /* Real fix, see CRT_COND_CLOCK_WORD's own comment above: honors this
     * cond var's own actually-configured clock instead of always
     * silently treating `abstime` as CLOCK_REALTIME. */
    result = clock_until(cond_clock_id(cond), abstime, &remaining);
    if (result != 0) {
      break;
    }
    result = cond_shared(cond)
                 ? __crt_wait32_timed_shared(&cond->__private[CRT_COND_SEQUENCE_WORD], sequence, &remaining)
                 : __crt_wait32_timed(&cond->__private[CRT_COND_SEQUENCE_WORD], sequence, &remaining);
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
  *clock_id = (int)(*attr & CRT_CONDATTR_CLOCK_MASK);
  return 0;
}

int pthread_condattr_setclock(pthread_condattr_t* attr, int clock_id) {
  if (attr == 0) {
    return EINVAL;
  }
  if (clock_id != PTHREAD_COND_CLOCK_REALTIME && clock_id != PTHREAD_COND_CLOCK_MONOTONIC) {
    return EINVAL;
  }
  *attr = (*attr & ~CRT_CONDATTR_CLOCK_MASK) | clock_id;
  return 0;
}

int pthread_condattr_getpshared(const pthread_condattr_t* attr, int* pshared) {
  if (attr == 0 || pshared == 0) {
    return EINVAL;
  }
  *pshared = (*attr & CRT_CONDATTR_PSHARED_BIT) != 0 ? PTHREAD_PROCESS_SHARED
                                                      : PTHREAD_PROCESS_PRIVATE;
  return 0;
}

int pthread_condattr_setpshared(pthread_condattr_t* attr, int pshared) {
  if (attr == 0) {
    return EINVAL;
  }
  if (pshared == PTHREAD_PROCESS_PRIVATE) {
    *attr &= ~CRT_CONDATTR_PSHARED_BIT;
    return 0;
  }
  if (pshared == PTHREAD_PROCESS_SHARED) {
#if CRT_PSHARED_SUPPORTED
    *attr |= CRT_CONDATTR_PSHARED_BIT;
    return 0;
#else
    return ENOTSUP;
#endif
  }
  return EINVAL;
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
    if ((*mutex_attr & CRT_MUTEXATTR_ROBUST_BIT) != 0) {
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
  mutex->__private[CRT_MUTEX_SHARED_WORD] =
      (mutex_attr != 0 && (*mutex_attr & CRT_MUTEXATTR_PSHARED_BIT) != 0) ? 1 : 0;
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
      wait_result = mutex_shared(mutex)
                         ? __crt_wait32_shared(&mutex->__private[CRT_MUTEX_STATE_WORD], 1)
                         : __crt_wait32(&mutex->__private[CRT_MUTEX_STATE_WORD], 1);
      if (wait_result != 0 && wait_result != EINTR && wait_result != EAGAIN) {
        return wait_result;
      }
    }
  }
  set_mutex_owner(mutex, self);
  mutex->__private[CRT_MUTEX_COUNT_WORD] = 1;
  return 0;
}

int pthread_mutex_timedlock(pthread_mutex_t* mutex, const struct timespec* abstime) {
  pthread_t self;
  int type;
  int expected;
  int wait_result;

  if (mutex == 0 || abstime == 0) {
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
      struct timespec remaining;
      int until_result = realtime_until(abstime, &remaining);

      if (until_result != 0) {
        return until_result;
      }
      wait_result = mutex_shared(mutex)
                         ? __crt_wait32_timed_shared(&mutex->__private[CRT_MUTEX_STATE_WORD], 1, &remaining)
                         : __crt_wait32_timed(&mutex->__private[CRT_MUTEX_STATE_WORD], 1, &remaining);
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
  return mutex_shared(mutex) ? __crt_wake32_one_shared(&mutex->__private[CRT_MUTEX_STATE_WORD])
                              : __crt_wake32_one(&mutex->__private[CRT_MUTEX_STATE_WORD]);
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
#if CRT_PSHARED_SUPPORTED
    *attr |= CRT_MUTEXATTR_PSHARED_BIT;
    return 0;
#else
    return ENOTSUP;
#endif
  }
  return EINVAL;
}

int pthread_mutexattr_getrobust(const pthread_mutexattr_t* attr, int* robust) {
  if (attr == 0 || robust == 0) {
    return EINVAL;
  }
  *robust = (*attr & CRT_MUTEXATTR_ROBUST_BIT) != 0
                ? PTHREAD_MUTEX_ROBUST
                : PTHREAD_MUTEX_STALLED;
  return 0;
}

int pthread_mutexattr_setrobust(pthread_mutexattr_t* attr, int robust) {
  if (attr == 0) {
    return EINVAL;
  }
  if (robust == PTHREAD_MUTEX_STALLED) {
    *attr &= ~CRT_MUTEXATTR_ROBUST_BIT;
    return 0;
  }
  if (robust == PTHREAD_MUTEX_ROBUST) {
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
  rwlock->__private[CRT_RWLOCK_STATE_WORD] = 0;
  rwlock->__private[CRT_RWLOCK_SHARED_WORD] =
      (attr != 0 && (*attr & CRT_RWLOCKATTR_PSHARED_BIT) != 0) ? 1 : 0;
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
    wait_result = rwlock_shared(rwlock)
                      ? __crt_wait32_shared(&rwlock->__private[CRT_RWLOCK_STATE_WORD], state)
                      : __crt_wait32(&rwlock->__private[CRT_RWLOCK_STATE_WORD], state);
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
    wait_result = rwlock_shared(rwlock)
                      ? __crt_wait32_shared(&rwlock->__private[CRT_RWLOCK_STATE_WORD], state)
                      : __crt_wait32(&rwlock->__private[CRT_RWLOCK_STATE_WORD], state);
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
    return rwlock_shared(rwlock) ? __crt_wake32_all_shared(&rwlock->__private[CRT_RWLOCK_STATE_WORD])
                                  : __crt_wake32_all(&rwlock->__private[CRT_RWLOCK_STATE_WORD]);
  }
  if (state > 0) {
    int previous = crt_atomic_fetch_add_acq_rel(rwlock_state(rwlock), -1);
    if (previous == 1) {
      return rwlock_shared(rwlock) ? __crt_wake32_all_shared(&rwlock->__private[CRT_RWLOCK_STATE_WORD])
                                    : __crt_wake32_all(&rwlock->__private[CRT_RWLOCK_STATE_WORD]);
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
#if CRT_PSHARED_SUPPORTED
    *attr |= CRT_RWLOCKATTR_PSHARED_BIT;
    return 0;
#else
    return ENOTSUP;
#endif
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
  /*
   * Unlike mutex/rwlock/cond/barrier, the spinlock below never calls into
   * an OS wait/wake primitive -- lock/trylock/unlock are pure __atomic_*
   * builtins on a plain int. Atomic CPU instructions on genuinely shared
   * memory behave correctly across process boundaries on every host this
   * project targets, so PTHREAD_PROCESS_SHARED is real and unconditional
   * here, including on Windows where the futex-backed primitives above
   * stay ENOTSUP.
   */
  if (pshared != PTHREAD_PROCESS_SHARED && pshared != PTHREAD_PROCESS_PRIVATE) {
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
  crt_pthread_control* control = pthread_get_current_control();

  if (control != 0) {
    return (pthread_t)(uintptr_t)control;
  }
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

  crt_spin_lock(&pthread_key_lock);
  for (i = 0; i < CRT_PTHREAD_KEYS_MAX; ++i) {
    if (pthread_key_used[i] == 0) {
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

  crt_spin_lock(&pthread_key_lock);
  if (!pthread_key_is_valid(key)) {
    result = EINVAL;
  } else {
    __crt_thread_key_values()[key] = 0;
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
  return __crt_thread_key_values()[key];
}

int pthread_setspecific(pthread_key_t key, const void* value) {
  if (!pthread_key_is_valid(key)) {
    return EINVAL;
  }
  __crt_thread_key_values()[key] = (void*)value;
  return 0;
}

int pthread_attr_init(pthread_attr_t* attr) {
  if (attr == 0) {
    return EINVAL;
  }
  attr->flags = 0;
  attr->stack_base = 0;
  attr->stack_size = CRT_PTHREAD_STACK_SIZE;
  attr->guard_size = CRT_PTHREAD_GUARD_SIZE;
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
  if (guard_size > (size_t)LONG_MAX) {
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
  attr->guard_size = 0;
  attr->flags |= CRT_PTHREAD_ATTR_FLAG_STACK_USER;
  return 0;
}

int pthread_attr_getinheritsched(const pthread_attr_t* attr, int* inheritsched) {
  if (attr == 0 || inheritsched == 0) {
    return EINVAL;
  }
  *inheritsched = (attr->flags & CRT_PTHREAD_ATTR_FLAG_EXPLICIT_SCHED) != 0
                      ? PTHREAD_EXPLICIT_SCHED
                      : PTHREAD_INHERIT_SCHED;
  return 0;
}

int pthread_attr_setinheritsched(pthread_attr_t* attr, int inheritsched) {
  if (attr == 0) {
    return EINVAL;
  }
  if (inheritsched == PTHREAD_INHERIT_SCHED) {
    attr->flags &= ~CRT_PTHREAD_ATTR_FLAG_EXPLICIT_SCHED;
    return 0;
  }
  if (inheritsched == PTHREAD_EXPLICIT_SCHED) {
    attr->flags |= CRT_PTHREAD_ATTR_FLAG_EXPLICIT_SCHED;
    return 0;
  }
  return EINVAL;
}

int pthread_attr_getschedpolicy(const pthread_attr_t* attr, int* policy) {
  if (attr == 0 || policy == 0) {
    return EINVAL;
  }
  *policy = attr->sched_policy;
  return 0;
}

int pthread_attr_setschedpolicy(pthread_attr_t* attr, int policy) {
  if (attr == 0) {
    return EINVAL;
  }
  if (policy == SCHED_OTHER || policy == SCHED_FIFO || policy == SCHED_RR) {
    attr->sched_policy = policy;
    return 0;
  }
  return EINVAL;
}

int pthread_attr_getschedparam(const pthread_attr_t* attr, struct sched_param* param) {
  if (attr == 0 || param == 0) {
    return EINVAL;
  }
  param->sched_priority = attr->sched_priority;
  return 0;
}

int pthread_attr_setschedparam(pthread_attr_t* attr, const struct sched_param* param) {
  if (attr == 0 || param == 0) {
    return EINVAL;
  }
  attr->sched_priority = param->sched_priority;
  return 0;
}

int pthread_attr_getscope(const pthread_attr_t* attr, int* scope) {
  if (attr == 0 || scope == 0) {
    return EINVAL;
  }
  *scope = PTHREAD_SCOPE_SYSTEM;
  return 0;
}

int pthread_attr_setscope(pthread_attr_t* attr, int scope) {
  if (attr == 0) {
    return EINVAL;
  }
  if (scope == PTHREAD_SCOPE_SYSTEM) {
    return 0;
  }
  if (scope == PTHREAD_SCOPE_PROCESS) {
    return ENOTSUP;
  }
  return EINVAL;
}

int pthread_getattr_np(pthread_t thread, pthread_attr_t* attr) {
  crt_pthread_control* control;

  if (thread == 0 || attr == 0) {
    return EINVAL;
  }
  control = pthread_control_from_thread(thread);
  if (control != 0) {
    *attr = control->attr;
    return 0;
  }
  return pthread_attr_init(attr);
}

int pthread_create(
    pthread_t* thread,
    const pthread_attr_t* attr,
    void* (*start_routine)(void*),
    void* arg) {
  crt_pthread_control* control;
  int* context_tid_word = 0;

  if (thread == 0 || start_routine == 0) {
    return EINVAL;
  }

  control = (crt_pthread_control*)calloc(1, sizeof(crt_pthread_control));
  if (control == 0) {
    return EAGAIN;
  }
  control->start_routine = start_routine;
  control->arg = arg;
#if defined(CRT_TARGET_OS_LINUX)
  context_tid_word = &control->tid_word;
#endif
  __crt_thread_context_init(&control->context, control, context_tid_word);
  if (attr != 0) {
    control->attr = *attr;
  } else {
    pthread_attr_init(&control->attr);
  }
  control->detached =
      (control->attr.flags & CRT_PTHREAD_ATTR_FLAG_DETACHED) != 0;

#if defined(CRT_TARGET_OS_WINDOWS)
  if ((control->attr.flags & CRT_PTHREAD_ATTR_FLAG_STACK_USER) != 0) {
    free(control);
    return ENOTSUP;
  }
  control->handle = CreateThread(
      0, control->attr.stack_size, pthread_windows_start, control, 0, &control->thread_id);
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
  control->stack_size = control->attr.stack_size;
  if ((control->attr.flags & CRT_PTHREAD_ATTR_FLAG_STACK_USER) != 0) {
    control->stack = control->attr.stack_base;
    control->mapping = 0;
    control->mapping_size = 0;
    control->stack_owned = 0;
  } else {
    unsigned long guard_size = (unsigned long)((control->attr.guard_size + 4095UL) & ~4095UL);
    unsigned long mapping_size;

    if (guard_size > ULONG_MAX - control->stack_size) {
      free(control);
      return EAGAIN;
    }
    mapping_size = control->stack_size + guard_size;
    control->mapping = mmap(0, mapping_size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (control->mapping == MAP_FAILED) {
      free(control);
      return EAGAIN;
    }
    control->mapping_size = mapping_size;
    control->stack = (char*)control->mapping + guard_size;
    control->stack_owned = 1;
    control->attr.stack_base = control->stack;
    control->attr.guard_size = guard_size;
    if (guard_size != 0 && mprotect(control->mapping, guard_size, PROT_NONE) != 0) {
      munmap(control->mapping, control->mapping_size);
      free(control);
      return EAGAIN;
    }
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
      munmap(control->mapping, control->mapping_size);
    }
    free(control);
    return -control->tid;
  }
  *thread = (pthread_t)(uintptr_t)control;
  return 0;
#elif defined(CRT_TARGET_OS_MACOS)
  {
    crt_macos_pthread_create_fn create_fn =
        (crt_macos_pthread_create_fn)crt_macos_pthread_symbol("pthread_create");
    crt_macos_pthread_attr_init_fn attr_init_fn =
        (crt_macos_pthread_attr_init_fn)crt_macos_pthread_symbol("pthread_attr_init");
    crt_macos_pthread_attr_destroy_fn attr_destroy_fn =
        (crt_macos_pthread_attr_destroy_fn)crt_macos_pthread_symbol("pthread_attr_destroy");
    crt_macos_pthread_attr_setstack_fn attr_setstack_fn =
        (crt_macos_pthread_attr_setstack_fn)crt_macos_pthread_symbol("pthread_attr_setstack");
    crt_macos_pthread_attr_setstacksize_fn attr_setstacksize_fn =
        (crt_macos_pthread_attr_setstacksize_fn)crt_macos_pthread_symbol("pthread_attr_setstacksize");
    crt_macos_pthread_attr_setguardsize_fn attr_setguardsize_fn =
        (crt_macos_pthread_attr_setguardsize_fn)crt_macos_pthread_symbol("pthread_attr_setguardsize");
    union {
      void* align_ptr;
      long long align_ll;
      char storage[128];
    } native_attr;
    void* native_attr_ptr = 0;
    int result;

    if (create_fn == 0) {
      free(control);
      return ENOSYS;
    }
    if (attr_init_fn != 0 && attr_destroy_fn != 0) {
      result = attr_init_fn(native_attr.storage);
      if (result != 0) {
        free(control);
        return result;
      }
      native_attr_ptr = native_attr.storage;
      if ((control->attr.flags & CRT_PTHREAD_ATTR_FLAG_STACK_USER) != 0) {
        if (attr_setstack_fn == 0) {
          attr_destroy_fn(native_attr_ptr);
          free(control);
          return ENOTSUP;
        }
        result = attr_setstack_fn(native_attr_ptr, control->attr.stack_base, control->attr.stack_size);
        if (result != 0) {
          attr_destroy_fn(native_attr_ptr);
          free(control);
          return result;
        }
      } else if (attr_setstacksize_fn != 0) {
        result = attr_setstacksize_fn(native_attr_ptr, control->attr.stack_size);
        if (result != 0) {
          attr_destroy_fn(native_attr_ptr);
          free(control);
          return result;
        }
      }
      if ((control->attr.flags & CRT_PTHREAD_ATTR_FLAG_STACK_USER) == 0 &&
          attr_setguardsize_fn != 0) {
        result = attr_setguardsize_fn(native_attr_ptr, control->attr.guard_size);
        if (result != 0) {
          attr_destroy_fn(native_attr_ptr);
          free(control);
          return result;
        }
      }
    }
    result = create_fn(&control->native_thread, native_attr_ptr, pthread_macos_start, control);
    if (native_attr_ptr != 0) {
      attr_destroy_fn(native_attr_ptr);
    }
    if (result != 0) {
      free(control);
      return result;
    }
    if (control->detached) {
      crt_macos_pthread_detach_fn detach_fn =
          (crt_macos_pthread_detach_fn)crt_macos_pthread_symbol("pthread_detach");
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
  crt_pthread_control* control = pthread_control_from_thread(thread);
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
        (crt_macos_pthread_detach_fn)crt_macos_pthread_symbol("pthread_detach");
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
  crt_pthread_control* control = pthread_control_from_thread(thread);

#if !defined(CRT_TARGET_OS_WINDOWS) && !defined(CRT_TARGET_OS_LINUX)
  (void)retval;
#endif

  if (control == 0) {
    return EINVAL;
  }
  if (pthread_is_current_thread(thread)) {
    return EDEADLK;
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
    if (wait_result == -EINVAL) {
      sched_yield();
      continue;
    }
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
        (crt_macos_pthread_join_fn)crt_macos_pthread_symbol("pthread_join");
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

int pthread_getschedparam(pthread_t thread, int* policy, struct sched_param* param) {
  if (thread == 0 || policy == 0 || param == 0) {
    return EINVAL;
  }
  *policy = SCHED_OTHER;
  param->sched_priority = 0;
  return 0;
}

int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param* param) {
  if (thread == 0 || param == 0) {
    return EINVAL;
  }
  if (policy == SCHED_OTHER && param->sched_priority == 0) {
    return 0;
  }
  if (policy == SCHED_FIFO || policy == SCHED_RR || param->sched_priority != 0) {
    return ENOTSUP;
  }
  return EINVAL;
}

int pthread_setschedprio(pthread_t thread, int priority) {
  struct sched_param param;

  if (thread == 0) {
    return EINVAL;
  }
  param.sched_priority = priority;
  return pthread_setschedparam(thread, SCHED_OTHER, &param);
}

pid_t pthread_gettid_np(pthread_t thread) {
  crt_pthread_control* control;

  if (thread == 0) {
    return (pid_t)-1;
  }
  if (pthread_is_current_thread(thread)) {
    return (pid_t)__crt_sys_thread_id();
  }
  control = (crt_pthread_control*)(uintptr_t)thread;
#if defined(CRT_TARGET_OS_WINDOWS)
  return (pid_t)control->thread_id;
#elif defined(CRT_TARGET_OS_LINUX)
  return (pid_t)control->tid;
#elif defined(CRT_TARGET_OS_MACOS)
  return (pid_t)(intptr_t)control->native_thread;
#else
  return (pid_t)-1;
#endif
}

int pthread_getcpuclockid(pthread_t thread, clockid_t* clock_id) {
  if (thread == 0 || clock_id == 0) {
    return EINVAL;
  }
  return ENOTSUP;
}

int pthread_setname_np(pthread_t thread, const char* name) {
  crt_pthread_control* control;
  size_t length;

  if (thread == 0 || name == 0) {
    return EINVAL;
  }
  length = strlen(name);
  if (length >= CRT_PTHREAD_NAME_MAX) {
    return ERANGE;
  }
  control = pthread_control_from_thread(thread);
  if (control != 0) {
    memcpy(control->context.name, name, length + 1);
    return 0;
  }
  if (pthread_is_current_thread(thread)) {
    memcpy(__crt_thread_name(), name, length + 1);
    return 0;
  }
  return ESRCH;
}

int pthread_getname_np(pthread_t thread, char* buf, size_t size) {
  crt_pthread_control* control;
  const char* name = "";
  size_t length;

  if (thread == 0 || buf == 0 || size == 0) {
    return EINVAL;
  }
  control = pthread_control_from_thread(thread);
  if (control != 0) {
    name = control->context.name;
  } else if (pthread_is_current_thread(thread)) {
    name = __crt_thread_name();
  } else {
    return ESRCH;
  }
  length = strlen(name);
  if (length + 1 > size) {
    return ERANGE;
  }
  memcpy(buf, name, length + 1);
  return 0;
}

int pthread_cancel(pthread_t thread) {
  if (thread == 0) {
    return EINVAL;
  }
  return ENOTSUP;
}

int pthread_setcancelstate(int state, int* oldstate) {
  if (state != PTHREAD_CANCEL_ENABLE && state != PTHREAD_CANCEL_DISABLE) {
    return EINVAL;
  }
  if (oldstate != 0) {
    *oldstate = PTHREAD_CANCEL_DISABLE;
  }
  return state == PTHREAD_CANCEL_DISABLE ? 0 : ENOTSUP;
}

int pthread_setcanceltype(int type, int* oldtype) {
  if (type != PTHREAD_CANCEL_DEFERRED && type != PTHREAD_CANCEL_ASYNCHRONOUS) {
    return EINVAL;
  }
  if (oldtype != 0) {
    *oldtype = PTHREAD_CANCEL_DEFERRED;
  }
  return type == PTHREAD_CANCEL_DEFERRED ? 0 : ENOTSUP;
}

void pthread_testcancel(void) {
}

void pthread_exit(void* retval) {
  crt_pthread_control* control = pthread_get_current_control();
  int detached = 0;

  if (control != 0) {
    control->result = retval;
    detached = __atomic_load_n(&control->detached, __ATOMIC_ACQUIRE);
  }
  pthread_run_key_destructors();
  if (control != 0) {
    __crt_thread_clear_current(&control->context);
  }
  if (detached) {
    pthread_control_destroy_from_worker(control);
  }
#if defined(CRT_TARGET_OS_WINDOWS)
  ExitThread(0);
#elif defined(CRT_TARGET_OS_MACOS)
  {
    crt_macos_pthread_exit_fn exit_fn =
        (crt_macos_pthread_exit_fn)crt_macos_pthread_symbol("pthread_exit");
    if (exit_fn != 0) {
      exit_fn(retval);
    }
  }
  __crt_sys_thread_exit(0);
#else
  __crt_sys_thread_exit(0);
#endif
}
