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
  FILE* renamed;
  char buffer[16];
  size_t nread;
  int ch;

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

  if (setvbuf(stream, buffer, _IOLBF, sizeof(buffer)) != 0) {
    fclose(stream);
    return fail("setvbuf");
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
  nread = fread(buffer, 1, 1, stream);
  if (nread != 0 || !feof(stream) || ferror(stream)) {
    fclose(stream);
    return fail("eof state");
  }
  clearerr(stream);
  if (feof(stream) || ferror(stream)) {
    fclose(stream);
    return fail("clearerr");
  }

  if (fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return fail("fseek reread");
  }
  ch = fgetc(stream);
  if (ch != 'a' || ungetc(ch, stream) != 'a' || getc(stream) != 'a') {
    fclose(stream);
    return fail("getc ungetc");
  }
  fclose(stream);

  if (rename("stdio_file_test.tmp", "stdio_file_test.renamed.tmp") != 0) {
    return fail("rename");
  }
  renamed = fopen("stdio_file_test.renamed.tmp", "r");
  if (renamed == 0) {
    return fail("fopen renamed");
  }
  fclose(renamed);
  if (remove("stdio_file_test.renamed.tmp") != 0) {
    return fail("remove");
  }

  puts("stdio_file_test: ok");
  return 0;
}
