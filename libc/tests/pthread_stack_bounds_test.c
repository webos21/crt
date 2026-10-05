/* pthread_getattr_np() must describe the calling thread's real stack. WTF::StackBounds and
 * JavaScriptCore's VM::setLastStackTop() derive the stack from it and abort when the address of a
 * local is not inside the reported range; the Windows implementation reported nothing (base 0). */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>

static int failures;

#define CHECK(condition, message)                                                        \
  do {                                                                                   \
    if (!(condition)) {                                                                  \
      fprintf(stderr, "pthread_stack_bounds_test: %s (line %d)\n", message, __LINE__);   \
      ++failures;                                                                        \
    }                                                                                    \
  } while (0)

static void check_current_stack(const char* who) {
  pthread_attr_t attr;
  void* base = 0;
  size_t size = 0;
  volatile char local = 0;
  uintptr_t address = (uintptr_t)&local;
  char message[96];

  snprintf(message, sizeof(message), "%s: pthread_getattr_np succeeds", who);
  CHECK(pthread_getattr_np(pthread_self(), &attr) == 0, message);
  snprintf(message, sizeof(message), "%s: pthread_attr_getstack succeeds", who);
  CHECK(pthread_attr_getstack(&attr, &base, &size) == 0, message);
  snprintf(message, sizeof(message), "%s: a stack is reported", who);
  CHECK(base != 0 && size >= 16 * 1024, message);
  snprintf(message, sizeof(message), "%s: a local variable lies inside the reported stack", who);
  CHECK(address >= (uintptr_t)base && address < (uintptr_t)base + size, message);
  pthread_attr_destroy(&attr);
}

static void* worker(void* argument) {
  (void)argument;
  check_current_stack("worker thread");
  return 0;
}

#if defined(_WIN32) || defined(__MINGW32__) || defined(CRT_TARGET_OS_WINDOWS)
/* WTF::StackBounds sizes the main thread's stack from getrlimit(RLIMIT_STACK) and ignores the
 * bounds above: a fixed 8 MiB on a 1 MiB Windows stack let JavaScriptCore recurse past the real end
 * of the stack and crash instead of throwing a RangeError. The limit must not exceed what exists. */
static void check_stack_limit_matches_the_main_stack(void) {
  pthread_attr_t attr;
  void* base = 0;
  size_t size = 0;
  struct rlimit limit;

  CHECK(getrlimit(RLIMIT_STACK, &limit) == 0, "getrlimit(RLIMIT_STACK) succeeds");
  CHECK(pthread_getattr_np(pthread_self(), &attr) == 0 && pthread_attr_getstack(&attr, &base, &size) == 0,
        "the main thread's stack is known");
  CHECK(limit.rlim_cur != RLIM_INFINITY && limit.rlim_cur <= size,
        "RLIMIT_STACK does not promise more than the main thread's real stack");
  pthread_attr_destroy(&attr);
}
#endif

int main(void) {
  pthread_t thread;

  check_current_stack("initial thread");
#if defined(_WIN32) || defined(__MINGW32__) || defined(CRT_TARGET_OS_WINDOWS)
  check_stack_limit_matches_the_main_stack();
#endif
  CHECK(pthread_create(&thread, 0, worker, 0) == 0, "pthread_create");
  CHECK(pthread_join(thread, 0) == 0, "pthread_join");
  if (failures != 0) {
    return 1;
  }
  printf("pthread_stack_bounds_test: ok\n");
  return 0;
}
