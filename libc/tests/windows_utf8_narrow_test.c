/* Windows narrow strings are UTF-8 (docs/design/windows_narrow_strings.md): every CRT-linked executable embeds the
 * active-code-page manifest, so argv, the environment and path arguments are UTF-8 and survive a round trip through
 * the file system and a child process. */
#include <crt/platform.h>
#include <dirent.h>
#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

/* U+D55C U+AE00 (Hangul), U+00E9 (e acute, outside most ANSI code pages), U+1F600 (supplementary plane). */
#define NAME "\xed\x95\x9c\xea\xb8\x80-\xc3\xa9-\xf0\x9f\x98\x80"

static int child(int argc, char** argv) {
  const char* value = getenv("CRT_UTF8_ENV");
  if (argc != 3 || strcmp(argv[2], NAME) != 0) return 21;
  if (value == 0 || strcmp(value, NAME) != 0) return 22;
  return 0;
}

int main(int argc, char** argv) {
  const char* directory = "windows-utf8-narrow-dir-" NAME;
  char path[512];
  struct dirent* entry;
  DIR* handle;
  FILE* file;
  int seen = 0, status = 0;
  pid_t pid;
  char* child_argv[4];
  if (argc >= 2 && strcmp(argv[1], "--child") == 0) return child(argc, argv);

  if (strcmp(crt_get_narrow_encoding(), "utf-8") != 0) return 1;
  /* Files: create, list, read back and remove a directory and a file whose names are not ASCII. */
  mkdir(directory, 0755);
  if (mkdir(directory, 0755) == 0 || errno != EEXIST) return 2;
  snprintf(path, sizeof(path), "%s/%s.txt", directory, NAME);
  file = fopen(path, "wb");
  if (file == 0 || fputs("utf8", file) < 0 || fclose(file) != 0) return 3;
  handle = opendir(directory);
  if (handle == 0) return 4;
  while ((entry = readdir(handle)) != 0) {
    if (entry->d_name[0] == '.') continue;
    if (strcmp(entry->d_name, NAME ".txt") != 0) return 5;
    ++seen;
  }
  closedir(handle);
  if (seen != 1) return 6;
  if (access(path, F_OK) != 0) return 7;
  if (unlink(path) != 0 || rmdir(directory) != 0) return 8;
  /* A child process receives the same bytes in argv and in its environment block. */
  if (setenv("CRT_UTF8_ENV", NAME, 1) != 0) return 9;
  child_argv[0] = argv[0];
  child_argv[1] = (char*)"--child";
  child_argv[2] = (char*)NAME;
  child_argv[3] = 0;
  if (posix_spawn(&pid, argv[0], 0, 0, child_argv, environ) != 0) return 10;
  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status)) return 11;
  if (WEXITSTATUS(status) != 0) return 30 + WEXITSTATUS(status);
  puts("windows_utf8_narrow_test: ok");
  return 0;
}
