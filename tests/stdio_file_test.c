#include <stdio.h>
#include <string.h>
#include <fcntl.h>
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
  FILE* reopened;
  FILE* renamed;
  FILE* observer;
  FILE* temp;
  char buffer[16];
  char small_buffer[4];
  size_t nread;
  int ch;
  int fd;

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

  stream = fopen("stdio_extra_test.tmp", "w+");
  if (stream == 0) {
    return fail("fopen extra");
  }
  if (fileno(stream) < 0) {
    fclose(stream);
    return fail("fileno");
  }
  if (fputs("line1\nline2\n", stream) == EOF ||
      fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return fail("write extra");
  }
  memset(buffer, 0, sizeof(buffer));
  if (fgets(buffer, sizeof(buffer), stream) != buffer ||
      strcmp(buffer, "line1\n") != 0) {
    fclose(stream);
    return fail("fgets");
  }
  reopened = freopen("stdio_freopen_test.tmp", "w+", stream);
  if (reopened != stream ||
      fputs("again", reopened) == EOF ||
      fseek(reopened, 0, SEEK_SET) != 0) {
    fclose(stream);
    return fail("freopen");
  }
  memset(buffer, 0, sizeof(buffer));
  if (fread(buffer, 1, 5, reopened) != 5 || strcmp(buffer, "again") != 0) {
    fclose(stream);
    return fail("freopen read");
  }
  fclose(stream);
  if (remove("stdio_extra_test.tmp") != 0 ||
      remove("stdio_freopen_test.tmp") != 0) {
    return fail("remove extra");
  }

  fd = open("stdio_fdopen_test.tmp", O_CREAT | O_RDWR | O_TRUNC, 0666);
  if (fd < 0) {
    return fail("open fdopen");
  }
  stream = fdopen(fd, "w+");
  if (stream == 0 ||
      fputs("fdopen", stream) == EOF ||
      fseek(stream, 0, SEEK_SET) != 0) {
    if (stream != 0) {
      fclose(stream);
    } else {
      close(fd);
    }
    return fail("fdopen");
  }
  memset(buffer, 0, sizeof(buffer));
  if (fread(buffer, 1, 6, stream) != 6 || strcmp(buffer, "fdopen") != 0) {
    fclose(stream);
    return fail("fdopen read");
  }
  fclose(stream);
  if (remove("stdio_fdopen_test.tmp") != 0) {
    return fail("remove fdopen");
  }

  temp = tmpfile();
  if (temp == 0 ||
      fputs("tmp", temp) == EOF ||
      fseek(temp, 0, SEEK_SET) != 0) {
    if (temp != 0) {
      fclose(temp);
    }
    return fail("tmpfile");
  }
  memset(buffer, 0, sizeof(buffer));
  if (fread(buffer, 1, 3, temp) != 3 || strcmp(buffer, "tmp") != 0) {
    fclose(temp);
    return fail("tmpfile read");
  }
  fclose(temp);

  stream = fopen("stdio_buffer_test.tmp", "w+");
  if (stream == 0) {
    return fail("fopen buffer");
  }
  if (setvbuf(stream, small_buffer, _IOFBF, sizeof(small_buffer)) != 0) {
    fclose(stream);
    return fail("setvbuf full");
  }
  if (fputc('A', stream) != 'A' || ftell(stream) != 1) {
    fclose(stream);
    return fail("buffered fputc ftell");
  }
  observer = fopen("stdio_buffer_test.tmp", "r");
  if (observer == 0) {
    fclose(stream);
    return fail("fopen observer");
  }
  nread = fread(buffer, 1, 1, observer);
  if (nread != 0) {
    fclose(observer);
    fclose(stream);
    return fail("full buffering flushed early");
  }
  fclose(observer);
  if (fflush(stream) != 0) {
    fclose(stream);
    return fail("fflush buffered");
  }
  if (fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return fail("fseek buffered");
  }
  memset(buffer, 0, sizeof(buffer));
  nread = fread(buffer, 1, 1, stream);
  if (nread != 1 || buffer[0] != 'A') {
    fclose(stream);
    return fail("read buffered write");
  }
  fclose(stream);
  if (remove("stdio_buffer_test.tmp") != 0) {
    return fail("remove buffer");
  }

  stream = fopen("stdio_line_buffer_test.tmp", "w+");
  if (stream == 0) {
    return fail("fopen line buffer");
  }
  if (setvbuf(stream, small_buffer, _IOLBF, sizeof(small_buffer)) != 0) {
    fclose(stream);
    return fail("setvbuf line");
  }
  if (fputs("L\n", stream) == EOF) {
    fclose(stream);
    return fail("line buffered fputs");
  }
  observer = fopen("stdio_line_buffer_test.tmp", "r");
  if (observer == 0) {
    fclose(stream);
    return fail("fopen line observer");
  }
  memset(buffer, 0, sizeof(buffer));
  nread = fread(buffer, 1, 2, observer);
  if (nread != 2 || buffer[0] != 'L' || buffer[1] != '\n') {
    fclose(observer);
    fclose(stream);
    return fail("line buffering did not flush");
  }
  fclose(observer);
  fclose(stream);
  if (remove("stdio_line_buffer_test.tmp") != 0) {
    return fail("remove line buffer");
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
