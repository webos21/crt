#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fail(const char* message) {
  static const char prefix[] = "stdio_edge_test: ";
  static const char suffix[] = "\n";

  write(2, prefix, sizeof(prefix) - 1);
  write(2, message, strlen(message));
  write(2, suffix, sizeof(suffix) - 1);
  return 1;
}

static int test_regular_pushback_position(void) {
  FILE* stream = fopen("stdio_edge_regular.tmp", "w+");

  if (stream == 0) {
    return fail("regular open");
  }
  if (fputs("abcdef", stream) == EOF ||
      fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return fail("regular prepare");
  }
  if (fgetc(stream) != 'a' ||
      fgetc(stream) != 'b' ||
      fgetc(stream) != 'c' ||
      fgetc(stream) != 'd') {
    fclose(stream);
    return fail("regular read");
  }
  if (ungetc('D', stream) != 'D' ||
      ungetc('C', stream) != 'C') {
    fclose(stream);
    return fail("regular multi ungetc");
  }
  if (ftell(stream) != 2 || ftello(stream) != 2) {
    fclose(stream);
    return fail("regular ftell pushback");
  }
  if (fseek(stream, 0, SEEK_CUR) != 0) {
    fclose(stream);
    return fail("regular seek cur pushback");
  }
  if (fgetc(stream) != 'c' || ftell(stream) != 3) {
    fclose(stream);
    return fail("regular seek discarded pushback");
  }
  fclose(stream);
  remove("stdio_edge_regular.tmp");
  return 0;
}

static int test_memory_pushback_position(void) {
  char data[] = "abcdef";
  FILE* stream = fmemopen(data, sizeof(data) - 1, "r");

  if (stream == 0) {
    return fail("memory open");
  }
  if (fgetc(stream) != 'a' ||
      fgetc(stream) != 'b' ||
      fgetc(stream) != 'c' ||
      fgetc(stream) != 'd') {
    fclose(stream);
    return fail("memory read");
  }
  if (ungetc('D', stream) != 'D' ||
      ungetc('C', stream) != 'C') {
    fclose(stream);
    return fail("memory multi ungetc");
  }
  if (ftell(stream) != 2) {
    fclose(stream);
    return fail("memory ftell pushback");
  }
  if (fgetc(stream) != 'C' || ftell(stream) != 3 ||
      fgetc(stream) != 'D' || ftell(stream) != 4) {
    fclose(stream);
    return fail("memory pop pushback");
  }
  fclose(stream);
  return 0;
}

static int test_eof_error_clear(void) {
  char data[] = "x";
  FILE* stream = fmemopen(data, sizeof(data) - 1, "r");

  if (stream == 0) {
    return fail("eof open");
  }
  if (fgetc(stream) != 'x' || fgetc(stream) != EOF || !feof(stream) || ferror(stream)) {
    fclose(stream);
    return fail("eof state");
  }
  if (ungetc('y', stream) != 'y' || feof(stream)) {
    fclose(stream);
    return fail("ungetc clears eof");
  }
  if (fgetc(stream) != 'y' || fgetc(stream) != EOF || !feof(stream)) {
    fclose(stream);
    return fail("eof after pushback");
  }
  clearerr(stream);
  if (feof(stream) || ferror(stream)) {
    fclose(stream);
    return fail("clearerr eof");
  }
  fclose(stream);

  stream = fopen("stdio_edge_writeonly.tmp", "w");
  if (stream == 0) {
    return fail("writeonly open");
  }
  errno = 0;
  if (fgetc(stream) != EOF || errno != EBADF || !ferror(stream)) {
    fclose(stream);
    return fail("writeonly read error");
  }
  clearerr(stream);
  if (ferror(stream) || feof(stream)) {
    fclose(stream);
    return fail("clearerr error");
  }
  errno = 0;
  if (ungetc('z', stream) != EOF || errno != EBADF || !ferror(stream)) {
    fclose(stream);
    return fail("writeonly ungetc");
  }
  fclose(stream);
  remove("stdio_edge_writeonly.tmp");
  return 0;
}

static int test_buffering_edges(void) {
  char buffer[4];
  char output[8];
  FILE* stream = fopen("stdio_edge_buffer.tmp", "w+");
  FILE* observer;

  if (stream == 0) {
    return fail("buffer open");
  }
  errno = 0;
  if (setvbuf(stream, 0, 123, 0) != EOF || errno != EINVAL) {
    fclose(stream);
    return fail("setvbuf invalid mode");
  }
  if (setvbuf(stream, buffer, _IOFBF, sizeof(buffer)) != 0 ||
      fputs("abc", stream) == EOF) {
    fclose(stream);
    return fail("buffer prepare");
  }
  observer = fopen("stdio_edge_buffer.tmp", "r");
  if (observer == 0) {
    fclose(stream);
    return fail("buffer observer");
  }
  memset(output, 0, sizeof(output));
  if (fread(output, 1, 3, observer) != 0) {
    fclose(observer);
    fclose(stream);
    return fail("buffer visible before flush");
  }
  fclose(observer);
  if (fflush(stream) != 0 ||
      fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return fail("buffer flush seek");
  }
  memset(output, 0, sizeof(output));
  if (fread(output, 2, 2, stream) != 1 ||
      output[0] != 'a' || output[1] != 'b' || output[2] != 'c' ||
      !feof(stream)) {
    fclose(stream);
    return fail("partial fread element");
  }
  fclose(stream);
  remove("stdio_edge_buffer.tmp");
  return 0;
}

int main(void) {
  if (test_regular_pushback_position() != 0 ||
      test_memory_pushback_position() != 0 ||
      test_eof_error_clear() != 0 ||
      test_buffering_edges() != 0) {
    return 1;
  }
  puts("stdio_edge_test: ok");
  return 0;
}
