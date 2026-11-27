/* A forked child that does not exec must be able to create and join threads. On macOS this failed
 * with SIGSEGV while fork() was a bare system call: libSystem's post-fork reinitialisation never
 * ran (libc/src/arch/macos/common/fork.c). */
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHILD_OK 42
#define THREADS 4

static int fail(const char* message) {
  fprintf(stderr, "fork_thread_test: %s (errno=%d)\n", message, errno);
  return 1;
}

static void* worker(void* arg) {
  /* Touch the allocator from the new thread too. */
  void* memory = malloc(4096);

  if (memory == 0) {
    return 0;
  }
  free(memory);
  return arg;
}

/* Returns 0 when several threads can be created, joined, and detached. */
static int use_threads(void) {
  pthread_t threads[THREADS];
  pthread_attr_t attr;
  pthread_t detached;
  int i;

  for (i = 0; i < THREADS; ++i) {
    if (pthread_create(&threads[i], 0, worker, (void*)(uintptr_t)(i + 1)) != 0) {
      return 1;
    }
  }
  for (i = 0; i < THREADS; ++i) {
    void* result = 0;

    if (pthread_join(threads[i], &result) != 0 || result != (void*)(uintptr_t)(i + 1)) {
      return 2;
    }
  }
  if (pthread_attr_init(&attr) != 0 ||
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0 ||
      pthread_create(&detached, &attr, worker, 0) != 0) {
    return 3;
  }
  pthread_attr_destroy(&attr);
  return 0;
}

static int expect_exit(pid_t child, int expected) {
  int status = 0;

  if (waitpid(child, &status, 0) != child) {
    return fail("waitpid failed");
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != expected) {
    fprintf(stderr, "fork_thread_test: child status 0x%x, wanted exit %d\n", status, expected);
    return 1;
  }
  return 0;
}

/* Forks; the child uses threads, then repeats this `depth` more times in a grandchild. */
static int fork_and_use_threads(int depth) {
  pid_t child = fork();

  if (child < 0) {
    return fail("fork failed");
  }
  if (child == 0) {
    int result;

    if (gettid() != getpid()) {
      _exit(10);
    }
    result = use_threads();
    if (result != 0) {
      _exit(result);
    }
    if (depth > 0 && fork_and_use_threads(depth - 1) != 0) {
      _exit(11);
    }
    _exit(CHILD_OK);
  }
  return expect_exit(child, CHILD_OK);
}

static void* fork_from_thread(void* arg) {
  (void)arg;
  /* The grandchild forks again from the stack the child inherited from this thread. */
  return (void*)(uintptr_t)fork_and_use_threads(1);
}

/* Touches about `frames` * 8 KiB of stack in frames larger than a page; frames must be below 256. */
__attribute__((noinline)) static unsigned int use_stack(unsigned int frames) {
  volatile unsigned char frame[8192];
  unsigned int below;
  unsigned int i;

  for (i = 0; i < sizeof(frame); i += 512) {
    frame[i] = (unsigned char)frames;
  }
  below = frames > 1 ? use_stack(frames - 1) : 0;
  return below + frame[512];
}

/* The child of a secondary thread keeps that thread's stack: it must be able to go deeper than the
 * parent ever did. `arg` is the number of 8 KiB frames to use. */
static void* fork_and_grow_stack(void* arg) {
  unsigned int frames = (unsigned int)(uintptr_t)arg;
  unsigned int expected = frames * (frames + 1U) / 2U;
  pid_t child = fork();

  if (child < 0) {
    return (void*)(uintptr_t)fail("fork failed");
  }
  if (child == 0) {
    if (pthread_kill(pthread_self(), 0) != 0) {
      _exit(12);
    }
    _exit(use_stack(frames) == expected ? CHILD_OK : 13);
  }
  /* The parent's own thread is unharmed. */
  if (expect_exit(child, CHILD_OK) != 0 || use_stack(frames) != expected) {
    return (void*)1;
  }
  return 0;
}

/* A thread created inside the child of a secondary-thread fork() forks in turn. */
static void* fork_then_fork_from_new_thread(void* arg) {
  pid_t child = fork();

  (void)arg;
  if (child < 0) {
    return (void*)(uintptr_t)fail("fork failed");
  }
  if (child == 0) {
    pthread_t thread;
    void* result = 0;

    if (pthread_create(&thread, 0, fork_from_thread, 0) != 0 ||
        pthread_join(thread, &result) != 0 || result != 0) {
      _exit(14);
    }
    _exit(CHILD_OK);
  }
  return (void*)(uintptr_t)expect_exit(child, CHILD_OK);
}

#if defined(CRT_TARGET_OS_WINDOWS)
/* The Windows copy holds the heap lock; a _Fork() child, which runs no fork() child hooks, must not
 * inherit it held. */
static void* raw_fork_and_allocate(void* arg) {
  pid_t child = _Fork();

  (void)arg;
  if (child < 0) {
    return (void*)(uintptr_t)fail("_Fork failed");
  }
  if (child == 0) {
    void* memory = malloc(100000);

    free(memory);
    _exit(memory != 0 ? CHILD_OK : 15);
  }
  return (void*)(uintptr_t)expect_exit(child, CHILD_OK);
}
#endif

#if defined(CRT_TARGET_OS_WINDOWS)
static void fault_handler(int sig) {
  _exit(sig == SIGSEGV ? CHILD_OK : 16);
}

