#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "fork_test: %s\n", message);
  return 1;
}

static int atfork_state;

static void atfork_prepare_one(void) {
  atfork_state = atfork_state * 10 + 1;
}

static void atfork_parent_one(void) {
  atfork_state = atfork_state * 10 + 1;
}

static void atfork_child_one(void) {
  atfork_state = atfork_state * 10 + 1;
}

static void atfork_prepare_two(void) {
  atfork_state = atfork_state * 10 + 2;
}

static void atfork_parent_two(void) {
  atfork_state = atfork_state * 10 + 2;
}

static void atfork_child_two(void) {
  atfork_state = atfork_state * 10 + 2;
}

static int wait_for_exit(pid_t pid, int expected) {
  int status = 0;

  if (waitpid(pid, &status, 0) != pid) {
    return fail("waitpid");
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != expected) {
    return fail("wait status");
  }
  return 0;
}

static int test_fork_basic(void) {
  pid_t pid = fork();

  if (pid < 0) {
#if defined(CRT_TARGET_OS_WINDOWS)
    return errno == ENOTSUP ? 0 : fail("windows fork errno");
#else
    return fail("fork");
#endif
  }
  if (pid == 0) {
    _exit(42);
  }
  return wait_for_exit(pid, 42);
}

static int test_fork_fd_inheritance(void) {
  int pipefd[2];
  pid_t pid;
  char byte = 0;

  if (pipe(pipefd) != 0) {
    return fail("pipe");
  }
  pid = _Fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("_Fork");
  }
  if (pid == 0) {
    close(pipefd[0]);
    if (write(pipefd[1], "x", 1) != 1) {
      _exit(125);
    }
    close(pipefd[1]);
    _exit(17);
  }
  close(pipefd[1]);
  if (read(pipefd[0], &byte, 1) != 1 || byte != 'x') {
    close(pipefd[0]);
    return fail("pipe inheritance");
  }
  close(pipefd[0]);
  return wait_for_exit(pid, 17);
}

static int test_pthread_atfork_order(void) {
  int pipefd[2];
  pid_t pid;
  int child_state = 0;

  atfork_state = 0;
  if (pthread_atfork(atfork_prepare_one, atfork_parent_one, atfork_child_one) != 0 ||
      pthread_atfork(atfork_prepare_two, atfork_parent_two, atfork_child_two) != 0) {
    return fail("pthread_atfork register");
  }
  if (pipe(pipefd) != 0) {
    return fail("atfork pipe");
  }
  pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("atfork fork");
  }
  if (pid == 0) {
    close(pipefd[0]);
    if (write(pipefd[1], &atfork_state, sizeof(atfork_state)) !=
        (ssize_t)sizeof(atfork_state)) {
      _exit(126);
    }
    close(pipefd[1]);
    _exit(0);
  }
  close(pipefd[1]);
  if (read(pipefd[0], &child_state, sizeof(child_state)) !=
      (ssize_t)sizeof(child_state)) {
    close(pipefd[0]);
    return fail("atfork child read");
  }
  close(pipefd[0]);
  if (wait_for_exit(pid, 0) != 0) {
    return 1;
  }
  if (atfork_state != 2112 || child_state != 2112) {
    return fail("atfork order");
  }
  return 0;
}

int main(void) {
  if (test_fork_basic() != 0 ||
      test_fork_fd_inheritance() != 0 ||
      test_pthread_atfork_order() != 0) {
    return 1;
  }
  puts("fork_test: ok");
  return 0;
}
