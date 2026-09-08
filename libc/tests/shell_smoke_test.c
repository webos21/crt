#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <private/crt_shell_process.h>

static const char* self_path;

static int fail(const char* message) {
  fprintf(stderr, "shell_smoke_test: %s\n", message);
  return 1;
}

static int write_all(int fd, const char* text) {
  size_t length = strlen(text);
  size_t offset = 0;

  while (offset < length) {
    ssize_t written = write(fd, text + offset, length - offset);

    if (written <= 0) {
      return -1;
    }
    offset += (size_t)written;
  }
  return 0;
}

static int read_to_buffer(int fd, char* buffer, size_t size) {
  size_t offset = 0;

  if (size == 0) {
    return -1;
  }
  while (offset + 1 < size) {
    ssize_t got = read(fd, buffer + offset, size - offset - 1);

    if (got < 0) {
      return -1;
    }
    if (got == 0) {
      break;
    }
    offset += (size_t)got;
  }
  buffer[offset] = 0;
  return 0;
}

static int wait_for_exit(pid_t pid, int expected) {
  int status = 0;

  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != expected) {
    return -1;
  }
  return 0;
}

static int run_child_mode(int argc, char** argv) {
  if (argc >= 2 && strcmp(argv[1], "producer") == 0) {
    return write_all(1, "hello\n") == 0 ? 5 : 77;
  }
  if (argc >= 2 && strcmp(argv[1], "upper") == 0) {
    char ch;

    while (read(0, &ch, 1) == 1) {
      if (ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - 'a' + 'A');
      }
      if (write(1, &ch, 1) != 1) {
        return 78;
      }
    }
    return 0;
  }
  if (argc >= 2 && strcmp(argv[1], "copy") == 0) {
    char ch;

    while (read(0, &ch, 1) == 1) {
      if (write(1, &ch, 1) != 1) {
        return 79;
      }
    }
    return 0;
  }
  if (argc >= 2 && strcmp(argv[1], "fd3") == 0) {
    return write_all(3, "fd3\n") == 0 ? 3 : 80;
  }
  if (argc >= 2 && strcmp(argv[1], "sleeper") == 0) {
    struct timespec delay;

    delay.tv_sec = 0;
    delay.tv_nsec = 200000000L;
    (void)nanosleep(&delay, 0);
    return 6;
  }
  if (argc >= 2 && strcmp(argv[1], "exec-target") == 0) {
    return 17;
  }
  if (argc >= 2 && strcmp(argv[1], "exec-wrapper") == 0) {
    char* child_argv[] = {(char*)self_path, "exec-target", 0};

    (void)execve(self_path, child_argv, environ);
    return 81;
  }
  if (argc >= 2 && strcmp(argv[1], "sig-child") == 0) {
    struct sigaction old_action;
#if defined(CRT_TARGET_OS_WINDOWS)
    sigset_t mask = 0;

    if (sigprocmask(SIG_SETMASK, 0, &mask) != 0 || sigismember(&mask, SIGINT) != 1) {
      return 82;
    }
#endif
    memset(&old_action, 0, sizeof(old_action));
    if (sigaction(SIGINT, 0, &old_action) != 0 || old_action.sa_handler != SIG_DFL) {
      return 83;
    }
    return 19;
  }
  return -1;
}

static int spawn_with_spec(pid_t* pid, char** argv, const posix_spawn_file_actions_t* actions) {
  struct crt_shell_child_spec spec;

  memset(&spec, 0, sizeof(spec));
  spec.path = self_path;
  spec.argv = argv;
  spec.envp = environ;
  spec.file_actions = actions;
  spec.flags = CRT_SHELL_CHILD_FLUSH_STDIO;
  return __crt_shell_spawn(pid, &spec);
}

