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

  if (argc >= 3 && strcmp(argv[1], "group-child") == 0) {
    pid_t expected = strcmp(argv[2], "self") == 0 ? getpid() : (pid_t)atoi(argv[2]);
    sigset_t mask;
    if (getpgrp() != expected) return 20;
    if (argc == 4 && (sigprocmask(SIG_SETMASK, 0, &mask) != 0 || sigismember(&mask, SIGUSR1) != 1)) return 21;
    return 0;
  }

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

  /* CLI children need their own group; preserve masks and report attribute errors synchronously. */
  {
    posix_spawnattr_t attr;
    sigset_t mask, parent_mask;
    char parent_group[32];
    char* group_argv[] = {path, "group-child", "self", "masked", 0};
    pid_t original_group = getpgrp();
    int parent_blocked;

    if (sigprocmask(SIG_SETMASK, 0, &parent_mask) != 0) return fail("read parent mask");
    parent_blocked = sigismember(&parent_mask, SIGUSR1);
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    if (posix_spawnattr_init(&attr) != 0 ||
        posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK) != 0 ||
        posix_spawnattr_setpgroup(&attr, 0) != 0 || posix_spawnattr_setsigmask(&attr, &mask) != 0) {
      return fail("prepare process group attributes");
    }
    result = posix_spawn(&pid, path, 0, &attr, group_argv, environ);
    if (result != 0) {
      fprintf(stderr, "spawn_status_test: SETPGROUP returned %d\n", result);
      posix_spawnattr_destroy(&attr);
      return fail("spawn child in its own group with a signal mask");
    }
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      posix_spawnattr_destroy(&attr);
      return fail("child process group and signal mask");
    }
    snprintf(parent_group, sizeof parent_group, "%d", (int)original_group);
    group_argv[2] = parent_group;
    group_argv[3] = 0;
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attr, original_group);
    result = posix_spawn(&pid, path, 0, &attr, group_argv, environ);
    if (result != 0 || waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      posix_spawnattr_destroy(&attr);
      return fail("join an existing process group");
    }
    posix_spawnattr_setpgroup(&attr, -1);
    pid = -1;
    result = posix_spawn(&pid, path, 0, &attr, group_argv, environ);
    posix_spawnattr_destroy(&attr);
    if (result != EINVAL || pid != -1 || waitpid(-1, &status, WNOHANG) != -1 || errno != ECHILD) {
      return fail("invalid process group returns EINVAL without a child to reap");
    }
    if (getpgrp() != original_group || sigprocmask(SIG_SETMASK, 0, &mask) != 0 ||
        sigismember(&mask, SIGUSR1) != parent_blocked) return fail("parent attributes unchanged");
  }

  printf("spawn_status_test: ok\n");
  return 0;
}
#endif
