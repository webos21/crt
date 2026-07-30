#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

struct cookie_buffer {
  char data[64];
  size_t pos;
  size_t len;
  int closed;
};

static int fail(const char* message) {
  puts(message);
  return 1;
}

static int make_vasprintf(char** text, const char* format, ...) {
  int result;
  va_list ap;

  va_start(ap, format);
  result = vasprintf(text, format, ap);
  va_end(ap);
  return result;
}

static int cookie_read(void* opaque, char* buf, int count) {
  struct cookie_buffer* cookie = (struct cookie_buffer*)opaque;
  int available;

  if (cookie->pos >= cookie->len) {
    return 0;
  }
  available = (int)(cookie->len - cookie->pos);
  if (available > count) {
    available = count;
  }
  memcpy(buf, cookie->data + cookie->pos, (size_t)available);
  cookie->pos += (size_t)available;
  return available;
}

static int cookie_write(void* opaque, const char* buf, int count) {
  struct cookie_buffer* cookie = (struct cookie_buffer*)opaque;
  size_t available = sizeof(cookie->data) - cookie->pos;

  if ((size_t)count > available) {
    count = (int)available;
  }
  memcpy(cookie->data + cookie->pos, buf, (size_t)count);
  cookie->pos += (size_t)count;
  if (cookie->pos > cookie->len) {
    cookie->len = cookie->pos;
  }
  return count;
}

static fpos_t cookie_seek(void* opaque, fpos_t offset, int whence) {
  struct cookie_buffer* cookie = (struct cookie_buffer*)opaque;
  fpos_t base;
  fpos_t target;

  if (whence == SEEK_SET) {
    base = 0;
  } else if (whence == SEEK_CUR) {
    base = (fpos_t)cookie->pos;
  } else if (whence == SEEK_END) {
    base = (fpos_t)cookie->len;
  } else {
    return (fpos_t)-1;
  }
  target = base + offset;
  if (target < 0 || (size_t)target > sizeof(cookie->data)) {
    return (fpos_t)-1;
  }
  cookie->pos = (size_t)target;
  return target;
}

static int cookie_close(void* opaque) {
  struct cookie_buffer* cookie = (struct cookie_buffer*)opaque;

  cookie->closed = 1;
  return 0;
}