static int test_pipeline(void) {
  int pipe_a[2];
  int pipe_b[2];
  posix_spawn_file_actions_t producer_actions;
  posix_spawn_file_actions_t upper_actions;
  char* producer_argv[] = {(char*)self_path, "producer", 0};
  char* upper_argv[] = {(char*)self_path, "upper", 0};
  char buffer[32];
  pid_t producer;
  pid_t upper;

  if (pipe(pipe_a) != 0 || pipe(pipe_b) != 0) {
    return fail("pipeline pipe");
  }
  if (posix_spawn_file_actions_init(&producer_actions) != 0 ||
      posix_spawn_file_actions_adddup2(&producer_actions, pipe_a[1], 1) != 0 ||
      posix_spawn_file_actions_addclose(&producer_actions, pipe_a[0]) != 0 ||
      posix_spawn_file_actions_addclose(&producer_actions, pipe_b[0]) != 0 ||
      posix_spawn_file_actions_addclose(&producer_actions, pipe_b[1]) != 0) {
    return fail("pipeline producer actions");
  }
  if (posix_spawn_file_actions_init(&upper_actions) != 0 ||
      posix_spawn_file_actions_adddup2(&upper_actions, pipe_a[0], 0) != 0 ||
      posix_spawn_file_actions_adddup2(&upper_actions, pipe_b[1], 1) != 0 ||
      posix_spawn_file_actions_addclose(&upper_actions, pipe_a[1]) != 0 ||
      posix_spawn_file_actions_addclose(&upper_actions, pipe_b[0]) != 0) {
    return fail("pipeline upper actions");
  }
  if (spawn_with_spec(&producer, producer_argv, &producer_actions) != 0 ||
      spawn_with_spec(&upper, upper_argv, &upper_actions) != 0) {
    return fail("pipeline spawn");
  }
  posix_spawn_file_actions_destroy(&producer_actions);
  posix_spawn_file_actions_destroy(&upper_actions);
  close(pipe_a[0]);
  close(pipe_a[1]);
  close(pipe_b[1]);
  if (read_to_buffer(pipe_b[0], buffer, sizeof(buffer)) != 0 || strcmp(buffer, "HELLO\n") != 0) {
    close(pipe_b[0]);
    return fail("pipeline output");
  }
  close(pipe_b[0]);
  if (wait_for_exit(producer, 5) != 0 || wait_for_exit(upper, 0) != 0) {
    return fail("pipeline wait");
  }
  return 0;
}

static int test_redirection(void) {
  posix_spawn_file_actions_t actions;
  char* producer_argv[] = {(char*)self_path, "producer", 0};
  char* copy_argv[] = {(char*)self_path, "copy", 0};
  char buffer[32];
  pid_t pid;
  int fd;
  int pipefd[2];

  (void)unlink("shell_smoke_out.txt");
  if (posix_spawn_file_actions_init(&actions) != 0 ||
      posix_spawn_file_actions_addopen(
          &actions, 1, "shell_smoke_out.txt", O_CREAT | O_WRONLY | O_TRUNC, 0600) != 0) {
    return fail("stdout redirect setup");
  }
  if (spawn_with_spec(&pid, producer_argv, &actions) != 0) {
    return fail("stdout redirect spawn");
  }
  posix_spawn_file_actions_destroy(&actions);
  if (wait_for_exit(pid, 5) != 0) {
    return fail("stdout redirect wait");
  }
  fd = open("shell_smoke_out.txt", O_RDONLY, 0);
  if (fd < 0 ||
      read_to_buffer(fd, buffer, sizeof(buffer)) != 0 ||
      strcmp(buffer, "hello\n") != 0) {
    if (fd >= 0) {
      close(fd);
    }
    return fail("stdout redirect output");
  }
  close(fd);

  fd = open("shell_smoke_in.txt", O_CREAT | O_WRONLY | O_TRUNC, 0600);
  if (fd < 0 || write_all(fd, "input\n") != 0 || close(fd) != 0 || pipe(pipefd) != 0) {
    return fail("stdin redirect setup");
  }
  if (posix_spawn_file_actions_init(&actions) != 0 ||
      posix_spawn_file_actions_addopen(&actions, 0, "shell_smoke_in.txt", O_RDONLY, 0) != 0 ||
      posix_spawn_file_actions_adddup2(&actions, pipefd[1], 1) != 0 ||
      posix_spawn_file_actions_addclose(&actions, pipefd[0]) != 0) {
    return fail("stdin redirect actions");
  }
  if (spawn_with_spec(&pid, copy_argv, &actions) != 0) {
    return fail("stdin redirect spawn");
  }
  posix_spawn_file_actions_destroy(&actions);
  close(pipefd[1]);
  if (read_to_buffer(pipefd[0], buffer, sizeof(buffer)) != 0 || strcmp(buffer, "input\n") != 0) {
    close(pipefd[0]);
    return fail("stdin redirect output");
  }
  close(pipefd[0]);
  if (wait_for_exit(pid, 0) != 0) {
    return fail("stdin redirect wait");
  }
  (void)unlink("shell_smoke_out.txt");
  (void)unlink("shell_smoke_in.txt");
  return 0;
}

