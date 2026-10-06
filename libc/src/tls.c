#include <stddef.h>
#include <string.h>

#include <private/crt_atomic.h>
#include <private/crt_tls.h>

#if defined(CRT_TARGET_OS_LINUX)
long __crt_sys_thread_id(void);
long __crt_sys_write(int fd, const void* buffer, unsigned long size);

static crt_thread_context main_context;

/* The registry of threads made by pthread_create(), keyed by kernel tid.
 *
 * Readers take no lock. errno, pthread_self(), pthread_getspecific() and pthread_kill() all start
 * here, and a signal handler can run at any point of any thread: a reader that waited for a lock
 * which the interrupted code of the same thread (or of a thread parked in a handler, as WTF's
 * suspend/resume protocol parks it) holds would never get it. The table is open-addressed and
 * never reallocated, a slot's key stays after its thread has gone (the context pointer is cleared:
 * a tombstone, so probe chains stay intact), and a slot is only ever filled key first and context
 * pointer second, both with release stores. Writers (thread start/end, fork) still serialize on
 * a spin lock; nothing a signal handler or a resumer does takes that lock. */
#define CRT_THREAD_TABLE_BITS 14
#define CRT_THREAD_TABLE_SIZE (1u << CRT_THREAD_TABLE_BITS)

typedef struct {
  long tid;                       /* 0: never used; else the tid this slot was last assigned to */
  void* control;                  /* pthread control block of the live thread (valid with context) */
  crt_thread_context* context;    /* 0: tombstone */
} crt_thread_slot;

static crt_spinlock thread_lock = CRT_SPINLOCK_INIT;
static crt_thread_slot thread_table[CRT_THREAD_TABLE_SIZE];

static unsigned int thread_slot_hash(long tid) {
  return (unsigned int)(((unsigned long)tid * 0x9E3779B97F4A7C15ul) >> (64 - CRT_THREAD_TABLE_BITS));
}

static crt_thread_context* linux_find_context(long tid) {
  unsigned int index = thread_slot_hash(tid);
  unsigned int probes;

  for (probes = 0; probes < CRT_THREAD_TABLE_SIZE; ++probes) {
    crt_thread_slot* slot = &thread_table[(index + probes) & (CRT_THREAD_TABLE_SIZE - 1)];
    long key = __atomic_load_n(&slot->tid, __ATOMIC_ACQUIRE);

    if (key == 0) {
      return 0;
    }
    if (key == tid) {
      crt_thread_context* context = __atomic_load_n(&slot->context, __ATOMIC_ACQUIRE);
      if (context != 0) {
        return context;
      }
    }
  }
  return 0;
}

/* Whether `control` is the control block of a live thread created by pthread_create(): such a
 * thread registers its context here (crt_thread_context.control) and unregisters it when it
 * ends. A pthread_t that is not one -- the initial thread's, which is its kernel tid -- must
 * not be dereferenced as a pointer. Lock-free, and it never dereferences the context (which
 * a dying thread may be freeing): the control pointer is kept in the slot. */
int __crt_thread_control_is_live(void* control) {
  unsigned int index;

  for (index = 0; index < CRT_THREAD_TABLE_SIZE; ++index) {
    crt_thread_slot* slot = &thread_table[index];
    if (__atomic_load_n(&slot->context, __ATOMIC_ACQUIRE) != 0 &&
        __atomic_load_n(&slot->control, __ATOMIC_ACQUIRE) == control) {
      return 1;
    }
  }
  return 0;
}

static void linux_register_context(crt_thread_context* context) {
  unsigned int index;
  unsigned int probes;
  crt_thread_slot* free_slot = 0;

  if (context == 0) {
    return;
  }
  if (context->tid == 0) {
    context->tid = __crt_sys_thread_id();
  }
  crt_spin_lock(&thread_lock);
  if (!context->listed) {
    index = thread_slot_hash(context->tid);
    for (probes = 0; probes < CRT_THREAD_TABLE_SIZE; ++probes) {
      crt_thread_slot* slot = &thread_table[(index + probes) & (CRT_THREAD_TABLE_SIZE - 1)];
      if (slot->tid == context->tid) { /* this tid's own tombstone, or a live entry to replace */
        free_slot = slot;
        break;
      }
      if (free_slot == 0 && slot->context == 0) {
        free_slot = slot;
      }
      if (slot->tid == 0) {
        break;
      }
    }
    if (free_slot == 0) {
      static const char message[] = "crt: thread registry full (16384 threads)\n";
      crt_spin_unlock(&thread_lock);
      __crt_sys_write(2, message, sizeof(message) - 1);
      __builtin_trap();
    }
    /* A tombstone being reused for another tid: clear the context first so a reader of the old
     * key never pairs it with the new context, then publish key, control and context in order. */
    __atomic_store_n(&free_slot->context, (crt_thread_context*)0, __ATOMIC_RELEASE);
    __atomic_store_n(&free_slot->control, context->control, __ATOMIC_RELEASE);
    __atomic_store_n(&free_slot->tid, context->tid, __ATOMIC_RELEASE);
    __atomic_store_n(&free_slot->context, context, __ATOMIC_RELEASE);
    context->listed = 1;
  }
  crt_spin_unlock(&thread_lock);
}

static void linux_unregister_context(crt_thread_context* context) {
  unsigned int index;

  if (context == 0) {
    return;
  }
  crt_spin_lock(&thread_lock);
  for (index = 0; index < CRT_THREAD_TABLE_SIZE; ++index) {
    if (thread_table[index].context == context) {
      __atomic_store_n(&thread_table[index].context, (crt_thread_context*)0, __ATOMIC_RELEASE);
    }
  }
  context->listed = 0;
  crt_spin_unlock(&thread_lock);
}
#elif defined(CRT_TARGET_OS_WINDOWS)
void __crt_windows_signal_after_fork_child(void);

