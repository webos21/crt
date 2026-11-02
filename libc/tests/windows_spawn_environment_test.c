/* A CRT child's environment is what its parent passed (docs: the CLI's shared child-environment contract).
 * The Windows PAL hands cwd, rootfs, signal state and the fd snapshot to a CRT child through CRT_* variables; the child
 * consumes them at startup and they must not remain visible in its environment. Their effects must remain. */
#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

static int is_internal(const char* entry) {
  return strncmp(entry, "CRT_BOOTSTRAP_", 14) == 0 || strncmp(entry, "CRT_CHILD_BOOTSTRAP=", 20) == 0 ||
         strncmp(entry, "CRT_FD_SNAPSHOT", 15) == 0;
}

static int child(const char* expected_cwd) {
  char cwd[1024];
  int seen_marker = 0;
  char** entry;
  for (entry = environ; *entry; ++entry) {
    if (is_internal(*entry)) { fprintf(stderr, "leaked: %s\n", *entry); return 21; }
    if (strcmp(*entry, "CRT_TEST_MARKER=kept") == 0) seen_marker = 1;
  }
  if (!seen_marker) return 22;
  if (getenv("CRT_CHILD_BOOTSTRAP") != 0 || getenv("CRT_FD_SNAPSHOT_PIPE") != 0) return 23;
  /* The effect of the consumed bootstrap remains: the parent's working directory reached this process. */
  if (getcwd(cwd, sizeof(cwd)) == 0) return 24;
  {
    size_t length = strlen(expected_cwd), i;
    for (i = 0; i < length && cwd[i]; ++i) {
      char a = expected_cwd[i] == '\\' ? '/' : expected_cwd[i], b = cwd[i] == '\\' ? '/' : cwd[i];
      if (a != b) return 25;
    }
    if (cwd[length] != 0) return 25;
  }
  return 0;
}

int main(int argc, char** argv) {
  char* envp[] = {"PATH=/nonexistent", "LC_ALL=C", "CRT_TEST_MARKER=kept", 0};
  char* arguments[4];
  char cwd[1024];
  pid_t pid;
  int status = 0;
  if (argc == 3 && strcmp(argv[1], "--child") == 0) return child(argv[2]);

  /* CTest runs this image through an absolute path; the child must start from a different working directory. */
  mkdir("windows-spawn-environment-dir", 0755);
  if (chdir("windows-spawn-environment-dir") != 0 || getcwd(cwd, sizeof(cwd)) == 0) return 1;
  arguments[0] = argv[0];
  arguments[1] = (char*)"--child";
  arguments[2] = cwd;
  arguments[3] = 0;
  if (posix_spawn(&pid, argv[0], 0, 0, arguments, envp) != 0) return 2;
  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status)) return 5;
  if (WEXITSTATUS(status) != 0) return 30 + WEXITSTATUS(status);
  /* The parent never sees the transport variables either. */
  if (getenv("CRT_CHILD_BOOTSTRAP") != 0) return 6;
  puts("windows_spawn_environment_test: ok");
  return 0;
}
