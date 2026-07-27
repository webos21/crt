#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <private/crt_atomic.h>

long __crt_sys_thread_id(void);
void __crt_sys_thread_exit(int status) __attribute__((noreturn));

#define CRT_PTHREAD_KEYS_MAX 128
#define CRT_PTHREAD_STACK_SIZE (1024UL * 1024UL)
#define CRT_PTHREAD_ATTR_FLAG_DETACHED 0x00000001U

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
#elif defined(CRT_TARGET_OS_LINUX)
long __crt_sys_clone_thread(void* stack_top, int (*entry)(void*), void* arg, unsigned long flags);
long __crt_sys_wait4(long pid, int* status, int options, void* rusage);

#define CRT_SIGCHLD 17UL
#define CRT_CLONE_VM 0x00000100UL
#define CRT_CLONE_FS 0x00000200UL
#define CRT_CLONE_FILES 0x00000400UL
#define CRT_CLONE_SIGHAND 0x00000800UL
#define CRT_CLONE_THREAD_FLAGS \
  (CRT_CLONE_VM | CRT_CLONE_FS | CRT_CLONE_FILES | CRT_CLONE_SIGHAND | CRT_SIGCHLD)
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

typedef struct {
#if defined(CRT_TARGET_OS_WINDOWS)
  HANDLE handle;
  DWORD thread_id;
#elif defined(CRT_TARGET_OS_LINUX)
  long tid;
  void* stack;
  unsigned long stack_size;
#elif defined(CRT_TARGET_OS_MACOS)
  crt_macos_pthread_t native_thread;
#endif
  void* (*start_routine)(void*);
  void* arg;
  void* result;
  int detached;
} crt_pthread_control;

static int pthread_key_used[CRT_PTHREAD_KEYS_MAX];
static crt_spinlock pthread_key_lock = CRT_SPINLOCK_INIT;

static crt_atomic_int* mutex_state(pthread_mutex_t* mutex) {
  return (crt_atomic_int*)&mutex->__private[0];
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
  return key >= 0 && key < CRT_PTHREAD_KEYS_MAX && pthread_key_used[key] != 0;
}

#if defined(CRT_TARGET_OS_WINDOWS) || defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_MACOS)
static void pthread_control_destroy(crt_pthread_control* control) {
  if (control == 0) {
    return;
  }
#if defined(CRT_TARGET_OS_LINUX)
  if (control->stack != 0) {
    munmap(control->stack, control->stack_size);
  }
#endif
  free(control);
}

static void pthread_control_destroy_from_worker(crt_pthread_control* control) {
  if (control == 0) {
    return;
  }
  /*
   * Linux threads currently run on the stack stored in the control block.
   * Reclaiming that mapping requires a later reaper/futex tranche.
   */
  free(control);
}

static int pthread_start(void* arg) {
  crt_pthread_control* control = (crt_pthread_control*)arg;
  int detached;

  control->result = control->start_routine(control->arg);
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

int pthread_mutex_init(pthread_mutex_t* mutex, const void* attr) {
  (void)attr;

  if (mutex == 0) {
    return EINVAL;
  }
  mutex->__private[0] = 0;
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

int pthread_create(
    pthread_t* thread,
    const pthread_attr_t* attr,
    void* (*start_routine)(void*),
    void* arg) {
  crt_pthread_control* control;
  (void)attr;

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
  control->stack_size = attr != 0 ? attr->stack_size : CRT_PTHREAD_STACK_SIZE;
  control->stack = mmap(0, control->stack_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (control->stack == MAP_FAILED) {
    free(control);
    return EAGAIN;
  }
  control->tid = __crt_sys_clone_thread(
      (char*)control->stack + control->stack_size, pthread_start, control, CRT_CLONE_THREAD_FLAGS);
  if (control->tid < 0) {
    munmap(control->stack, control->stack_size);
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
  {
    long wait_result;
    int status = 0;
    do {
      wait_result = __crt_sys_wait4(control->tid, &status, 0, 0);
    } while (wait_result == -EINTR);
    if (wait_result < 0) {
      return -wait_result;
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
  (void)retval;
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
