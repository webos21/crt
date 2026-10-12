/* posix_spawn() reports a failure between fork and exec as its return value (Bionic and glibc do),
 * and readlink("/proc/self/exe") yields the executable's own path. WebKit's process launcher finds
 * its sibling executables through the second and must hear about a missing one through the first.
 * Unix hosts only: the Windows spawn path has its own status reporting. */

#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(_WIN32) || defined(__MINGW32__)
int main(void) {
  printf("spawn_status_test: ok\n");
  return 0;
}
#else
extern char** environ;

static int fail(const char* what) {
  fprintf(stderr, "spawn_status_test: FAILED %s (errno %d)\n", what, errno);
  return 1;
}

int main(int argc, char** argv) {
  char path[4096];
  struct stat st;
  ssize_t length;
  pid_t pid = -1;
  int status = 0;
  int result;
  char* child_argv[] = {"spawn_status_test", "child", 0};

  if (argc > 1 && strcmp(argv[1], "child") == 0) {
    return 7;
  }

  length = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (length <= 0 || path[0] != '/') {
    return fail("readlink(/proc/self/exe) is an absolute path");
  }
  path[length] = 0;
  if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || (st.st_mode & 0100) == 0) {
    return fail("the link target is the running executable");
  }

  /* Exec failure: ENOENT comes back, no child pid is handed out, and nothing is left to reap. */
  result = posix_spawn(&pid, "/nonexistent/crt-spawn-status/none", 0, 0, child_argv, environ);
  if (result != ENOENT) {
    fprintf(stderr, "spawn_status_test: posix_spawn returned %d\n", result);
    return fail("posix_spawn of a missing executable returns ENOENT");
  }
  if (waitpid(-1, &status, WNOHANG) != -1 || errno != ECHILD) {
    return fail("the failed spawn left a child behind");
  }

  /* A failing file action is reported too. */
  {
    posix_spawn_file_actions_t actions;

    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, 9, "/nonexistent/crt-spawn-status/in", O_RDONLY, 0);
    result = posix_spawn(&pid, path, &actions, 0, child_argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (result != ENOENT) {
      return fail("a failing file action is reported by posix_spawn");
    }
    if (waitpid(-1, &status, WNOHANG) != -1 || errno != ECHILD) {
      return fail("the failed file action left a child behind");
    }
  }

  /* Success is still success, and the status pipe does not leak into the child. */
  result = posix_spawn(&pid, path, 0, 0, child_argv, environ);
  if (result != 0 || pid <= 0) {
    return fail("posix_spawn of this executable");
  }
  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 7) {
    return fail("the spawned child ran and exited 7");
  }

  printf("spawn_status_test: ok\n");
  return 0;
}
#endif
