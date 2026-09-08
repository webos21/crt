#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "job_control_test: %s\n", message);
  return 1;
}

static int test_tty_errors(void) {
  int fd;
  pid_t pgrp;

  fd = open("job_control_regular.tmp", O_CREAT | O_RDWR | O_TRUNC, 0600);
  if (fd < 0) {
    return fail("open regular");
  }
  errno = 0;
  if (tcgetpgrp(fd) != -1 || errno != ENOTTY) {
    close(fd);
    unlink("job_control_regular.tmp");
    return fail("tcgetpgrp regular");
  }
  pgrp = getpgrp();
  if (pgrp <= 0) {
    close(fd);
    unlink("job_control_regular.tmp");
    return fail("getpgrp");
  }
  errno = 0;
  if (tcsetpgrp(fd, pgrp) != -1 || errno != ENOTTY) {
    close(fd);
    unlink("job_control_regular.tmp");
    return fail("tcsetpgrp regular");
  }
  errno = 0;
  if (tcsetpgrp(fd, 0) != -1 || errno != EINVAL) {
    close(fd);
    unlink("job_control_regular.tmp");
    return fail("tcsetpgrp invalid pgrp");
  }
  close(fd);
  unlink("job_control_regular.tmp");
  return 0;
}

static int test_process_group(void) {
  pid_t self = getpid();
  pid_t pgrp;

  if (self <= 0) {
    return fail("getpid");
  }
  if (setpgid(0, 0) != 0) {
#if defined(CRT_TARGET_OS_WINDOWS)
    return fail("windows setpgid self");
#else
    if (errno != EPERM && errno != EACCES) {
      return fail("setpgid self");
    }
#endif
  }
  pgrp = getpgrp();
  if (pgrp <= 0) {
    return fail("getpgrp after setpgid");
  }
#if defined(CRT_TARGET_OS_WINDOWS)
  if (pgrp != self) {
    return fail("windows pgrp");
  }
  if (setsid() != self || getpgrp() != self) {
    return fail("windows setsid");
  }
#else
  {
    pid_t pid = fork();

    if (pid < 0) {
      return fail("fork");
    }
    if (pid == 0) {
      pid_t sid = setsid();

      _exit(sid > 0 && getpgrp() == sid ? 0 : 77);
    }
    {
      int status = 0;

      if (waitpid(pid, &status, 0) != pid ||
          !WIFEXITED(status) ||
          WEXITSTATUS(status) != 0) {
        return fail("setsid child");
      }
    }
  }
#endif
  return 0;
}

int main(void) {
  if (test_process_group() != 0 ||
      test_tty_errors() != 0) {
    return 1;
  }
  puts("job_control_test: ok");
  return 0;
}
