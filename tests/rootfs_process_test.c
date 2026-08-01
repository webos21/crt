#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int fail(const char* message) {
  printf("rootfs_process_test: %s\n", message);
  return 1;
}

int main(int argc, char** argv) {
  posix_spawnattr_t attr;
  posix_spawn_file_actions_t actions;
  struct sched_param sched;
  short flags;
  pid_t pgroup;
  sigset_t sigmask;
#if defined(CRT_TARGET_OS_WINDOWS)
  char cwd[1024];
  char root[1400];
  char posix_cwd[128];
  struct stat st;
  int fd;
#endif

  if (argc == 2 && strcmp(argv[1], "child") == 0) {
    return 7;
  }
#if defined(CRT_TARGET_OS_WINDOWS)
  if (argc == 2 && strcmp(argv[1], "bootstrap-child") == 0) {
    sigset_t current_mask = 0;

    if (getenv("CRT_ROOTFS") == 0 ||
        getcwd(posix_cwd, sizeof(posix_cwd)) == 0 ||
        strcmp(posix_cwd, "/tmp") != 0 ||
        sigprocmask(SIG_SETMASK, 0, &current_mask) != 0 ||
        sigismember(&current_mask, SIGINT) != 1) {
      return 88;
    }
    return 14;
  }
  if (argc == 2 && strcmp(argv[1], "exec-target") == 0) {
    return 13;
  }
  if (argc == 2 && strcmp(argv[1], "exec-wrapper") == 0) {
    char* exec_argv[] = {"/proc/self/exe", "exec-target", 0};

    (void)execve("/proc/self/exe", exec_argv, environ);
    return 89;
  }
#endif

  if (posix_spawnattr_init(&attr) != 0 ||
      posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP) != 0 ||
      posix_spawnattr_getflags(&attr, &flags) != 0 ||
      flags != POSIX_SPAWN_SETPGROUP ||
      posix_spawnattr_setpgroup(&attr, 123) != 0 ||
      posix_spawnattr_getpgroup(&attr, &pgroup) != 0 ||
      pgroup != 123) {
    return fail("spawn attr flags/pgroup");
  }
  sched.sched_priority = 5;
  if (posix_spawnattr_setschedparam(&attr, &sched) != 0 ||
      posix_spawnattr_getschedparam(&attr, &sched) != 0 ||
      sched.sched_priority != 5 ||
      posix_spawnattr_setschedpolicy(&attr, SCHED_OTHER) != 0 ||
      posix_spawnattr_getschedpolicy(&attr, &sched.sched_priority) != 0 ||
      sched.sched_priority != SCHED_OTHER) {
    return fail("spawn attr sched");
  }
  sigmask = 3;
  if (posix_spawnattr_setsigmask(&attr, &sigmask) != 0 ||
      posix_spawnattr_getsigmask(&attr, &sigmask) != 0 ||
      sigmask != 3 ||
      posix_spawnattr_setsigdefault(&attr, &sigmask) != 0 ||
      posix_spawnattr_getsigdefault(&attr, &sigmask) != 0 ||
      sigmask != 3) {
    return fail("spawn attr signal");
  }
  if (posix_spawnattr_destroy(&attr) != 0) {
    return fail("spawn attr destroy");
  }
  if (posix_spawn_file_actions_init(&actions) != 0 ||
      posix_spawn_file_actions_addclose(&actions, 3) != 0 ||
      posix_spawn_file_actions_adddup2(&actions, 0, 3) != 0 ||
      posix_spawn_file_actions_addopen(&actions, 4, "/dev/null", O_RDONLY, 0) != 0 ||
      posix_spawn_file_actions_addchdir_np(&actions, "/tmp") != 0 ||
      posix_spawn_file_actions_addfchdir_np(&actions, 0) != 0 ||
      posix_spawn_file_actions_destroy(&actions) != 0) {
    return fail("spawn file actions");
  }

