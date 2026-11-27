/* A forked child that does not exec must be able to create and join threads. On macOS this failed
 * with SIGSEGV while fork() was a bare system call: libSystem's post-fork reinitialisation never
 * ran (libc/src/arch/macos/common/fork.c). */
#include <errno.h>
#include <pthread.h>
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
  return (void*)(uintptr_t)fork_and_use_threads(0);
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

int main(void) {
  pthread_t thread;
  pthread_t busy;
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
  /* fork() while another thread is creating and joining threads. */
  if (pthread_create(&busy, 0, busy_worker, 0) != 0) {
    return fail("busy thread create");
  }
  for (i = 0; i < 50; ++i) {
    if (fork_and_use_threads(0) != 0) {
      return fail("fork beside a busy thread");
    }
  }
  __atomic_store_n(&stop_workers, 1, __ATOMIC_RELEASE);
  if (pthread_join(busy, &result) != 0 || result != 0) {
    return fail("busy thread");
  }
  printf("fork_thread_test: ok\n");
  return 0;
}
