/* __cxa_thread_atexit() for Windows: destructors of `thread_local` objects.
 *
 * The Itanium C++ ABI has the compiler register each non-trivially destructible thread_local
 * object with __cxa_thread_atexit(destructor, object, dso_handle) on first use. On Linux that
 * entry point is libc++abi's (cxa_thread_atexit.cpp, built for UNIX only), which the Windows
 * libc++abi does not build, so a Windows image containing such an object (WebCore, Skia, WTF's
 * thread-specific data) had an undefined reference that a COFF link reports (an ELF link with
 * --gc-sections drops the unreferenced function silently).
 *
 * Same fallback, in C, on the pthread key CRT already runs at thread exit: one key, a per-thread
 * list, run in reverse registration order until empty (a destructor may register more). The
 * process's initial thread never "exits", so the first registration also arranges an atexit()
 * call that runs the list of the thread that calls exit(). `dso` is accepted and ignored: this
 * runtime does not unload an image while threads still hold objects of it.
 */

#include <pthread.h>
#include <stdlib.h>

typedef struct crt_thread_dtor {
  void (*destructor)(void*);
  void* object;
  struct crt_thread_dtor* next;
} crt_thread_dtor;

static pthread_key_t crt_thread_dtor_key;
static pthread_once_t crt_thread_dtor_once = PTHREAD_ONCE_INIT;
static int crt_thread_dtor_key_ok;
static volatile int crt_thread_dtor_exit_registered;

static void crt_thread_dtor_run(void* list) {
  crt_thread_dtor* head = (crt_thread_dtor*)list;

  /* Registrations made by a destructor land on a fresh list under the key; they were registered
   * later than everything still pending, so they run before it. */
  pthread_setspecific(crt_thread_dtor_key, NULL);
  while (head != NULL) {
    crt_thread_dtor* entry = head;
    crt_thread_dtor* added;

    head = entry->next;
    entry->destructor(entry->object);
    free(entry);
    added = (crt_thread_dtor*)pthread_getspecific(crt_thread_dtor_key);
    if (added != NULL) {
      crt_thread_dtor* tail = added;

      pthread_setspecific(crt_thread_dtor_key, NULL);
      while (tail->next != NULL) {
        tail = tail->next;
      }
      tail->next = head;
      head = added;
    }
  }
}

static void crt_thread_dtor_at_exit(void) {
  if (crt_thread_dtor_key_ok) {
    crt_thread_dtor_run(pthread_getspecific(crt_thread_dtor_key));
  }
}

static void crt_thread_dtor_init(void) {
  crt_thread_dtor_key_ok = pthread_key_create(&crt_thread_dtor_key, crt_thread_dtor_run) == 0;
}

int __cxa_thread_atexit(void (*destructor)(void*), void* object, void* dso) {
  crt_thread_dtor* entry;

  (void)dso;
  pthread_once(&crt_thread_dtor_once, crt_thread_dtor_init);
  if (!crt_thread_dtor_key_ok) {
    return -1;
  }
  entry = (crt_thread_dtor*)malloc(sizeof(*entry));
  if (entry == NULL) {
    return -1;
  }
  entry->destructor = destructor;
  entry->object = object;
  entry->next = (crt_thread_dtor*)pthread_getspecific(crt_thread_dtor_key);
  if (pthread_setspecific(crt_thread_dtor_key, entry) != 0) {
    free(entry);
    return -1;
  }
  if (!crt_thread_dtor_exit_registered) {
    crt_thread_dtor_exit_registered = 1;
    atexit(crt_thread_dtor_at_exit);
  }
  return 0;
}
