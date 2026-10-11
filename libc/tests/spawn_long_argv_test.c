/* A child gets an argument vector far longer than Windows' 32,767-character command line (a
 * Linux program may pass up to ARG_MAX): 4000 arguments of 40 characters, 160,000 bytes. The
 * parent spawns itself; the child checks the count, every argument and an embedded space, then
 * spawns a grandchild with a few of them to show the vector is not sticky. */

#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

#define COUNT 4000
#define WIDTH 40

static void make_argument(int index, char* out, size_t size) {
  int i;

  /* "argNNNN-" plus a position-dependent tail; every fifth one contains a space. */
  for (i = 0; i < WIDTH && (size_t)i + 1 < size; ++i) {
    out[i] = (char)('a' + ((index * 7 + i * 3) % 26));
  }
  out[0] = 'a';
  out[1] = 'r';
  out[2] = 'g';
  if (index % 5 == 0) {
    out[10] = ' ';
  }
  out[WIDTH] = 0;
}

static int run_child(int argc, char** argv) {
  char expected[WIDTH + 1];
  int i;

  if (argc != COUNT + 2) {
    fprintf(stderr, "child: argc=%d expected %d\n", argc, COUNT + 2);
    return 1;
  }
  for (i = 0; i < COUNT; ++i) {
    make_argument(i, expected, sizeof(expected));
    if (strcmp(argv[i + 2], expected) != 0) {
      fprintf(stderr, "child: argument %d differs: [%s] vs [%s]\n", i, argv[i + 2], expected);
      return 1;
    }
  }
  if (getenv("CRT_ARGV_FILE") != NULL) {
    fprintf(stderr, "child: CRT_ARGV_FILE leaked into the environment\n");
    return 1;
  }
  {
    /* A short vector again: the spill must not stick to a grandchild. */
    char* const args[] = {argv[0], "grandchild", "x", NULL};
    pid_t pid;
    int status = 0;

    if (posix_spawn(&pid, argv[0], NULL, NULL, args, environ) != 0) {
      fprintf(stderr, "child: grandchild spawn failed: %d\n", errno);
      return 1;
    }
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      fprintf(stderr, "child: grandchild status %d\n", status);
      return 1;
    }
  }
  printf("spawn_long_argv_test: child ok (%d arguments)\n", argc);
  return 0;
}

int main(int argc, char** argv) {
  char** args;
  char* storage;
  pid_t pid;
  int status = 0;
  int i;

  if (argc >= 2 && strcmp(argv[1], "grandchild") == 0) {
    return argc == 3 && strcmp(argv[2], "x") == 0 ? 0 : 1;
  }
  if (argc >= 2 && strcmp(argv[1], "child") == 0) {
    return run_child(argc, argv);
  }

  args = (char**)calloc((size_t)COUNT + 3, sizeof(char*));
  storage = (char*)malloc((size_t)COUNT * (WIDTH + 1));
  if (args == NULL || storage == NULL) {
    return 2;
  }
  args[0] = argv[0];
  args[1] = "child";
  for (i = 0; i < COUNT; ++i) {
    char* slot = storage + (size_t)i * (WIDTH + 1);

    make_argument(i, slot, WIDTH + 1);
    args[i + 2] = slot;
  }
  args[COUNT + 2] = NULL;
  if (posix_spawn(&pid, argv[0], NULL, NULL, args, environ) != 0) {
    perror("posix_spawn");
    return 3;
  }
  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "spawn_long_argv_test: child failed (status %d)\n", status);
    return 4;
  }
  printf("spawn_long_argv_test: ok\n");
  return 0;
}
