/* Who frees a thread's control block, when pthread_detach() meets the thread's exit.
 *
 * pthread_detach() used to set the detached flag and then keep reading the block, which a thread
 * that saw the flag could already have freed. A thread that had read the flag just before it was set
 * freed nothing, and nobody else did. pthread_exit() read the flag before the key destructors ran,
 * and on macOS pthread_create() released the native thread again if the new thread had already
 * detached itself.
 *
 * This fixture owns pthread.c. Every control block gets its own page, which free() makes
 * inaccessible instead of releasing, so a later access faults. The interleavings are forced by
 * wrapping the compare-and-swap, the thread's entry and the native thread creation call.
 *
 * With an argument, only the case of that name runs. */
#if defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_MACOS)
#include <stdlib.h>
#include <sys/mman.h>

static void* test_calloc(size_t count, size_t size);
static void test_free(void* pointer);
static int test_after_cas(const volatile void* object, int exchanged);

#define calloc test_calloc
#define free test_free
#define __atomic_compare_exchange_n(object, expected, desired, weak, success, failure) \
  test_after_cas(                                                                     \
      (object),                                                                       \
      __atomic_compare_exchange_n((object), (expected), (desired), (weak), (success), (failure)))
#if defined(CRT_TARGET_OS_LINUX)
#define __crt_sys_clone_thread test_clone_thread
#else
#define __crt_macho_find_symbol_in_loaded_image test_find_symbol
#endif
#include "../src/pthread.c"
#undef calloc
#undef free
#undef __atomic_compare_exchange_n
#if defined(CRT_TARGET_OS_LINUX)
#undef __crt_sys_clone_thread
long __crt_sys_clone_thread(
    void* stack_top,
    int (*entry)(void*),
    void* arg,
    unsigned long flags,
    int* parent_tid,
    int* child_tid,
    void* tls);
#else
#undef __crt_macho_find_symbol_in_loaded_image
void* __crt_macho_find_symbol_in_loaded_image(const char* image_name, const char* symbol);
#endif
#include <stdio.h>

/* How long a thread or its creator is held. Whatever is free to run does so well within this. */
#define HOLD_MILLISECONDS 300
#define DEADLINE_MILLISECONDS 5000

#define TEST_BLOCKS 32
#define TEST_BLOCK_BYTES 65536

static void* blocks[TEST_BLOCKS];
static int block_freed[TEST_BLOCKS];
static int block_count;

static void sleep_one_millisecond(void) {
  struct timespec delay = {0, 1000000L};

  (void)nanosleep(&delay, 0);
}

static int wait_for_flag(const int* flag, int milliseconds) {
  int i;

  for (i = 0; i < milliseconds && !__atomic_load_n(flag, __ATOMIC_ACQUIRE); ++i) {
    sleep_one_millisecond();
  }
  return __atomic_load_n(flag, __ATOMIC_ACQUIRE);
}

static int block_index(const volatile void* pointer) {
  int i;

  for (i = 0; i < __atomic_load_n(&block_count, __ATOMIC_ACQUIRE); ++i) {
    if ((const volatile char*)pointer >= (const volatile char*)blocks[i] &&
        (const volatile char*)pointer < (const volatile char*)blocks[i] + TEST_BLOCK_BYTES) {
      return i;
    }
  }
  return -1;
}

