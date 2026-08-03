#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int signal_pipe[2];

static int fail(const char* message) {
  fprintf(stderr, "fork_signal_test: %s\n", message);
  return 1;
}

static void handle_child_signal(int sig) {
  char byte = (char)sig;

  (void)write(signal_pipe[1], &byte, 1);
}

int main(void) {
  struct sigaction action;
  pid_t pid;
  int status = 0;
  char byte = 0;

  if (pipe(signal_pipe) != 0) {
    return fail("pipe");
  }
  memset(&action, 0, sizeof(action));
  action.sa_handler = handle_child_signal;
  if (sigaction(SIGUSR1, &action, 0) != 0) {
    return fail("sigaction");
  }
  pid = fork();
  if (pid < 0) {
#if defined(CRT_TARGET_OS_WINDOWS)
    if (errno == ENOTSUP) {
      close(signal_pipe[0]);
      close(signal_pipe[1]);
      puts("fork_signal_test: ok");
      return 0;
    }
#endif
    return fail("fork");
  }
  if (pid == 0) {
    close(signal_pipe[0]);
    if (raise(SIGUSR1) != 0) {
      _exit(125);
    }
    close(signal_pipe[1]);
    _exit(23);
  }
  close(signal_pipe[1]);
  if (read(signal_pipe[0], &byte, 1) != 1 || byte != (char)SIGUSR1) {
    close(signal_pipe[0]);
    return fail("child signal action");
  }
  close(signal_pipe[0]);
  if (waitpid(pid, &status, 0) != pid ||
      !WIFEXITED(status) ||
      WEXITSTATUS(status) != 23) {
    return fail("wait status");
  }
  puts("fork_signal_test: ok");
  return 0;
}
