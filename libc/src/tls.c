#include <stddef.h>
#include <string.h>

#include <private/crt_atomic.h>
#include <private/crt_tls.h>

#if defined(CRT_TARGET_OS_LINUX)
long __crt_sys_thread_id(void);

static crt_spinlock thread_lock = CRT_SPINLOCK_INIT;
static crt_thread_context main_context;
static crt_thread_context* thread_head;

static crt_thread_context* linux_find_context(long tid) {
  crt_thread_context* context;

  crt_spin_lock(&thread_lock);
  context = thread_head;
  while (context != 0) {
    if (context->tid == tid ||
        (context->tid_word != 0 && __atomic_load_n(context->tid_word, __ATOMIC_ACQUIRE) == (int)tid)) {
      crt_spin_unlock(&thread_lock);
      return context;
    }
    context = context->next;
  }
  crt_spin_unlock(&thread_lock);
  return 0;
}

static void linux_register_context(crt_thread_context* context) {
  if (context == 0) {
    return;
  }
  if (context->tid == 0) {
    context->tid = __crt_sys_thread_id();
  }
  crt_spin_lock(&thread_lock);
  if (!context->listed) {
    context->next = thread_head;
    thread_head = context;
    context->listed = 1;
  }
  crt_spin_unlock(&thread_lock);
}

static void linux_unregister_context(crt_thread_context* context) {
  crt_thread_context** link;

  if (context == 0) {
    return;
  }
  crt_spin_lock(&thread_lock);
  link = &thread_head;
  while (*link != 0) {
    if (*link == context) {
      *link = context->next;
      context->next = 0;
      context->listed = 0;
      break;
    }
    link = &(*link)->next;
  }
  crt_spin_unlock(&thread_lock);
}
#elif defined(CRT_TARGET_OS_WINDOWS)
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
  thread_head = 0;
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

void** __crt_thread_key_values(void) {
  return __crt_thread_get_current()->key_values;
}

char* __crt_thread_name(void) {
  return __crt_thread_get_current()->name;
}