int main(void) {
  struct cookie_buffer cookie;
  char* text = 0;
  char fixed[16];
  char raw[6];
  FILE* stream;
  char* line;
  wchar_t* wtext = 0;
  size_t line_length;
  size_t size = 0;
  size_t wsize = 0;

  if (asprintf(&text, "value:%d:%s", 42, "ok") != 11) {
    return fail("stdio_memory_test: asprintf length");
  }
  if (strcmp(text, "value:42:ok") != 0) {
    return fail("stdio_memory_test: asprintf content");
  }
  free(text);
  text = 0;
  if (make_vasprintf(&text, "%s:%04d", "hex", 26) != 8 ||
      strcmp(text, "hex:0026") != 0) {
    free(text);
    return fail("stdio_memory_test: vasprintf");
  }
  free(text);
  text = 0;

  memcpy(raw, "ab\0cd", sizeof(raw));
  stream = fmemopen(raw, sizeof(raw), "r");
  if (stream == 0) {
    return fail("stdio_memory_test: fmemopen raw open");
  }
  memset(fixed, 0, sizeof(fixed));
  if (fread(fixed, 1, sizeof(raw), stream) != sizeof(raw) ||
      memcmp(fixed, raw, sizeof(raw)) != 0) {
    fclose(stream);
    return fail("stdio_memory_test: fmemopen read capacity");
  }
  fclose(stream);

  memcpy(raw, "abcde", sizeof(raw));
  stream = fmemopen(raw, sizeof(raw), "r+");
  if (stream == 0 ||
      fseek(stream, (long)sizeof(raw) - 1, SEEK_SET) != 0 ||
      fputc('\0', stream) == EOF) {
    if (stream != 0) {
      fclose(stream);
    }
    return fail("stdio_memory_test: fmemopen final nul write");
  }
  fclose(stream);
  if (memcmp(raw, "abcde", 5) != 0 || raw[5] != '\0') {
    return fail("stdio_memory_test: fmemopen final nul content");
  }

  memcpy(raw, "abcde", sizeof(raw));
  stream = fmemopen(raw, sizeof(raw), "r+");
  if (stream == 0 ||
      fseek(stream, (long)sizeof(raw) - 1, SEEK_SET) != 0) {
    if (stream != 0) {
      fclose(stream);
    }
    return fail("stdio_memory_test: fmemopen overflow seek");
  }
  errno = 0;
  if (fputc('Z', stream) != EOF || errno != ENOSPC || !ferror(stream)) {
    fclose(stream);
    return fail("stdio_memory_test: fmemopen overflow");
  }
  fclose(stream);

  memcpy(fixed, "abcdef", 7);
  stream = fmemopen(fixed, sizeof(fixed), "r+");
  if (stream == 0) {
    return fail("stdio_memory_test: fmemopen read");
  }
  if (fgetc_unlocked(stream) != 'a' || fgetc_unlocked(stream) != 'b') {
    return fail("stdio_memory_test: fgetc_unlocked");
  }
  if (ungetc('B', stream) != 'B' ||
      ungetc('A', stream) != 'A' ||
      ungetc('0', stream) != '0' ||
      ungetc('!', stream) != '!' ||
      fgetc(stream) != '!' ||
      fgetc(stream) != '0' ||
      fgetc(stream) != 'A' ||
      fgetc(stream) != 'B') {
    return fail("stdio_memory_test: ungetc");
  }
  if (fseek(stream, 3, SEEK_SET) != 0 || fputs_unlocked("XY", stream) == EOF) {
    return fail("stdio_memory_test: fmemopen write");
  }
  if (fflush_unlocked(stream) != 0) {
    return fail("stdio_memory_test: fmemopen flush");
  }
  if (strcmp(fixed, "abcXYf") != 0) {
    return fail("stdio_memory_test: fmemopen content");
  }
  fclose(stream);

  stream = open_memstream(&text, &size);
  if (stream == 0) {
    return fail("stdio_memory_test: open_memstream");
  }
  if (__fsetlocking(stream, FSETLOCKING_QUERY) != FSETLOCKING_INTERNAL) {
    return fail("stdio_memory_test: fsetlocking query");
  }
  flockfile(stream);
  if (ftrylockfile(stream) != 0) {
    return fail("stdio_memory_test: ftrylockfile recursive");
  }
  if (fwrite_unlocked("hello", 1, 5, stream) != 5 ||
      putc_unlocked(' ', stream) == EOF ||
      fputs_unlocked("world", stream) == EOF) {
    return fail("stdio_memory_test: memstream write");
  }
  funlockfile(stream);
  funlockfile(stream);
  if (fflush(stream) != 0) {
    return fail("stdio_memory_test: memstream flush");
  }
  if (size != 11 || strcmp(text, "hello world") != 0) {
    return fail("stdio_memory_test: memstream content");
  }
  clearerr_unlocked(stream);
  if (feof_unlocked(stream) || ferror_unlocked(stream)) {
    return fail("stdio_memory_test: unlocked state");
  }
  if (fclose(stream) != 0) {
    return fail("stdio_memory_test: memstream close");
  }
  if (size != 11 || strcmp(text, "hello world") != 0) {
    return fail("stdio_memory_test: memstream close sync");
  }
  free(text);
  text = 0;
  size = 99;

  stream = open_memstream(&text, &size);
  if (stream == 0) {
    return fail("stdio_memory_test: open_memstream seek open");
  }
  if (fputs("ab", stream) == EOF ||
      fseek(stream, 5, SEEK_SET) != 0 ||
      fputc('Z', stream) == EOF ||
      fflush(stream) != 0) {
    fclose(stream);
    free(text);
    return fail("stdio_memory_test: open_memstream seek write");
  }
  if (size != 6 ||
      text[0] != 'a' || text[1] != 'b' ||
      text[2] != '\0' || text[3] != '\0' || text[4] != '\0' ||
      text[5] != 'Z' || text[6] != '\0') {
    fclose(stream);
    free(text);
    return fail("stdio_memory_test: open_memstream seek content");
  }
  fclose(stream);
  free(text);

  stream = open_wmemstream(&wtext, &wsize);
  if (stream == 0) {
    return fail("stdio_memory_test: open_wmemstream open");
  }
  if (fputwc(L'A', stream) == WEOF ||
      fseek(stream, 3, SEEK_SET) != 0 ||
      fputwc(L'Z', stream) == WEOF ||
      fflush(stream) != 0) {
    fclose(stream);
    free(wtext);
    return fail("stdio_memory_test: open_wmemstream seek");
  }
  if (wsize != 4 || wtext[0] != L'A' || wtext[1] != 0 ||
      wtext[2] != 0 || wtext[3] != L'Z' || wtext[4] != 0) {
    fclose(stream);
    free(wtext);
    return fail("stdio_memory_test: open_wmemstream content");
  }
  fclose(stream);
  free(wtext);

  cookie.pos = 0;
  cookie.len = 0;
  cookie.closed = 0;
  stream = funopen(&cookie, cookie_read, cookie_write, cookie_seek, cookie_close);
  if (stream == 0) {
    return fail("stdio_memory_test: funopen");
  }
  if (__fsetlocking(stream, FSETLOCKING_BYCALLER) != FSETLOCKING_INTERNAL ||
      __fsetlocking(stream, FSETLOCKING_QUERY) != FSETLOCKING_BYCALLER) {
    fclose(stream);
    return fail("stdio_memory_test: fsetlocking bycaller");
  }
  flockfile(stream);
  if (setlinebuf(stream) != 0 ||
      fputs("one\n", stream) == EOF ||
      fwrite("two\nthree", 1, 9, stream) != 9 ||
      fseek(stream, 0, SEEK_SET) != 0) {
    funlockfile(stream);
    fclose(stream);
    return fail("stdio_memory_test: funopen write");
  }
  funlockfile(stream);
  if (!__freadable(stream) || !__fwritable(stream) || __fbufsize(stream) == 0) {
    fclose(stream);
    return fail("stdio_memory_test: stdio_ext state");
  }
  line = fgetln(stream, &line_length);
  if (line == 0 || line_length != 4 || memcmp(line, "one\n", 4) != 0) {
    fclose(stream);
    return fail("stdio_memory_test: fgetln first");
  }
  if (fpurge(stream) != 0 || fgetc(stream) != EOF || !feof(stream)) {
    fclose(stream);
    return fail("stdio_memory_test: fpurge");
  }
  if (fclose(stream) != 0 || !cookie.closed) {
    return fail("stdio_memory_test: funopen close");
  }

  puts("stdio_memory_test: ok");
  return 0;
}
