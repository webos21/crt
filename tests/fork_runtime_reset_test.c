#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "fork_runtime_reset_test: %s\n", message);
  return 1;
}

#if !defined(CRT_TARGET_OS_WINDOWS)
static FILE* locked_stream;
static volatile int worker_locked;
static volatile int worker_release;

static void* stream_lock_worker(void* arg) {
  (void)arg;

  flockfile(locked_stream);
  worker_locked = 1;
  while (!worker_release) {
    sched_yield();
  }
  funlockfile(locked_stream);
  return 0;
}

static int test_child_stdio_lock_reset(void) {
  pthread_t thread;
  pid_t pid;
  int status = 0;

  locked_stream = tmpfile();
  if (locked_stream == 0) {
    return fail("tmpfile");
  }
  worker_locked = 0;
  worker_release = 0;
  if (pthread_create(&thread, 0, stream_lock_worker, 0) != 0) {
    fclose(locked_stream);
    return fail("pthread_create");
  }
  while (!worker_locked) {
    sched_yield();
  }
  pid = fork();
  if (pid < 0) {
    worker_release = 1;
    pthread_join(thread, 0);
    fclose(locked_stream);
    return fail("fork");
  }
  if (pid == 0) {
    if (ftrylockfile(locked_stream) != 0) {
      _exit(77);
    }
    funlockfile(locked_stream);
    _exit(0);
  }
  worker_release = 1;
  if (pthread_join(thread, 0) != 0) {
    fclose(locked_stream);
    return fail("pthread_join");
  }
  fclose(locked_stream);
  if (waitpid(pid, &status, 0) != pid ||
      !WIFEXITED(status) ||
      WEXITSTATUS(status) != 0) {
    return fail("child stdio lock reset");
  }
  return 0;
}
#endif

int main(void) {
#if defined(CRT_TARGET_OS_WINDOWS)
  errno = 0;
  if (fork() != -1 || errno != ENOTSUP) {
    return fail("windows fork policy");
  }
#else
  if (test_child_stdio_lock_reset() != 0) {
    return 1;
  }
#endif
  puts("fork_runtime_reset_test: ok");
  return 0;
}