typedef unsigned long DWORD;
typedef int BOOL;

#define CRT_WINAPI
#define CRT_TLS_OUT_OF_INDEXES ((DWORD)0xffffffffUL)
#define CRT_MEM_COMMIT 0x00001000
#define CRT_MEM_RESERVE 0x00002000
#define CRT_PAGE_READWRITE 0x04

__declspec(dllimport) DWORD CRT_WINAPI TlsAlloc(void);
__declspec(dllimport) BOOL CRT_WINAPI TlsFree(DWORD dwTlsIndex);
__declspec(dllimport) void* CRT_WINAPI TlsGetValue(DWORD dwTlsIndex);
__declspec(dllimport) BOOL CRT_WINAPI TlsSetValue(DWORD dwTlsIndex, void* lpTlsValue);
__declspec(dllimport) void* CRT_WINAPI VirtualAlloc(
    void* lpAddress,
    unsigned long long dwSize,
    DWORD flAllocationType,
    DWORD flProtect);

static volatile long thread_tls_index = (long)CRT_TLS_OUT_OF_INDEXES;
static crt_thread_context fallback_context;

long* __crt_windows_tls_index_ptr(void) {
  return (long*)&thread_tls_index;
}

static DWORD windows_tls_index(void) {
  DWORD index = (DWORD)thread_tls_index;

  if (index == CRT_TLS_OUT_OF_INDEXES) {
    long expected;
    DWORD new_index = TlsAlloc();
    if (new_index == CRT_TLS_OUT_OF_INDEXES) {
      return CRT_TLS_OUT_OF_INDEXES;
    }
    expected = (long)CRT_TLS_OUT_OF_INDEXES;
    if (!__atomic_compare_exchange_n(&thread_tls_index, &expected, (long)new_index, 0,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
      TlsFree(new_index);
      index = (DWORD)thread_tls_index;
    } else {
      index = new_index;
    }
  }
  return index;
}
#else
static __thread crt_thread_context* current_context;
static crt_thread_context main_context;
#endif

void __crt_thread_context_init(crt_thread_context* context, void* control, int* tid_word) {
  if (context == 0) {
    return;
  }
  memset(context, 0, sizeof(*context));
  context->control = control;
#if defined(CRT_TARGET_OS_LINUX)
  context->tid_word = tid_word;
#else
  (void)tid_word;
#endif
}

void __crt_thread_set_current(crt_thread_context* context) {
#if defined(CRT_TARGET_OS_LINUX)
  linux_register_context(context);
#elif defined(CRT_TARGET_OS_WINDOWS)
  DWORD index = windows_tls_index();
  if (index != CRT_TLS_OUT_OF_INDEXES) {
    TlsSetValue(index, context);
  }
#else
  current_context = context;
#endif
}

crt_thread_context* __crt_thread_get_current(void) {
#if defined(CRT_TARGET_OS_LINUX)
  crt_thread_context* context = linux_find_context(__crt_sys_thread_id());
  return context != 0 ? context : &main_context;
#elif defined(CRT_TARGET_OS_WINDOWS)
  DWORD index = windows_tls_index();
  crt_thread_context* context;

  if (index == CRT_TLS_OUT_OF_INDEXES) {
    return &fallback_context;
  }
  context = (crt_thread_context*)TlsGetValue(index);
  if (context == 0) {
    context = (crt_thread_context*)VirtualAlloc(
        0, sizeof(crt_thread_context), CRT_MEM_RESERVE | CRT_MEM_COMMIT, CRT_PAGE_READWRITE);
    if (context == 0) {
      return &fallback_context;
    }
    __crt_thread_context_init(context, 0, 0);
    if (!TlsSetValue(index, context)) {
      return &fallback_context;
    }
  }
  return context;
#else
  return current_context != 0 ? current_context : &main_context;
#endif
}

void __crt_thread_clear_current(crt_thread_context* context) {
#if defined(CRT_TARGET_OS_LINUX)
  linux_unregister_context(context);
#elif defined(CRT_TARGET_OS_WINDOWS)
  DWORD index = windows_tls_index();
  (void)context;
  if (index != CRT_TLS_OUT_OF_INDEXES) {
    TlsSetValue(index, 0);
  }
#else
  if (current_context == context) {
    current_context = 0;
  }
#endif
}

void __crt_thread_after_fork_child(crt_thread_context* current) {
#if defined(CRT_TARGET_OS_LINUX)
  if (current == 0) {
    current = &main_context;
  }
  thread_lock.state.value = 0;
  memset(thread_table, 0, sizeof(thread_table));
  current->tid = __crt_sys_thread_id();
  current->tid_word = 0;
  current->listed = 0;
  current->next = 0;
  linux_register_context(current);
#elif defined(CRT_TARGET_OS_WINDOWS)
  if (current == 0) {
    current = &fallback_context;
  }
  __crt_thread_set_current(current);
  __crt_windows_signal_after_fork_child();
#else
  if (current == 0) {
    current = &main_context;
  }
  current_context = current;
#endif
}

void* __crt_thread_control(void) {
  return __crt_thread_get_current()->control;
}

int* __crt_thread_errno(void) {
  return &__crt_thread_get_current()->errno_value;
}

int* __crt_thread_h_errno(void) {
  return &__crt_thread_get_current()->h_errno_value;
}

void** __crt_thread_key_values(void) {
  return __crt_thread_get_current()->key_values;
}

char* __crt_thread_name(void) {
  return __crt_thread_get_current()->name;
}
