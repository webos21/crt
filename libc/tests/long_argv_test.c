/* A process must receive every argument it was spawned with. The Windows startup code used
 * to keep at most 255 arguments and 8191 command-line characters and silently dropped the
 * rest, so a long link line (ICU's 280-object libicuin link) lost its tail and the libraries
 * on it, failing far from the cause. The OS limit is 32767 characters; this spawns the test
 * itself with many long arguments, below that limit but above both old ones. */
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

#define ARG_COUNT 700
#define ARG_PREFIX "argument-number-"

static void expected(char* out, size_t size, int index) {
  snprintf(out, size, ARG_PREFIX "%04d-xxxxxxxxxxxx", index);  /* 32 characters */
}

static int child(int argc, char** argv) {
  char want[64];
  int i;

  if (argc != ARG_COUNT + 2) {
    fprintf(stderr, "long_argv_test: child got argc=%d, wanted %d\n", argc, ARG_COUNT + 2);
    return 3;
  }
  for (i = 0; i < ARG_COUNT; ++i) {
    expected(want, sizeof(want), i);
    if (strcmp(argv[i + 2], want) != 0) {
      fprintf(stderr, "long_argv_test: child argument %d is \"%s\"\n", i + 2, argv[i + 2]);
      return 4;
    }
  }
  if (argv[argc] != 0) {
    fprintf(stderr, "long_argv_test: child argv is not null-terminated\n");
    return 5;
  }
  return 0;
}

int main(int argc, char** argv) {
  static char storage[ARG_COUNT][64];
  char* child_argv[ARG_COUNT + 3];
  pid_t pid;
  int status = 0;
  int i;

  if (argc > 1 && strcmp(argv[1], "--child") == 0) {
    return child(argc, argv);
  }
  child_argv[0] = argv[0];
  child_argv[1] = (char*)"--child";
  for (i = 0; i < ARG_COUNT; ++i) {
    expected(storage[i], sizeof(storage[i]), i);
    child_argv[i + 2] = storage[i];
  }
  child_argv[ARG_COUNT + 2] = 0;
  if (posix_spawn(&pid, argv[0], 0, 0, child_argv, environ) != 0) {
    fprintf(stderr, "long_argv_test: posix_spawn failed\n");
    return 1;
  }
  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "long_argv_test: the child did not see all %d arguments (status=0x%x)\n", ARG_COUNT, status);
    return 1;
  }
#if defined(_WIN32) || defined(__MINGW32__) || defined(CRT_TARGET_OS_WINDOWS)
  {
    /* A program named without its .exe (ICU's data Makefile runs `../bin/icupkg`, a tool its
     * own build produced as icupkg.exe) is found by the suffix, as on MSYS/Cygwin. */
    size_t length = strlen(argv[0]);
    char bare[1024];
    struct stat sb;

    if (length > 4 && length < sizeof(bare) && strcmp(argv[0] + length - 4, ".exe") == 0) {
      memcpy(bare, argv[0], length - 4);
      bare[length - 4] = 0;
      child_argv[0] = bare;
      /* mksh checks a command with stat() and access(X_OK) before it runs it. */
      if (stat(bare, &sb) != 0 || !S_ISREG(sb.st_mode) || access(bare, X_OK) != 0) {
        fprintf(stderr, "long_argv_test: stat/access of \"%s\" (no .exe) failed\n", bare);
        return 1;
      }
      if (posix_spawn(&pid, bare, 0, 0, child_argv, environ) != 0 ||
          waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "long_argv_test: spawning \"%s\" (no .exe) failed (status=0x%x)\n", bare, status);
        return 1;
      }
    }
  }
#endif
  printf("long_argv_test: ok args=%d chars=%d\n", ARG_COUNT, ARG_COUNT * 33);
  return 0;
}
