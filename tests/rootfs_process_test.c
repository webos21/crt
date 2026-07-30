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
  if (execve("/proc/self/exe", child_argv, environ) != -1 || errno != ENOTSUP) {
    return fail("execve unsupported policy");
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
