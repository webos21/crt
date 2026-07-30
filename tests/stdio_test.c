#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fail(const char* message) {
  static const char prefix[] = "stdio_test: ";
  static const char suffix[] = "\n";
  write(2, prefix, sizeof(prefix) - 1);
  write(2, message, strlen(message));
  write(2, suffix, sizeof(suffix) - 1);
  return 1;
}

int main(void) {
  static const char bytes[] = "stdio bytes\n";
  unsigned long start = 0;
  unsigned long end = 0;
  unsigned long offset = 0;
  long inode = 0;
  char perms[10];
  char dev[10];
  char path[32];
  int chars_read = 0;

  if (fputc('s', stdout) != 's') {
    return fail("fputc");
  }
  if (fputc('\n', stdout) == EOF) {
    return fail("fputc newline");
  }
  if (fputs("stdio fputs\n", stdout) == EOF) {
    return fail("fputs");
  }
  if (fwrite(bytes, 1, sizeof(bytes) - 1, stdout) != sizeof(bytes) - 1) {
    return fail("fwrite");
  }
  if (puts("stdio_test: ok") == EOF) {
    return fail("puts");
  }
  if (fflush(stdout) != 0) {
    return fail("fflush");
  }

  errno = 0;
  if (fputc('x', 0) != EOF || errno != EBADF) {
    return fail("bad stream errno");
  }

  if (sscanf("1000-1fff rwxp 40 08:01 123 /tmp/a",
             "%lx-%lx %9s %lx %9s %ld %s%n",
             &start, &end, perms, &offset, dev, &inode, path, &chars_read) != 7) {
    return fail("sscanf");
  }
  if (start != 0x1000UL || end != 0x1fffUL || offset != 0x40UL || inode != 123 ||
      strcmp(perms, "rwxp") != 0 || strcmp(dev, "08:01") != 0 ||
      strcmp(path, "/tmp/a") != 0 || chars_read != 34) {
    return fail("sscanf values");
  }

  return 0;
}