static int test_high_fd(void) {
  int pipefd[2];
  posix_spawn_file_actions_t actions;
  char* child_argv[] = {(char*)self_path, "fd3", 0};
  char buffer[32];
  pid_t pid;

  if (pipe(pipefd) != 0 ||
      posix_spawn_file_actions_init(&actions) != 0 ||
      posix_spawn_file_actions_adddup2(&actions, pipefd[1], 3) != 0) {
    return fail("fd3 setup");
  }
  if (spawn_with_spec(&pid, child_argv, &actions) != 0) {
    return fail("fd3 spawn");
  }
  posix_spawn_file_actions_destroy(&actions);
  close(pipefd[1]);
  if (read_to_buffer(pipefd[0], buffer, sizeof(buffer)) != 0 || strcmp(buffer, "fd3\n") != 0) {
    close(pipefd[0]);
    return fail("fd3 output");
  }
  close(pipefd[0]);
  if (wait_for_exit(pid, 3) != 0) {
    return fail("fd3 wait");
  }
  return 0;
}

static int test_exec_and_wait(void) {
  char* exec_argv[] = {(char*)self_path, "exec-wrapper", 0};
  char* sleep_argv[] = {(char*)self_path, "sleeper", 0};
  pid_t pid;
  int status = 0;

  if (spawn_with_spec(&pid, exec_argv, 0) != 0 || wait_for_exit(pid, 17) != 0) {
    return fail("exec-like flow");
  }
  if (spawn_with_spec(&pid, sleep_argv, 0) != 0) {
    return fail("wnohang spawn");
  }
  if (waitpid(pid, &status, WNOHANG) != 0) {
    return fail("wnohang early");
  }
  if (wait_for_exit(pid, 6) != 0) {
    return fail("wnohang final");
  }
  return 0;
}

static int test_signal_contract(void) {
  struct crt_shell_child_spec spec;
  sigset_t mask = 0;
  char* child_argv[] = {(char*)self_path, "sig-child", 0};
  pid_t pid;

  if (sigemptyset(&mask) != 0 ||
      sigaddset(&mask, SIGINT) != 0 ||
      signal(SIGINT, SIG_IGN) == SIG_ERR) {
    return fail("signal setup");
  }
  memset(&spec, 0, sizeof(spec));
  spec.path = self_path;
  spec.argv = child_argv;
  spec.envp = environ;
  spec.sigmask = (sigset64_t)mask;
  spec.sigdefault = (sigset64_t)mask;
  spec.flags = CRT_SHELL_CHILD_FLUSH_STDIO |
               CRT_SHELL_CHILD_SET_SIGMASK |
               CRT_SHELL_CHILD_SET_SIGDEFAULT;
  if (__crt_shell_spawn(&pid, &spec) != 0) {
    return fail("signal spawn");
  }
  (void)signal(SIGINT, SIG_DFL);
  if (wait_for_exit(pid, 19) != 0) {
    return fail("signal wait");
  }
  return 0;
}

int main(int argc, char** argv) {
  int child_result;

  self_path = argv != 0 && argv[0] != 0 ? argv[0] : "/proc/self/exe";
  child_result = run_child_mode(argc, argv);
  if (child_result >= 0) {
    return child_result;
  }
  if (argc >= 2 && strcmp(argv[1], "run-pipeline") == 0) {
    return test_pipeline();
  }
  if (argc >= 2 && strcmp(argv[1], "run-redirection") == 0) {
    return test_redirection();
  }
  if (argc >= 2 && strcmp(argv[1], "run-fd3") == 0) {
    return test_high_fd();
  }
  if (argc >= 2 && strcmp(argv[1], "run-exec-wait") == 0) {
    return test_exec_and_wait();
  }
  if (argc >= 2 && strcmp(argv[1], "run-signal") == 0) {
    return test_signal_contract();
  }
  if (test_pipeline() != 0) {
    return 1;
  }
  if (test_redirection() != 0) {
    return 1;
  }
  if (test_high_fd() != 0) {
    return 1;
  }
  if (test_exec_and_wait() != 0) {
    return 1;
  }
  if (test_signal_contract() != 0) {
    return 1;
  }
  puts("shell_smoke_test: ok");
  return 0;
}
