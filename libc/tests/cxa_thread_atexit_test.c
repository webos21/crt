/* __cxa_thread_atexit() (Windows: libc/src/arch/windows/common/cxa_thread_atexit.c): destructors of
 * a thread run when that thread exits, last registered first, including ones registered by a
 * destructor, and not for another thread's list. */

#include <pthread.h>
#include <stdio.h>
#include <string.h>

extern int __cxa_thread_atexit(void (*destructor)(void*), void* object, void* dso);
extern void* __dso_handle;

static char order[64];
static int order_length;
static pthread_mutex_t order_lock = PTHREAD_MUTEX_INITIALIZER;

static void note(char c) {
  pthread_mutex_lock(&order_lock);
  if (order_length < (int)sizeof(order) - 1) {
    order[order_length++] = c;
    order[order_length] = 0;
  }
  pthread_mutex_unlock(&order_lock);
}

static void nested(void* object) {
  (void)object;
  note('n');
}

static void destructor(void* object) {
  char c = (char)(size_t)object;

  note(c);
  if (c == 'b') {
    __cxa_thread_atexit(nested, 0, &__dso_handle);
  }
}

static void* worker(void* argument) {
  (void)argument;
  __cxa_thread_atexit(destructor, (void*)(size_t)'a', &__dso_handle);
  __cxa_thread_atexit(destructor, (void*)(size_t)'b', &__dso_handle);
  __cxa_thread_atexit(destructor, (void*)(size_t)'c', &__dso_handle);
  note('w');
  return 0;
}

int main(void) {
  pthread_t thread;

  if (pthread_create(&thread, 0, worker, 0) != 0 || pthread_join(thread, 0) != 0) {
    fprintf(stderr, "cxa_thread_atexit_test: thread failed\n");
    return 1;
  }
  /* w first (the thread body), then c, b (which registers n), n, a: n runs before a because it
   * was registered later than a. */
  if (strcmp(order, "wcbna") != 0) {
    fprintf(stderr, "cxa_thread_atexit_test: order \"%s\", expected \"wcbna\"\n", order);
    return 2;
  }
  printf("cxa_thread_atexit_test: ok\n");
  return 0;
}
