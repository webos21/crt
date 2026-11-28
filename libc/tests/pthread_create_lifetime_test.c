/* A new thread must not run before pthread_create() has finished with its control block: a thread
 * that is detached may exit and have that block reaped and freed at once. The Linux backend used to
 * store the clone() result in the block after the thread was already running; when the thread was
 * faster, the store hit freed memory (the allocator's free-list link) and some later malloc()
 * crashed.
 *
 * This fixture owns pthread.c and wraps its clone() so that the creator is held right after the
 * system call returns, which is the interleaving that otherwise needs a loaded machine. */
#if defined(CRT_TARGET_OS_LINUX)
#define __crt_sys_clone_thread test_clone_thread
#include "../src/pthread.c"
#undef __crt_sys_clone_thread
#include <stdio.h>

long __crt_sys_clone_thread(
    void* stack_top,
    int (*entry)(void*),
    void* arg,
    unsigned long flags,
    int* parent_tid,
    int* child_tid,
    void* tls);

/* How long the creator is held. A thread that is free to run starts within this by a wide margin. */
#define HOLD_MILLISECONDS 300

static int hold_creator;
static int ran_while_held;
static int routine_started;
static int routine_checked;
static int routine_error;
static pthread_t published;

static void sleep_one_millisecond(void) {
  struct timespec delay = {0, 1000000L};

  (void)nanosleep(&delay, 0);
}

long test_clone_thread(
    void* stack_top,
    int (*entry)(void*),
    void* arg,
    unsigned long flags,
    int* parent_tid,
    int* child_tid,
    void* tls) {
  long result =
      __crt_sys_clone_thread(stack_top, entry, arg, flags, parent_tid, child_tid, tls);

  /* Not the reaper, which pthread_create() also starts through clone(). */
  if (result > 0 && entry == pthread_start && hold_creator) {
    int i;

    for (i = 0; i < HOLD_MILLISECONDS && !__atomic_load_n(&routine_started, __ATOMIC_ACQUIRE); ++i) {
      sleep_one_millisecond();
    }
    ran_while_held = __atomic_load_n(&routine_started, __ATOMIC_ACQUIRE);
  }
  return result;
}

static void* routine(void* arg) {
  __atomic_store_n(&routine_started, 1, __ATOMIC_RELEASE);
  /* What pthread_create() published must be visible to the new thread. */
  if (published != pthread_self()) {
    routine_error = 1;
  } else if (((crt_pthread_control*)(uintptr_t)published)->tid != gettid()) {
    routine_error = 2;
  }
  if (arg != 0 && pthread_detach(pthread_self()) != 0) {
    routine_error = 3;
  }
  __atomic_store_n(&routine_checked, 1, __ATOMIC_RELEASE);
  return 0;
}

static int run_case(const char* name, int create_detached, int detach_itself) {
  pthread_attr_t attr;
  int result;
  int i;

  published = 0;
  ran_while_held = 0;
  routine_error = 0;
  __atomic_store_n(&routine_started, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&routine_checked, 0, __ATOMIC_RELEASE);
  if (pthread_attr_init(&attr) != 0 ||
      (create_detached &&
       pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0)) {
    fprintf(stderr, "pthread_create_lifetime_test: %s: attributes\n", name);
    return 1;
  }
  hold_creator = 1;
  result = pthread_create(&published, &attr, routine, detach_itself ? (void*)1 : 0);
  hold_creator = 0;
  pthread_attr_destroy(&attr);
  if (result != 0) {
    fprintf(stderr, "pthread_create_lifetime_test: %s: pthread_create %d\n", name, result);
    return 1;
  }
  if (ran_while_held) {
    fprintf(stderr,
            "pthread_create_lifetime_test: %s: the thread ran before pthread_create() was done\n",
            name);
    return 1;
  }
  if (!create_detached && !detach_itself) {
    if (pthread_join(published, 0) != 0) {
      fprintf(stderr, "pthread_create_lifetime_test: %s: pthread_join\n", name);
      return 1;
    }
  } else {
    for (i = 0; i < 5000 && !__atomic_load_n(&routine_checked, __ATOMIC_ACQUIRE); ++i) {
      sleep_one_millisecond();
    }
  }
  if (!__atomic_load_n(&routine_checked, __ATOMIC_ACQUIRE) || routine_error != 0) {
    fprintf(stderr, "pthread_create_lifetime_test: %s: thread check %d\n", name, routine_error);
    return 1;
  }
  return 0;
}

int main(void) {
  if (run_case("joinable", 0, 0) != 0 || run_case("created detached", 1, 0) != 0 ||
      run_case("detaches itself", 0, 1) != 0) {
    return 1;
  }
  puts("pthread_create_lifetime_test: ok");
  return 0;
}
#else
#include <stdio.h>

int main(void) {
  /* macOS and Windows create threads through the host; pthread_detach_lifetime_test holds the
   * creator on macOS. */
  puts("pthread_create_lifetime_test: skipped (Linux clone() backend)");
  return 0;
}
#endif
