#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

static int thread_value;

static int fail(const char* message) {
  fprintf(stderr, "pthread_attr_test: %s\n", message);
  return 1;
}

static void* worker(void* arg) {
  thread_value = *(int*)arg;
  return &thread_value;
}

static void* stack_worker(void* arg) {
  (void)arg;
  return (void*)pthread_self();
}

int main(void) {
  pthread_attr_t attr;
  pthread_t thread;
  void* result = 0;
  int create_result;
  size_t stack_size = 0;
  size_t guard_size = 1;
  void* stack_addr = (void*)1;
  void* user_stack;
  int detach_state = -1;
  int pshared = -1;
  int input = 77;

  if (pthread_attr_init(0) != EINVAL) {
    return fail("pthread_attr_init invalid");
  }
  if (pthread_attr_init(&attr) != 0) {
    return fail("pthread_attr_init");
  }
  if (pthread_attr_getdetachstate(&attr, &detach_state) != 0 ||
      detach_state != PTHREAD_CREATE_JOINABLE) {
    return fail("default detach state");
  }
  if (pthread_attr_getstacksize(&attr, &stack_size) != 0 ||
      stack_size < PTHREAD_STACK_MIN) {
    return fail("default stack size");
  }
  if (pthread_attr_getguardsize(&attr, &guard_size) != 0 || guard_size < 4096) {
    return fail("default guard size");
  }
  if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0 ||
      pthread_attr_getdetachstate(&attr, &detach_state) != 0 ||
      detach_state != PTHREAD_CREATE_DETACHED) {
    return fail("set detached");
  }
  if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0 ||
      pthread_attr_getdetachstate(&attr, &detach_state) != 0 ||
      detach_state != PTHREAD_CREATE_JOINABLE) {
    return fail("set joinable");
  }
  if (pthread_attr_setdetachstate(&attr, 99) != EINVAL) {
    return fail("invalid detach state");
  }
  if (pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN - 1) != EINVAL) {
    return fail("invalid stack size");
  }
  if (pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN * 2) != 0 ||
      pthread_attr_getstacksize(&attr, &stack_size) != 0 ||
      stack_size != PTHREAD_STACK_MIN * 2) {
    return fail("set stack size");
  }
  if (pthread_attr_setguardsize(&attr, 4096) != 0 ||
      pthread_attr_getguardsize(&attr, &guard_size) != 0 ||
      guard_size != 4096) {
    return fail("guard size");
  }
  if (pthread_attr_getstack(&attr, &stack_addr, &stack_size) != 0 ||
      stack_addr != 0 ||
      stack_size != PTHREAD_STACK_MIN * 2) {
    return fail("get stack");
  }
  if (pthread_create(&thread, &attr, worker, &input) != 0) {
    return fail("pthread_create with attr");
  }
  if (pthread_join(thread, &result) != 0 || result != &thread_value || thread_value != 77) {
    return fail("pthread_join with attr");
  }
  if (pthread_attr_destroy(&attr) != 0 || pthread_attr_destroy(0) != EINVAL) {
    return fail("pthread_attr_destroy");
  }

  user_stack = mmap(0, PTHREAD_STACK_MIN * 4, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (user_stack == MAP_FAILED) {
    return fail("mmap user stack");
  }
  if (pthread_attr_init(&attr) != 0 ||
      pthread_attr_setstack(&attr, user_stack, PTHREAD_STACK_MIN * 4) != 0 ||
      pthread_attr_getstack(&attr, &stack_addr, &stack_size) != 0 ||
      stack_addr != user_stack ||
      stack_size != PTHREAD_STACK_MIN * 4 ||
      pthread_attr_getguardsize(&attr, &guard_size) != 0 ||
      guard_size != 0) {
    munmap(user_stack, PTHREAD_STACK_MIN * 4);
    return fail("user stack attr");
  }
  create_result = pthread_create(&thread, &attr, stack_worker, 0);
#if defined(CRT_TARGET_OS_WINDOWS)
  if (create_result != ENOTSUP) {
    munmap(user_stack, PTHREAD_STACK_MIN * 4);
    return fail("windows user stack policy");
  }
#else
  if (create_result != 0) {
    munmap(user_stack, PTHREAD_STACK_MIN * 4);
    return fail("pthread_create user stack");
  }
  if (pthread_join(thread, &result) != 0 || result != (void*)thread) {
    munmap(user_stack, PTHREAD_STACK_MIN * 4);
    return fail("pthread_join user stack");
  }
#endif
  pthread_attr_destroy(&attr);
  munmap(user_stack, PTHREAD_STACK_MIN * 4);

  {
    pthread_mutexattr_t mutex_attr;
    pthread_rwlockattr_t rwlock_attr;
    pthread_condattr_t cond_attr;
    int clock_id = -1;
    int robust = -1;

    if (pthread_mutexattr_init(&mutex_attr) != 0 ||
        pthread_mutexattr_getpshared(&mutex_attr, &pshared) != 0 ||
        pshared != PTHREAD_PROCESS_PRIVATE ||
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED) != ENOTSUP ||
        pthread_mutexattr_setpshared(&mutex_attr, 99) != EINVAL ||
        pthread_mutexattr_getrobust(&mutex_attr, &robust) != 0 ||
        robust != PTHREAD_MUTEX_STALLED ||
        pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST) != ENOTSUP ||
        pthread_mutexattr_setrobust(&mutex_attr, 99) != EINVAL) {
      return fail("mutex pshared attr");
    }
    pthread_mutexattr_destroy(&mutex_attr);

    if (pthread_rwlockattr_init(&rwlock_attr) != 0 ||
        pthread_rwlockattr_getpshared(&rwlock_attr, &pshared) != 0 ||
        pshared != PTHREAD_PROCESS_PRIVATE ||
        pthread_rwlockattr_setpshared(&rwlock_attr, PTHREAD_PROCESS_SHARED) != ENOTSUP ||
        pthread_rwlockattr_setpshared(&rwlock_attr, 99) != EINVAL) {
      return fail("rwlock pshared attr");
    }
    pthread_rwlockattr_destroy(&rwlock_attr);

    if (pthread_condattr_init(&cond_attr) != 0 ||
        pthread_condattr_getclock(&cond_attr, &clock_id) != 0 ||
        clock_id != PTHREAD_COND_CLOCK_REALTIME ||
        pthread_condattr_setclock(&cond_attr, PTHREAD_COND_CLOCK_MONOTONIC) != 0 ||
        pthread_condattr_getclock(&cond_attr, &clock_id) != 0 ||
        clock_id != PTHREAD_COND_CLOCK_MONOTONIC ||
        pthread_condattr_setclock(&cond_attr, 99) != EINVAL) {
      return fail("cond clock attr");
    }
    pthread_condattr_destroy(&cond_attr);
  }

  printf("pthread_attr_test: ok\n");
  return 0;
}