/* pthread.c allocates nothing but control blocks. Only the main thread creates threads here. */
static void* test_calloc(size_t count, size_t size) {
  void* page;

  if (block_count >= TEST_BLOCKS || count * size > TEST_BLOCK_BYTES) {
    return 0;
  }
  page = mmap(0, TEST_BLOCK_BYTES, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (page == MAP_FAILED) {
    return 0;
  }
  blocks[block_count] = page;
  __atomic_store_n(&block_count, block_count + 1, __ATOMIC_RELEASE);
  return page;
}

static void test_free(void* pointer) {
  int index = block_index(pointer);

  if (index < 0) {
    free(pointer);
    return;
  }
  if (mprotect(pointer, TEST_BLOCK_BYTES, PROT_NONE) != 0) {
    fprintf(stderr, "pthread_detach_lifetime_test: mprotect failed\n");
    _exit(1);
  }
  __atomic_store_n(&block_freed[index], 1, __ATOMIC_RELEASE);
}

/* --- Interleaving: the thread runs to the end right after pthread_detach()'s compare-and-swap. */
static pid_t cas_hook_thread; /* the detaching thread; 0 when the hook is off */
static int cas_hook_block = -1;
static int may_exit;

static int test_after_cas(const volatile void* object, int exchanged) {
  if (exchanged && cas_hook_thread != 0 && cas_hook_thread == gettid() &&
      block_index(object) == cas_hook_block) {
    cas_hook_thread = 0;
    __atomic_store_n(&may_exit, 1, __ATOMIC_RELEASE);
    (void)wait_for_flag(&block_freed[cas_hook_block], DEADLINE_MILLISECONDS);
  }
  return exchanged;
}

/* --- Interleaving: the thread has decided who frees its block, but has not exited yet. */
static int hold_exit;
static int at_exit_point;
static int detach_returned;

static void hold_before_exit(void) {
  if (hold_exit) {
    __atomic_store_n(&at_exit_point, 1, __ATOMIC_RELEASE);
    (void)wait_for_flag(&detach_returned, HOLD_MILLISECONDS);
  }
}

/* --- Interleaving: the creator is held right after the system has started the thread. */
static int hold_creator;
static int routine_started;
static int self_detached;
static int ran_while_held;

static void hold_creating_thread(void) {
  if (hold_creator) {
    (void)wait_for_flag(&self_detached, HOLD_MILLISECONDS);
    ran_while_held = __atomic_load_n(&routine_started, __ATOMIC_ACQUIRE);
  }
}

#if defined(CRT_TARGET_OS_LINUX)
static int test_thread_entry(void* arg) {
  int result = pthread_start(arg);

  hold_before_exit();
  return result;
}

long test_clone_thread(
    void* stack_top,
    int (*entry)(void*),
    void* arg,
    unsigned long flags,
    int* parent_tid,
    int* child_tid,
    void* tls) {
  int is_thread = entry == pthread_start; /* not the reaper */
  long result = __crt_sys_clone_thread(
      stack_top, is_thread ? test_thread_entry : entry, arg, flags, parent_tid, child_tid, tls);

  if (result > 0 && is_thread) {
    hold_creating_thread();
  }
  return result;
}
#else
static crt_macos_pthread_create_fn real_create;
static void* (*real_entry)(void*);

static void* test_thread_entry(void* arg) {
  void* result = real_entry(arg);

  hold_before_exit();
  return result;
}

static int test_native_create(
    crt_macos_pthread_t* thread, const void* attr, void* (*entry)(void*), void* arg) {
  int result;

  real_entry = entry;
  result = real_create(thread, attr, test_thread_entry, arg);
  if (result == 0) {
    hold_creating_thread();
  }
  return result;
}

void* test_find_symbol(const char* image_name, const char* symbol) {
  void* found = __crt_macho_find_symbol_in_loaded_image(image_name, symbol);

  if (found != 0 && strcmp(symbol, "pthread_create") == 0) {
    real_create = (crt_macos_pthread_create_fn)found;
    return (void*)test_native_create;
  }
  return found;
}
#endif

/* --- The cases. */
static int failed(const char* name, const char* what, int value) {
  fprintf(stderr, "pthread_detach_lifetime_test: %s: %s (%d)\n", name, what, value);
  return 1;
}

static void reset_hooks(void) {
  cas_hook_thread = 0;
  cas_hook_block = -1;
  hold_exit = 0;
  hold_creator = 0;
  ran_while_held = 0;
  __atomic_store_n(&may_exit, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&at_exit_point, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&detach_returned, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&routine_started, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&self_detached, 0, __ATOMIC_RELEASE);
}

static int expect_freed(const char* name, int index) {
  if (!wait_for_flag(&block_freed[index], DEADLINE_MILLISECONDS)) {
    return failed(name, "the control block was never freed", index);
  }
  return 0;
}

static void* wait_then_return(void* arg) {
  (void)arg;
  (void)wait_for_flag(&may_exit, DEADLINE_MILLISECONDS);
  return 0;
}

/* The thread sees the detach, exits and is freed before pthread_detach() returns. */
static int case_exit_inside_detach(const char* name) {
  pthread_t thread;
  int index = block_count;
  int result = pthread_create(&thread, 0, wait_then_return, 0);

  if (result != 0) {
    return failed(name, "pthread_create", result);
  }
  cas_hook_block = index;
  cas_hook_thread = gettid();
  result = pthread_detach(thread);
  if (cas_hook_thread != 0) {
    return failed(name, "pthread_detach made no state change", result);
  }
  if (result != 0) {
    return failed(name, "pthread_detach", result);
  }
  return expect_freed(name, index);
}

static void* return_at_once(void* arg) {
  (void)arg;
  return 0;
}

static void* wait_then_call_exit(void* arg) {
  (void)arg;
  (void)wait_for_flag(&may_exit, DEADLINE_MILLISECONDS);
  pthread_exit(0);
}

/* The thread has already chosen to leave its block alone when pthread_detach() runs. */
static int case_detach_during_exit(const char* name) {
  pthread_t thread;
  int index = block_count;
  int result;

  hold_exit = 1;
  result = pthread_create(&thread, 0, return_at_once, 0);
  if (result != 0) {
    return failed(name, "pthread_create", result);
  }
  if (!wait_for_flag(&at_exit_point, DEADLINE_MILLISECONDS)) {
    return failed(name, "the thread did not reach its exit", 0);
  }
  result = pthread_detach(thread);
  __atomic_store_n(&detach_returned, 1, __ATOMIC_RELEASE);
  if (result != 0) {
    return failed(name, "pthread_detach", result);
  }
  return expect_freed(name, index);
}

/* The thread is long gone when pthread_detach() runs. */
static int case_detach_after_exit(const char* name) {
  pthread_t thread;
  int index = block_count;
  int result = pthread_create(&thread, 0, return_at_once, 0);
  int i;

  if (result != 0) {
    return failed(name, "pthread_create", result);
  }
  for (i = 0; i < 100; ++i) {
    sleep_one_millisecond();
  }
  result = pthread_detach(thread);
  if (result != 0) {
    return failed(name, "pthread_detach", result);
  }
  return expect_freed(name, index);
}

static pthread_key_t detach_key;
static int destructor_result;

static void detach_from_destructor(void* value) {
  (void)value;
  destructor_result = pthread_detach(pthread_self());
}

static void* detach_in_destructor_then_exit(void* arg) {
  (void)pthread_setspecific(detach_key, arg);
  pthread_exit(0);
}

/* A key destructor run by pthread_exit() detaches the thread. */
static int case_detach_in_destructor(const char* name) {
  pthread_t thread;
  int index = block_count;
  int result = pthread_key_create(&detach_key, detach_from_destructor);

  if (result != 0) {
    return failed(name, "pthread_key_create", result);
  }
  destructor_result = -1;
  result = pthread_create(&thread, 0, detach_in_destructor_then_exit, &detach_key);
  if (result != 0) {
    return failed(name, "pthread_create", result);
  }
  if (expect_freed(name, index) != 0) {
    return 1;
  }
  if (destructor_result != 0) {
    return failed(name, "pthread_detach in the destructor", destructor_result);
  }
  return 0;
}

/* pthread_exit() of a thread that is detached while it runs. */
static int case_exit_call_detached(const char* name) {
  pthread_t thread;
  int index = block_count;
  int result = pthread_create(&thread, 0, wait_then_call_exit, 0);

  if (result != 0) {
    return failed(name, "pthread_create", result);
  }
  result = pthread_detach(thread);
  __atomic_store_n(&may_exit, 1, __ATOMIC_RELEASE);
  if (result != 0) {
    return failed(name, "pthread_detach", result);
  }
  return expect_freed(name, index);
}

static void* detach_self(void* arg) {
  int result;

  (void)arg;
  __atomic_store_n(&routine_started, 1, __ATOMIC_RELEASE);
  result = pthread_detach(pthread_self());
  if (result == 0) {
    __atomic_store_n(&self_detached, 1, __ATOMIC_RELEASE);
  }
  return 0;
}

/* The new thread wants to detach itself while pthread_create() is still at work. */
static int case_self_detach_during_create(const char* name) {
  pthread_t thread;
  int index = block_count;
  int result;

  hold_creator = 1;
  result = pthread_create(&thread, 0, detach_self, 0);
  hold_creator = 0;
  if (result != 0) {
    return failed(name, "pthread_create", result);
  }
  if (ran_while_held) {
    return failed(name, "the thread ran before pthread_create() was done", 0);
  }
  if (!wait_for_flag(&self_detached, DEADLINE_MILLISECONDS)) {
    return failed(name, "pthread_detach(pthread_self()) did not succeed", 0);
  }
  return expect_freed(name, index);
}

static const struct {
  const char* name;
  int (*run)(const char* name);
} cases[] = {
    {"exit-inside-detach", case_exit_inside_detach},
    {"detach-during-exit", case_detach_during_exit},
    {"detach-after-exit", case_detach_after_exit},
    {"detach-in-destructor", case_detach_in_destructor},
    {"exit-call-detached", case_exit_call_detached},
    {"self-detach-during-create", case_self_detach_during_create},
};

int main(int argc, char** argv) {
  size_t i;
  int ran = 0;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    if (argc > 1 && strcmp(argv[1], cases[i].name) != 0) {
      continue;
    }
    reset_hooks();
    if (cases[i].run(cases[i].name) != 0) {
      return 1;
    }
    ++ran;
  }
  if (ran == 0) {
    fprintf(stderr, "pthread_detach_lifetime_test: no such case\n");
    return 1;
  }
  puts("pthread_detach_lifetime_test: ok");
  return 0;
}
#else
#include <stdio.h>

int main(void) {
  /* The same ownership rule applies to the Windows backend, which this fixture does not wrap. */
  puts("pthread_detach_lifetime_test: skipped (Linux and macOS)");
  return 0;
}
#endif
