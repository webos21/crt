#ifndef CRT_PRIVATE_CRT_TLS_H
#define CRT_PRIVATE_CRT_TLS_H

#define CRT_TLS_KEYS_MAX 128
#define CRT_TLS_NAME_MAX 16

#if defined(CRT_TARGET_OS_WINDOWS)
/* A thread's signal state (libc/src/arch/windows/common/signal_backend.c). Windows has no kernel
 * signals, so the CRT keeps each thread's mask and pending set itself and delivers to a thread by
 * suspending it and running the handler on it. It lives in the thread's own context, so it lasts as
 * long as the thread's control block does and another thread can reach it from a pthread_t. */
typedef struct crt_signal_state {
  volatile unsigned long mask;
  volatile unsigned long pending;
  volatile long waiting;      /* 1 while the thread is blocked in sigsuspend() */
  volatile long alive;        /* 0 once the thread has ended */
  volatile long in_wait;      /* 1 while the thread is parked in an interruptible CRT wait */
  void* volatile wait_address; /* the address that wait is parked on (0: a handle wait) */
  void* wake_event;           /* auto-reset event: signalled when a signal becomes pending */
  void* thread_handle;        /* suspend/context rights, owned by this state */
  unsigned long thread_id;
} crt_signal_state;
#endif

typedef struct crt_thread_context {
  void* control;
#if defined(CRT_TARGET_OS_WINDOWS)
  crt_signal_state signal;
#endif
  int errno_value;
  int h_errno_value;
  void* key_values[CRT_TLS_KEYS_MAX];
  char name[CRT_TLS_NAME_MAX];
#if defined(CRT_TARGET_OS_LINUX)
  long tid;
  int* tid_word;
  int listed;
  struct crt_thread_context* next;
#endif
} crt_thread_context;

void __crt_thread_context_init(crt_thread_context* context, void* control, int* tid_word);
void __crt_thread_set_current(crt_thread_context* context);
crt_thread_context* __crt_thread_get_current(void);
void __crt_thread_clear_current(crt_thread_context* context);
void __crt_thread_after_fork_child(crt_thread_context* current);
#if defined(CRT_TARGET_OS_WINDOWS)
/* Address of the lazily-TlsAlloc()'d slot index (tls.c). A memory-copy
 * fork() copies this like any other writable static, but the copied
 * index number is only valid in the PARENT's Win32 TLS bitmap -- the
 * child never called TlsAlloc() itself, so this must be reset to
 * CRT_TLS_OUT_OF_INDEXES in the child's copy before anything calls
 * __crt_thread_get_current()/__crt_thread_after_fork_child() there, to
 * force a fresh, legitimately-allocated index. */
long* __crt_windows_tls_index_ptr(void);
#endif
void* __crt_thread_control(void);
#if defined(CRT_TARGET_OS_LINUX)
int __crt_thread_control_is_live(void* control);
#endif
int* __crt_thread_errno(void);
int* __crt_thread_h_errno(void);
void** __crt_thread_key_values(void);
char* __crt_thread_name(void);

#endif
