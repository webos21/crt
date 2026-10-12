/* kill() of another process: the existence probe, SIGKILL, and what waitpid() then reports. */

#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

int main(int argc, char** argv) {
  char* args[3];
  pid_t pid;
  int status = 0;

  if (argc == 2 && strcmp(argv[1], "child") == 0) {
    sleep(20);
    return 0;
  }
  args[0] = argv[0];
  args[1] = "child";
  args[2] = NULL;
  if (posix_spawn(&pid, argv[0], NULL, NULL, args, environ) != 0) {
    perror("posix_spawn");
    return 1;
  }
  if (kill(pid, 0) != 0) {
    fprintf(stderr, "spawn_kill_test: kill(pid, 0) on a live child failed (errno %d)\n", errno);
    return 2;
  }
  if (kill(pid, SIGKILL) != 0) {
    fprintf(stderr, "spawn_kill_test: kill(pid, SIGKILL) failed (errno %d)\n", errno);
    return 3;
  }
  if (waitpid(pid, &status, 0) != pid) {
    fprintf(stderr, "spawn_kill_test: waitpid failed (errno %d)\n", errno);
    return 4;
  }
  if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL) {
    fprintf(stderr, "spawn_kill_test: status 0x%x is not 'killed by SIGKILL'\n", status);
    return 5;
  }
  if (kill(pid, 0) == 0 || errno != ESRCH) {
    fprintf(stderr, "spawn_kill_test: kill(pid, 0) on a reaped child gave errno %d\n", errno);
    return 6;
  }
  printf("spawn_kill_test: ok\n");
  return 0;
}