#if defined(CRT_TARGET_OS_WINDOWS)
  pid_t pid;
  int status = 0;
  char* child_argv[] = {"/proc/self/exe", "child", 0};

  if (getcwd(cwd, sizeof(cwd)) == 0) {
    return fail("getcwd native");
  }
  snprintf(root, sizeof(root), "%s/rootfs_process_test.root", cwd);
  (void)mkdir(root, 0777);
  if (setenv("CRT_ROOTFS", root, 1) != 0) {
    return fail("setenv CRT_ROOTFS");
  }
  if (mkdir("/tmp", 0777) != 0 && errno != EEXIST) {
    return fail("mkdir /tmp");
  }
  fd = open("/tmp/sample.txt", O_CREAT | O_RDWR | O_TRUNC, 0600);
  if (fd < 0) {
    return fail("open rootfs file");
  }
  if (write(fd, "ok", 2) != 2 || close(fd) != 0) {
    return fail("write rootfs file");
  }
  if (access("/tmp/sample.txt", R_OK | W_OK) != 0) {
    return fail("access rootfs file");
  }
  if (stat("/dev/null", &st) != 0 || !S_ISCHR(st.st_mode)) {
    return fail("stat /dev/null");
  }
  if (access("/proc/self/exe", R_OK) != 0) {
    return fail("access /proc/self/exe");
  }
  if (chdir("/tmp") != 0 || getcwd(posix_cwd, sizeof(posix_cwd)) == 0 ||
      strcmp(posix_cwd, "/tmp") != 0) {
    return fail("chdir/getcwd rootfs");
  }
  if (chdir(cwd) != 0) {
    return fail("chdir back");
  }
  if (posix_spawn(&pid, "/proc/self/exe", 0, 0, child_argv, environ) != 0) {
    return fail("posix_spawn");
  }
  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 7) {
    return fail("waitpid");
  }
  if (posix_spawn(0, "/proc/self/exe", 0, 0, child_argv, environ) != 0) {
    return fail("posix_spawn null pid");
  }
  if (wait(0) <= 0) {
    return fail("wait null pid child");
  }
  {
    posix_spawnattr_t child_attr;
    posix_spawn_file_actions_t child_actions;
    sigset_t child_mask = 0;
    char* bootstrap_argv[] = {"/proc/self/exe", "bootstrap-child", 0};

    if (sigemptyset(&child_mask) != 0 ||
        sigaddset(&child_mask, SIGINT) != 0 ||
        posix_spawnattr_init(&child_attr) != 0 ||
        posix_spawnattr_setflags(
            &child_attr, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF) != 0 ||
        posix_spawnattr_setsigmask(&child_attr, &child_mask) != 0 ||
        posix_spawnattr_setsigdefault(&child_attr, &child_mask) != 0 ||
        posix_spawn_file_actions_init(&child_actions) != 0 ||
        posix_spawn_file_actions_addchdir_np(&child_actions, "/tmp") != 0) {
      return fail("bootstrap setup");
    }
    if (posix_spawn(&pid, "/proc/self/exe", &child_actions, &child_attr, bootstrap_argv, environ) != 0) {
      posix_spawn_file_actions_destroy(&child_actions);
      posix_spawnattr_destroy(&child_attr);
      return fail("bootstrap spawn");
    }
    posix_spawn_file_actions_destroy(&child_actions);
    posix_spawnattr_destroy(&child_attr);
    if (waitpid(pid, &status, 0) != pid ||
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 14) {
      return fail("bootstrap wait");
    }
  }
  {
    char* exec_argv[] = {"/proc/self/exe", "exec-wrapper", 0};

    if (posix_spawn(&pid, "/proc/self/exe", 0, 0, exec_argv, environ) != 0) {
      return fail("exec wrapper spawn");
    }
    if (waitpid(pid, &status, 0) != pid ||
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 13) {
      return fail("exec wrapper wait");
    }
  }
  if (unlink("/tmp/sample.txt") != 0 || rmdir("/tmp") != 0) {
    return fail("cleanup rootfs");
  }
  (void)unsetenv("CRT_ROOTFS");
#else
  if (posix_spawn(0, 0, 0, 0, 0, 0) != EINVAL) {
    return fail("posix_spawn invalid");
  }
#endif

  printf("rootfs_process_test: ok\n");
  return 0;
}
