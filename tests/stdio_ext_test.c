#include <errno.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <string.h>

static int fail(const char* message) {
  fprintf(stderr, "stdio_ext_test: %s\n", message);
  return 1;
}

int main(void) {
  char buffer[32];
  char readback[8];
  FILE* stream;
  size_t pending;

  stream = tmpfile();
  if (stream == 0) {
    return fail("tmpfile");
  }
  if (!__freadable(stream) || !__fwritable(stream) ||
      __freading(stream) || __fwriting(stream)) {
    fclose(stream);
    return fail("initial state");
  }
  if (setvbuf(stream, buffer, _IOFBF, sizeof(buffer)) != 0 ||
      __fbufsize(stream) != sizeof(buffer)) {
    fclose(stream);
    return fail("fbufsize");
  }
  if (fputs("abcdef", stream) == EOF ||
      !__fwriting(stream) ||
      __freading(stream)) {
    fclose(stream);
    return fail("write state");
  }
  pending = __fpending(stream);
  if (pending != 6) {
    fclose(stream);
    return fail("fpending");
  }
  __fpurge(stream);
  if (__fpending(stream) != 0 ||
      fseek(stream, 0, SEEK_SET) != 0 ||
      fread(readback, 1, sizeof(readback), stream) != 0 ||
      !feof(stream)) {
    fclose(stream);
    return fail("fpurge write");
  }
  clearerr(stream);
  if (fseek(stream, 0, SEEK_SET) != 0 ||
      fputs("abcdef", stream) == EOF ||
      fflush(stream) != 0 ||
      fseek(stream, 0, SEEK_SET) != 0 ||
      fgetc(stream) != 'a' ||
      __freadahead(stream) < 5) {
    fclose(stream);
    return fail("freadahead");
  }
  if (ungetc('Z', stream) != 'Z' ||
      __freadahead(stream) < 6) {
    fclose(stream);
    return fail("freadahead ungetc");
  }
  __fpurge(stream);
  if (__freadahead(stream) != 0 ||
      fgetc(stream) != EOF ||
      !feof(stream)) {
    fclose(stream);
    return fail("fpurge read");
  }
  __fseterr(stream);
  if (!ferror(stream)) {
    fclose(stream);
    return fail("fseterr");
  }
  clearerr(stream);
  if (ferror(stream)) {
    fclose(stream);
    return fail("clearerr");
  }
  fclose(stream);

  stream = tmpfile();
  if (stream == 0) {
    return fail("tmpfile line");
  }
  if (setvbuf(stream, buffer, _IOLBF, sizeof(buffer)) != 0 ||
      !__flbf(stream) ||
      fputs("line", stream) == EOF ||
      __fpending(stream) != 4) {
    fclose(stream);
    return fail("line pending");
  }
  _flushlbf();
  if (__fpending(stream) != 0) {
    fclose(stream);
    return fail("flushlbf");
  }
  fclose(stream);

  printf("stdio_ext_test: ok\n");
  return 0;
}
