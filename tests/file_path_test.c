#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "file_path_test: %s\n", message);
  return 1;
}

int main(void) {
  char cwd[4096];
  char byte = 'Z';
  char readback = 0;
  char linkbuf[64];
  char resolved[PATH_MAX];
  char* allocated_path;
  int fd;
  int copy;
  int high_copy;
  int pipefd[2];
  struct stat st;

  if (getcwd(cwd, sizeof(cwd)) == 0 || cwd[0] == 0) {
    return fail("getcwd");
  }
  if (mkdir("file_path_test.dir", 0777) != 0) {
    return fail("mkdir");
  }
  if (stat("file_path_test.dir", &st) != 0 || !S_ISDIR(st.st_mode)) {
    return fail("stat dir");
  }
  if (chdir("file_path_test.dir") != 0) {
    return fail("chdir into");
  }
  fd = open("sample.tmp", O_CREAT | O_RDWR | O_TRUNC, 0666);
  if (fd < 0) {
    return fail("open sample");
  }
  if (write(fd, &byte, 1) != 1) {
    close(fd);
    return fail("write sample");
  }
  if (access("sample.tmp", F_OK | R_OK | W_OK) != 0) {
    close(fd);
    return fail("access sample");
  }
  if (access("missing.tmp", F_OK) == 0) {
    close(fd);
    return fail("access missing");
  }
  if (stat("sample.tmp", &st) != 0 || !S_ISREG(st.st_mode) || st.st_size != 1) {
    close(fd);
    return fail("stat sample");
  }
  memset(&st, 0, sizeof(st));
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size != 1 || st.st_nlink < 1) {
    close(fd);
    return fail("fstat sample");
  }
  if (isatty(fd) != 0) {
    close(fd);
    return fail("isatty regular");
  }
  if (fcntl(fd, F_GETFD) != 0 ||
      fcntl(fd, F_SETFD, FD_CLOEXEC) != 0 ||
      fcntl(fd, F_GETFL) < 0) {
    close(fd);
    return fail("fcntl flags");
  }
  memset(&st, 0, sizeof(st));
  if (lstat("sample.tmp", &st) != 0 || !S_ISREG(st.st_mode) || st.st_size != 1) {
    close(fd);
    return fail("lstat sample");
  }
  if (realpath("./sample.tmp", resolved) == 0 || strstr(resolved, "sample.tmp") == 0) {
    close(fd);
    return fail("realpath buffer");
  }
  allocated_path = realpath("sample.tmp", 0);
  if (allocated_path == 0 || strstr(allocated_path, "sample.tmp") == 0) {
    free(allocated_path);
    close(fd);
    return fail("realpath alloc");
  }
  free(allocated_path);
#if defined(CRT_TARGET_OS_WINDOWS)
  errno = 0;
  if (symlink("sample.tmp", "sample.link") == 0 || errno != ENOSYS) {
    close(fd);
    return fail("windows symlink policy");
  }
  errno = 0;
  if (readlink("sample.link", linkbuf, sizeof(linkbuf)) >= 0 || errno != ENOSYS) {
    close(fd);
    return fail("windows readlink policy");
  }
#else
  if (symlink("sample.tmp", "sample.link") != 0) {
    close(fd);
    return fail("symlink");
  }
  memset(linkbuf, 0, sizeof(linkbuf));
  if (readlink("sample.link", linkbuf, sizeof(linkbuf) - 1) != 10 ||
      strcmp(linkbuf, "sample.tmp") != 0) {
    close(fd);
    return fail("readlink");
  }
  memset(&st, 0, sizeof(st));
  if (lstat("sample.link", &st) != 0 || !S_ISLNK(st.st_mode)) {
    close(fd);
    return fail("lstat symlink");
  }
  if (remove("sample.link") != 0) {
    close(fd);
    return fail("remove symlink");
  }
#endif
  if (lseek(fd, 0, SEEK_SET) != 0) {
    close(fd);
    return fail("lseek sample");
  }
  copy = dup(fd);
  if (copy < 0) {
    close(fd);
    return fail("dup");
  }
  if (read(copy, &readback, 1) != 1 || readback != 'Z') {
    close(copy);
    close(fd);
    return fail("read dup");
  }
  close(copy);
  if (dup2(fd, 10) != 10) {
    close(fd);
    return fail("dup2");
  }
  close(10);
  high_copy = fcntl(fd, F_DUPFD, 8);
  if (high_copy < 8) {
    close(fd);
    return fail("fcntl dupfd");
  }
  close(high_copy);
  if (pipe(pipefd) != 0) {
    close(fd);
    return fail("pipe");
  }
  byte = 'P';
  readback = 0;
  if (write(pipefd[1], &byte, 1) != 1 ||
      read(pipefd[0], &readback, 1) != 1 ||
      readback != 'P') {
    close(pipefd[0]);
    close(pipefd[1]);
    close(fd);
    return fail("pipe read/write");
  }
  close(pipefd[0]);
  close(pipefd[1]);
  close(fd);
  if (remove("sample.tmp") != 0) {
    return fail("remove sample");
  }
  if (chdir(cwd) != 0) {
    return fail("chdir back");
  }
  if (rmdir("file_path_test.dir") != 0) {
    return fail("rmdir");
  }

  printf("file_path_test: ok\n");
  return 0;
}
