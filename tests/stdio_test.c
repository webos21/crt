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

  return 0;
}
