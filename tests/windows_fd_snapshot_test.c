#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <private/crt_fd_table.h>

static int fail(const char* message) {
  fprintf(stderr, "windows_fd_snapshot_test: %s\n", message);
  return 1;
}

int main(void) {
  struct crt_fd_snapshot snapshot;

#if defined(CRT_TARGET_OS_WINDOWS)
  int pipefd[2];
  char out = 'x';
  char in = 0;

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
#else
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
