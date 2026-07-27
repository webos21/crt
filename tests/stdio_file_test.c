#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fail(const char* message) {
  static const char prefix[] = "stdio_file_test: ";
  static const char suffix[] = "\n";
  write(2, prefix, sizeof(prefix) - 1);
  write(2, message, strlen(message));
  write(2, suffix, sizeof(suffix) - 1);
  return 1;
}

int main(void) {
  FILE* stream;
  char buffer[16];
  size_t nread;

  stream = fopen("stdio_file_test.tmp", "w+");
  if (stream == 0) {
    return fail("fopen w+");
  }

  if (fputs("abcdef", stream) == EOF) {
    fclose(stream);
    return fail("fputs");
  }
  if (ftell(stream) != 6) {
    fclose(stream);
    return fail("ftell after write");
  }
  if (fseek(stream, 1, SEEK_SET) != 0) {
    fclose(stream);
    return fail("fseek set");
  }

  memset(buffer, 0, sizeof(buffer));
  nread = fread(buffer, 1, 3, stream);
  if (nread != 3 || buffer[0] != 'b' || buffer[1] != 'c' || buffer[2] != 'd') {
    fclose(stream);
    return fail("fread");
  }

  if (fclose(stream) != 0) {
    return fail("fclose");
  }

  stream = fopen("stdio_file_test.tmp", "r");
  if (stream == 0) {
    return fail("fopen r");
  }
  memset(buffer, 0, sizeof(buffer));
  nread = fread(buffer, 1, 6, stream);
  if (nread != 6 || strcmp(buffer, "abcdef") != 0) {
    fclose(stream);
    return fail("reread");
  }
  fclose(stream);

  puts("stdio_file_test: ok");
  return 0;
}
