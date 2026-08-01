#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <private/crt_fd_table.h>

static int fail(const char* message) {
  fprintf(stderr, "windows_fd_snapshot_test: %s\n", message);
  return 1;
}

#if defined(CRT_TARGET_OS_WINDOWS)
static int parse_fd_arg(const char* text) {
  int value = 0;

  while (*text >= '0' && *text <= '9') {
    value = value * 10 + (*text - '0');
    ++text;
  }
  return *text == 0 ? value : -1;
}

static void format_fd_arg(int fd, char* buffer, size_t size) {
  char digits[16];
  size_t count = 0;
  size_t pos = 0;

  if (size == 0) {
    return;
  }
  do {
    digits[count++] = (char)('0' + (fd % 10));
    fd /= 10;
  } while (fd != 0 && count < sizeof(digits));
  while (count > 0 && pos + 1 < size) {
    buffer[pos++] = digits[--count];
  }
  buffer[pos] = 0;
}
#endif

int main(int argc, char** argv) {
  struct crt_fd_snapshot snapshot;

#if defined(CRT_TARGET_OS_WINDOWS)
  int pipefd[2];
  char out = 'x';
  char in = 0;
  char encoded[8192];
  struct crt_fd_snapshot decoded;

  if (argc == 3 && strcmp(argv[1], "child") == 0) {
    int child_fd = parse_fd_arg(argv[2]);

    if (child_fd < 0 || write(child_fd, "b", 1) != 1) {
      return 77;
    }
    return 9;
  }
  if (argc == 2 && strcmp(argv[1], "action-child") == 0) {
    if (write(7, "c", 1) != 1) {
      return 78;
    }
    return 10;
  }

  memset(&snapshot, 0, sizeof(snapshot));
  if (pipe(pipefd) != 0) {
    return fail("pipe");
  }
  if (__crt_fd_snapshot_export(&snapshot) != 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("export");
  }
  if (snapshot.magic != CRT_FD_SNAPSHOT_MAGIC ||
      snapshot.version != CRT_FD_SNAPSHOT_VERSION ||
      snapshot.count == 0 ||
      snapshot.capacity != CRT_FD_SNAPSHOT_MAX) {
    __crt_fd_snapshot_dispose(&snapshot);
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("snapshot header");
  }
  if (__crt_fd_snapshot_encode(&snapshot, encoded, sizeof(encoded)) != 0 ||
      __crt_fd_snapshot_decode(encoded, &decoded) != 0 ||
      decoded.magic != CRT_FD_SNAPSHOT_MAGIC ||
      decoded.version != CRT_FD_SNAPSHOT_VERSION ||
      decoded.count != snapshot.count) {
    __crt_fd_snapshot_dispose(&snapshot);
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("snapshot codec");
  }
  close(pipefd[0]);
  close(pipefd[1]);
  if (__crt_fd_snapshot_import(&snapshot) != 0) {
    __crt_fd_snapshot_dispose(&snapshot);
    return fail("import");
  }
  __crt_fd_snapshot_dispose(&snapshot);
  if (write(pipefd[1], &out, 1) != 1 ||
      read(pipefd[0], &in, 1) != 1 ||
      in != out) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("roundtrip fd");
  }
  close(pipefd[0]);
  close(pipefd[1]);
  errno = 0;
  if (fork() != -1 || errno != ENOTSUP) {
    return fail("fork policy");
  }
  if (pipe(pipefd) != 0) {
    return fail("transport pipe");
  }
  {
    pid_t pid;
    int status = 0;
    char fd_arg[16];
    char* child_argv[] = {"/proc/self/exe", "child", fd_arg, 0};

    format_fd_arg(pipefd[1], fd_arg, sizeof(fd_arg));
    if (posix_spawn(&pid, "/proc/self/exe", 0, 0, child_argv, environ) != 0) {
      close(pipefd[0]);
      close(pipefd[1]);
      return fail("transport spawn");
    }
    close(pipefd[1]);
    in = 0;
    if (read(pipefd[0], &in, 1) != 1 || in != 'b') {
      close(pipefd[0]);
      return fail("transport read");
    }
    close(pipefd[0]);
    if (waitpid(pid, &status, 0) != pid ||
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 9) {
      return fail("transport wait");
    }
  }
  if (pipe(pipefd) != 0) {
    return fail("action pipe");
  }
  {
    pid_t pid;
    int status = 0;
    posix_spawn_file_actions_t actions;
    char* child_argv[] = {"/proc/self/exe", "action-child", 0};

    if (posix_spawn_file_actions_init(&actions) != 0 ||
        posix_spawn_file_actions_adddup2(&actions, pipefd[1], 7) != 0) {
      close(pipefd[0]);
      close(pipefd[1]);
      return fail("action setup");
    }
    if (posix_spawn(&pid, "/proc/self/exe", &actions, 0, child_argv, environ) != 0) {
      posix_spawn_file_actions_destroy(&actions);
      close(pipefd[0]);
      close(pipefd[1]);
      return fail("action spawn");
    }
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[1]);
    in = 0;
    if (read(pipefd[0], &in, 1) != 1 || in != 'c') {
      close(pipefd[0]);
      return fail("action read");
    }
    close(pipefd[0]);
    if (waitpid(pid, &status, 0) != pid ||
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 10) {
      return fail("action wait");
    }
  }
#else
  (void)argc;
  (void)argv;
  memset(&snapshot, 0, sizeof(snapshot));
  if (__crt_fd_snapshot_export(&snapshot) != ENOTSUP ||
      snapshot.magic != CRT_FD_SNAPSHOT_MAGIC ||
      snapshot.version != CRT_FD_SNAPSHOT_VERSION ||
      snapshot.capacity != CRT_FD_SNAPSHOT_MAX) {
    return fail("non-windows export policy");
  }
  if (__crt_fd_snapshot_import(&snapshot) != ENOTSUP) {
    return fail("non-windows import policy");
  }
  __crt_fd_snapshot_dispose(&snapshot);
#endif

  puts("windows_fd_snapshot_test: ok");
  return 0;
}
