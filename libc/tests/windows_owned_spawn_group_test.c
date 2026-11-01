/* Owned spawn cancellation must cover descendants after the leader is reaped,
 * and must never terminate a separately spawned sibling. */
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char** environ;

static void pause_tick(void) {
  struct timespec delay = {0, 10000000};
  nanosleep(&delay, 0);
}

static pid_t ready_pid(const char* path) {
  int i;
  for (i = 0; i < 500; ++i) {
    FILE* file = fopen(path, "r");
    long pid = 0;
    if (file != 0) {
      int scanned = fscanf(file, "%ld", &pid);
      fclose(file);
      if (scanned == 1 && pid > 0) return (pid_t)pid;
    }
    pause_tick();
  }
  return -1;
}

static int gone(pid_t pid) {
  int i;
  for (i = 0; i < 300; ++i) {
    if (kill(pid, 0) == -1 && errno == ESRCH) return 1;
    pause_tick();
  }
  return 0;
}

static int fail(const char* message) {
  fprintf(stderr, "windows_owned_spawn_group_test: %s (errno %d)\n", message, errno);
  return 1;
}

static int run_tree(const char* self, int native, int orphan) {
  char ready[4096];
  const char* temporary = getenv("TEMP");
  pid_t leader = -1, sibling = -1, leaf = -1;
  posix_spawnattr_t attr;
  int status, ok = 0;
  char* tree_args[] = {(char*)self, orphan ? "orphan" : "parent", ready, 0};
  char* sibling_args[] = {(char*)self, "sleep", 0};
  snprintf(ready, sizeof ready, "%s/crt-owned-group-%ld-%d-%d", temporary ? temporary : ".",
           (long)getpid(), native, orphan);
  unlink(ready);
  if (posix_spawnattr_init(&attr) != 0) return fail("attributes");
  posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
  posix_spawnattr_setpgroup(&attr, 0);
  if (native) setenv("CRT_SPAWN_NATIVE_WINDOWS", "1", 1);
  else unsetenv("CRT_SPAWN_NATIVE_WINDOWS");
  if (posix_spawn(&sibling, self, 0, &attr, sibling_args, environ) != 0 ||
      posix_spawn(&leader, self, 0, &attr, tree_args, environ) != 0) goto done;
  leaf = ready_pid(ready);
  if (leaf <= 0) goto done;
  if (orphan && (waitpid(leader, &status, 0) != leader ||
                 !WIFEXITED(status) || WEXITSTATUS(status) != 0)) goto done;
  if (kill(-leader, 0) != 0) goto done;
  errno = 0;
  if (kill(-leader, SIGTERM) != -1 || errno != ENOSYS) goto done;
  if (kill(-leader, SIGKILL) != 0 || !gone(leaf) || kill(sibling, 0) != 0) goto done;
  if (!orphan && (waitpid(leader, &status, 0) != leader ||
                  !WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL)) goto done;
  ok = 1;
done:
  unsetenv("CRT_SPAWN_NATIVE_WINDOWS");
  if (leader > 0) { kill(-leader, SIGKILL); kill(leader, SIGKILL); waitpid(leader, &status, 0); }
  if (leaf > 0 && !ok) kill(leaf, SIGKILL);
  if (sibling > 0) { kill(-sibling, SIGKILL); kill(sibling, SIGKILL); waitpid(sibling, &status, 0); }
  posix_spawnattr_destroy(&attr);
  unlink(ready);
  return ok ? 0 : fail("group/tree cancellation, reaped leader or sibling isolation");
}

int main(int argc, char** argv) {
  if (argc >= 2 && strcmp(argv[1], "quick") == 0) return 0;
  if (argc >= 2 && strcmp(argv[1], "sleep") == 0) { sleep(20); return 0; }
  if (argc == 3 && strcmp(argv[1], "leaf") == 0) {
    FILE* file = fopen(argv[2], "w");
    if (!file) return 10;
    fprintf(file, "%ld\n", (long)getpid());
    fclose(file);
    sleep(20);
    return 0;
  }
  if (argc == 3 && (strcmp(argv[1], "parent") == 0 || strcmp(argv[1], "orphan") == 0)) {
    char* args[] = {argv[0], "leaf", argv[2], 0};
    pid_t leaf;
    if (posix_spawn(&leaf, argv[0], 0, 0, args, environ) != 0 || ready_pid(argv[2]) <= 0) return 11;
    if (strcmp(argv[1], "parent") == 0) sleep(20);
    return 0;
  }
  {
    posix_spawnattr_t attr;
    pid_t pid;
    int status, i, mode, orphan;
    char* args[] = {argv[0], "quick", 0};
    for (mode = 0; mode < 2; ++mode)
      for (orphan = 0; orphan < 2; ++orphan)
        if (run_tree(argv[0], mode, orphan)) return 1;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attr, 0);
    /* More cycles than the fixed registry capacity expose leaked failed/empty jobs. */
    for (i = 0; i < 80; ++i) {
      if (posix_spawn(&pid, "C:/crt-no-such-group-test.exe", 0, &attr, args, environ) != ENOENT)
        return fail("failed spawn cleanup");
      if (posix_spawn(&pid, argv[0], 0, &attr, args, environ) != 0 ||
          waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return fail("empty group reclamation");
    }
    posix_spawnattr_setpgroup(&attr, -1);
    if (posix_spawn(&pid, argv[0], 0, &attr, args, environ) != EINVAL) return fail("negative pgroup");
    posix_spawnattr_setpgroup(&attr, getpid());
    if (posix_spawn(&pid, argv[0], 0, &attr, args, environ) != ENOTSUP) return fail("unsupported group join");
    posix_spawnattr_destroy(&attr);
  }
  puts("windows_owned_spawn_group_test: ok");
  return 0;
}
