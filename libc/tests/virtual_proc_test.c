/* Exercises /proc/mounts, /proc/stat, /proc/self/status, /proc/self/cmdline,
 * and /proc/self/environ. On Linux these are all answered by the real
 * kernel procfs (no CRT code involved at all); on Windows and macOS they
 * are answered by the virtual backing in libc/src/fd.c (see that file's own
 * "Virtual /proc files" comment). Assertions are deliberately structural
 * (field counts/shapes, not exact values) wherever real Linux content would
 * otherwise legitimately differ from this PAL's own synthetic content, so
 * the same test binary verifies both the real and the virtual path without
 * needing to know which one it's actually running against. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUF_MAX 8192

static char* g_argv0;

static int fail(const char* message) {
  fprintf(stderr, "virtual_proc_test: %s\n", message);
  return 1;
}

static long read_whole_file(const char* path, char* buf, size_t size) {
  int fd = open(path, O_RDONLY);
  size_t total = 0;
  ssize_t n;

  if (fd < 0) {
    return -1;
  }
  while (total < size) {
    n = read(fd, buf + total, size - total);
    if (n < 0) {
      close(fd);
      return -1;
    }
    if (n == 0) {
      break;
    }
    total += (size_t)n;
  }
  close(fd);
  return (long)total;
}

static int check_proc_mounts(void) {
  char buf[BUF_MAX];
  long len = read_whole_file("/proc/mounts", buf, sizeof(buf));
  char* line;
  char* saveptr;
  int saw_root = 0;

  if (len <= 0) {
    return fail("/proc/mounts empty or unreadable");
  }
  buf[len < (long)sizeof(buf) ? len : (long)sizeof(buf) - 1] = 0;
  line = strtok_r(buf, "\n", &saveptr);
  while (line != 0) {
    char* fields[8];
    int field_count = 0;
    char* tok;
    char* field_saveptr;
    char line_copy[512];

    strncpy(line_copy, line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = 0;
    tok = strtok_r(line_copy, " \t", &field_saveptr);
    while (tok != 0 && field_count < 8) {
      fields[field_count++] = tok;
      tok = strtok_r(0, " \t", &field_saveptr);
    }
    /* device mountpoint fstype options dump pass -- six whitespace-
     * separated fields, real Linux and this PAL's own synthetic line both
     * follow this shape. */
    if (field_count != 6) {
      return fail("/proc/mounts line does not have 6 fields");
    }
    if (strcmp(fields[1], "/") == 0) {
      saw_root = 1;
    }
    line = strtok_r(0, "\n", &saveptr);
  }
  if (!saw_root) {
    return fail("/proc/mounts has no root mount line");
  }
  return 0;
}

static int check_proc_stat(void) {
  char buf[BUF_MAX];
  long len = read_whole_file("/proc/stat", buf, sizeof(buf));

  if (len <= 0) {
    return fail("/proc/stat empty or unreadable");
  }
  buf[len < (long)sizeof(buf) ? len : (long)sizeof(buf) - 1] = 0;
  if (strncmp(buf, "cpu", 3) != 0) {
    return fail("/proc/stat does not start with a cpu line");
  }
  if (strstr(buf, "cpu0") == 0) {
    return fail("/proc/stat has no cpu0 line");
  }
  if (strstr(buf, "btime") == 0) {
    return fail("/proc/stat has no btime line");
  }
  return 0;
}

static int check_proc_self_status(void) {
  char buf[BUF_MAX];
  long len = read_whole_file("/proc/self/status", buf, sizeof(buf));
  char expect[64];

  if (len <= 0) {
    return fail("/proc/self/status empty or unreadable");
  }
  buf[len < (long)sizeof(buf) ? len : (long)sizeof(buf) - 1] = 0;
  if (strstr(buf, "Name:\t") == 0) {
    return fail("/proc/self/status missing Name:");
  }
  snprintf(expect, sizeof(expect), "Pid:\t%d\n", (int)getpid());
  if (strstr(buf, expect) == 0) {
    return fail("/proc/self/status Pid: does not match getpid()");
  }
  if (strstr(buf, "PPid:\t") == 0) {
    return fail("/proc/self/status missing PPid:");
  }
  return 0;
}

static int check_proc_self_cmdline(void) {
  char buf[BUF_MAX];
  long len = read_whole_file("/proc/self/cmdline", buf, sizeof(buf));

  if (len <= 0) {
    return fail("/proc/self/cmdline empty or unreadable");
  }
  buf[len < (long)sizeof(buf) ? len : (long)sizeof(buf) - 1] = 0;
  /* First NUL-terminated token must be exactly this process's own argv[0],
   * the same real argv the OS handed to main(). */
  if (strcmp(buf, g_argv0) != 0) {
    return fail("/proc/self/cmdline first token != argv[0]");
  }
  return 0;
}

static int check_proc_self_environ(void) {
  char buf[BUF_MAX];
  long len = read_whole_file("/proc/self/environ", buf, sizeof(buf));
  const char* needle = "CRT_VIRTUAL_PROC_TEST_VAR=hello123";
  long offset = 0;
  int found = 0;

  if (len <= 0) {
    return fail("/proc/self/environ empty or unreadable");
  }
  /* Walk NUL-separated entries rather than a plain strstr(): a substring
   * match could accidentally hit inside some other unrelated entry's
   * value. */
  while (offset < len) {
    size_t entry_len = strlen(buf + offset);

    if (strcmp(buf + offset, needle) == 0) {
      found = 1;
    }
    offset += (long)entry_len + 1;
  }
  if (!found) {
    return fail("/proc/self/environ missing the ctest-injected var");
  }
  return 0;
}

int main(int argc, char** argv) {
  (void)argc;
  g_argv0 = argv[0];

  if (check_proc_mounts() || check_proc_stat() || check_proc_self_status() ||
      check_proc_self_cmdline() || check_proc_self_environ()) {
    return 1;
  }

  printf("virtual_proc_test: ok\n");
  return 0;
}
