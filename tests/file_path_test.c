#include <fcntl.h>
#include <stdio.h>
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
  int fd;
  int copy;
  struct stat st;

  if (getcwd(cwd, sizeof(cwd)) == 0 || cwd[0] == 0) {
    return fail("getcwd");
  }
  if (mkdir("file_path_test.dir", 0777) != 0) {
    return fail("mkdir");
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
  if (access("sample.tmp", F_OK | R_OK) != 0) {
    close(fd);
    return fail("access sample");
  }
  if (stat("sample.tmp", &st) != 0 || !S_ISREG(st.st_mode) || st.st_size != 1) {
    close(fd);
    return fail("stat sample");
  }
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
