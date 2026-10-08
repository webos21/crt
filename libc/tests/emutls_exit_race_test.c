/* Emulated thread-local storage (-femulated-tls, Windows) while the process exits with other threads running.
 *
 * compiler-rt's emutls registers an atexit() handler that frees its TLS index and mutex. exit() runs atexit
 * handlers while other threads still run (only the final ExitProcess stops them), so a thread that touched a
 * thread_local in that window got TlsGetValue() == NULL with ERROR_INVALID_PARAMETER, and emutls aborted the
 * process with exit status 134 -- a silent failure in about 5% of JavaScriptCore runs, always right after
 * the script had finished. The project's own __emutls_get_address() in libc
 * tears nothing down.
 *
 * The test runs itself as a child many times: the child starts threads that spin on a thread_local, the main
 * thread calls exit(0) while they do, and every child must end with status 0. It also checks that
 * thread_locals are per thread, start from their template and that a control object another module has already
 * numbered works as the first use (compiler-rt's fast path skipped the initialisation for it). */
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(CRT_TARGET_OS_WINDOWS)
#include <spawn.h>

extern char** environ;

static __thread long counter = 7;
static __thread char buffer[100];

static volatile int stop;

static void* spinner(void* argument) {
  (void)argument;
  while (!stop) {
    ++counter;
    buffer[counter % 100] = (char)counter;
  }
  return 0;
}

static int run_child(void) {
  pthread_t threads[4];
  int i;

  for (i = 0; i < 4; ++i) pthread_create(&threads[i], 0, spinner, 0);
  for (i = 0; i < 200; ++i) {
    ++counter;
    buffer[counter % 100] = 1;
  }
  exit(0); /* the spinners are still running */
}

static void* checker(void* argument) {
  long* result = (long*)argument;

  *result = counter; /* a new thread starts from the template, not from the creator's value */
  counter = 1234;
  return 0;
}

typedef struct {
  size_t size;
  size_t align;
  uintptr_t index;
  void* templ;
} control_object;

extern void* __emutls_get_address(control_object* control);

static int check_semantics(void) {
  pthread_t thread;
  long seen = -1;
  static const long initial = 99;
  /* A control object numbered by another module before this module's emutls was ever used. */
  static control_object numbered = {sizeof(long), sizeof(long), 41, (void*)&initial};
  long* object;

  counter = 55;
  pthread_create(&thread, 0, checker, &seen);
  pthread_join(thread, 0);
  if (seen != 7) {
    fprintf(stderr, "emutls_exit_race_test: a new thread saw %ld instead of its template 7\n", seen);
    return 1;
  }
  if (counter != 55) {
    fprintf(stderr, "emutls_exit_race_test: the main thread's thread_local was overwritten by another thread\n");
    return 1;
  }
  object = (long*)__emutls_get_address(&numbered);
  if (object == 0 || *object != 99 || object != (long*)__emutls_get_address(&numbered)) {
    fprintf(stderr, "emutls_exit_race_test: a pre-numbered control object did not work\n");
    return 1;
  }
  return 0;
}

int main(int argc, char** argv) {
  int round;

  if (argc > 1 && strcmp(argv[1], "child") == 0) {
    return run_child();
  }
  if (check_semantics() != 0) {
    return 1;
  }
  for (round = 0; round < 100; ++round) {
    pid_t pid;
    int status = 0;
    char* const child_argv[] = {argv[0], (char*)"child", 0};

    if (posix_spawn(&pid, argv[0], 0, 0, child_argv, environ) != 0) {
      fprintf(stderr, "emutls_exit_race_test: cannot start the child\n");
      return 1;
    }
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      fprintf(stderr, "emutls_exit_race_test: child %d ended with status %d (round %d)\n", (int)pid,
              WIFEXITED(status) ? WEXITSTATUS(status) : -1, round);
      return 1;
    }
  }
  printf("emutls_exit_race_test: ok\n");
  return 0;
}

#else

int main(void) {
  printf("emutls_exit_race_test: ok (skipped: Windows' emulated TLS only)\n");
  return 0;
}

#endif