/* Windows checks exception frames against the stack bounds in the TEB: a fault on the stack taken
 * over from a secondary thread must still reach its handler. */
static void* fork_and_fault(void* arg) {
  pid_t child = fork();

  (void)arg;
  if (child < 0) {
    return (void*)(uintptr_t)fail("fork failed");
  }
  if (child == 0) {
    volatile int* volatile nowhere = 0;

    if (signal(SIGSEGV, fault_handler) == SIG_ERR) {
      _exit(17);
    }
    *nowhere = 1;
    _exit(18);
  }
  return (void*)(uintptr_t)expect_exit(child, CHILD_OK);
}
#endif

/* The forking thread is the child's only one: when it returns, the process ends with status 0. */
static void* fork_and_return(void* arg) {
  pid_t child = fork();

  if (child < 0) {
    return (void*)(uintptr_t)fail("fork failed");
  }
  if (child == 0) {
    return 0;
  }
  *(int*)arg = expect_exit(child, 0);
  return 0;
}

static int run_thread(void* (*routine)(void*), void* arg, size_t stack_size) {
  pthread_attr_t attr;
  pthread_t thread;
  void* result = 0;

  if (pthread_attr_init(&attr) != 0 ||
      (stack_size != 0 && pthread_attr_setstacksize(&attr, stack_size) != 0) ||
      pthread_create(&thread, &attr, routine, arg) != 0) {
    return 1;
  }
  pthread_attr_destroy(&attr);
  return pthread_join(thread, &result) != 0 || result != 0;
}

static int stop_workers;

static void* busy_worker(void* arg) {
  (void)arg;
  while (__atomic_load_n(&stop_workers, __ATOMIC_ACQUIRE) == 0) {
    pthread_t thread;

    if (pthread_create(&thread, 0, worker, 0) != 0 || pthread_join(thread, 0) != 0) {
      return (void*)1;
    }
  }
  return 0;
}

/* Keeps the allocator's block list changing: a fork() must not copy it half way through an update. */
static void* churn_worker(void* arg) {
  void* blocks[64] = {0};
  unsigned int seed = (unsigned int)(uintptr_t)arg;
  unsigned int i;

  while (__atomic_load_n(&stop_workers, __ATOMIC_ACQUIRE) == 0) {
    seed = seed * 1103515245U + 12345U;
    i = (seed >> 16) % 64U;
    free(blocks[i]);
    blocks[i] = malloc(1U + (seed >> 8) % 6000U);
    if (blocks[i] == 0) {
      return (void*)1;
    }
  }
  for (i = 0; i < 64U; ++i) {
    free(blocks[i]);
  }
  return 0;
}

int main(void) {
  pthread_t thread;
  pthread_t busy;
  pthread_t churn;
  void* result = 0;
  int i;

  /* Before the parent has ever created a thread, including children and grandchildren. */
  if (fork_and_use_threads(2) != 0) {
    return fail("single-threaded parent");
  }
  /* After the parent has used threads, and the parent keeps working afterwards. */
  if (use_threads() != 0) {
    return fail("parent threads");
  }
  if (fork_and_use_threads(1) != 0 || use_threads() != 0) {
    return fail("parent that used threads");
  }
  /* fork() called by a secondary thread: that thread is the child's only one. */
  if (pthread_create(&thread, 0, fork_from_thread, 0) != 0 ||
      pthread_join(thread, &result) != 0 || result != 0) {
    return fail("fork from a secondary thread");
  }
  if (run_thread(fork_then_fork_from_new_thread, 0, 0) != 0) {
    return fail("fork from a new thread of a forked child");
  }
#if defined(CRT_TARGET_OS_WINDOWS)
  if (run_thread(raw_fork_and_allocate, 0, 0) != 0) {
    return fail("_Fork from a secondary thread");
  }
  if (run_thread(fork_and_fault, 0, 0) != 0) {
    return fail("fault handler after fork from a secondary thread");
  }
#endif
  /* About 320 KiB of a default thread stack, and 2000 KiB of a 4 MiB one. */
  if (run_thread(fork_and_grow_stack, (void*)(uintptr_t)40, 0) != 0) {
    return fail("stack growth after fork from a secondary thread");
  }
  if (run_thread(fork_and_grow_stack, (void*)(uintptr_t)250, 4U * 1024U * 1024U) != 0) {
    return fail("stack growth after fork from a thread with a 4 MiB stack");
  }
  {
    int returned = 1;

    if (run_thread(fork_and_return, &returned, 0) != 0 || returned != 0) {
      return fail("forked thread returning from its start routine");
    }
  }
  /* fork() while other threads create and join threads and allocate and free memory. */
  if (pthread_create(&busy, 0, busy_worker, 0) != 0 ||
      pthread_create(&churn, 0, churn_worker, (void*)(uintptr_t)1) != 0) {
    return fail("busy thread create");
  }
  for (i = 0; i < 50; ++i) {
    if (fork_and_use_threads(0) != 0) {
      return fail("fork beside a busy thread");
    }
  }
  __atomic_store_n(&stop_workers, 1, __ATOMIC_RELEASE);
  if (pthread_join(busy, &result) != 0 || result != 0 ||
      pthread_join(churn, &result) != 0 || result != 0) {
    return fail("busy thread");
  }
  printf("fork_thread_test: ok\n");
  return 0;
}
