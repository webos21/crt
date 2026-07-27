#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct __crt_FILE {
  int fd;
};

static FILE stdin_storage = {0};
static FILE stdout_storage = {1};
static FILE stderr_storage = {2};

FILE* stdin = &stdin_storage;
FILE* stdout = &stdout_storage;
FILE* stderr = &stderr_storage;

static int stream_fd(FILE* stream) {
  if (stream == 0) {
    errno = EBADF;
    return -1;
  }
  return stream->fd;
}

int fputc(int c, FILE* stream) {
  unsigned char byte = (unsigned char)c;
  int fd = stream_fd(stream);

  if (fd < 0) {
    return EOF;
  }
  if (write(fd, &byte, 1) != 1) {
    return EOF;
  }
  return byte;
}

int fputs(const char* s, FILE* stream) {
  size_t length;
  int fd = stream_fd(stream);

  if (fd < 0) {
    return EOF;
  }
  length = strlen(s);
  if (write(fd, s, length) != (ssize_t)length) {
    return EOF;
  }
  return 0;
}

int puts(const char* s) {
  if (fputs(s, stdout) == EOF) {
    return EOF;
  }
  if (fputc('\n', stdout) == EOF) {
    return EOF;
  }
  return 0;
}

int putchar(int c) {
  return fputc(c, stdout);
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
  size_t total;
  ssize_t result;
  int fd = stream_fd(stream);

  if (fd < 0 || size == 0 || nmemb == 0) {
    return 0;
  }
  if (nmemb > ((size_t)-1) / size) {
    errno = EINVAL;
    return 0;
  }

  total = size * nmemb;
  result = read(fd, ptr, total);
  if (result <= 0) {
    return 0;
  }
  return (size_t)result / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
  size_t total;
  ssize_t result;
  int fd = stream_fd(stream);

  if (fd < 0 || size == 0 || nmemb == 0) {
    return 0;
  }
  if (nmemb > ((size_t)-1) / size) {
    errno = EINVAL;
    return 0;
  }

  total = size * nmemb;
  result = write(fd, ptr, total);
  if (result <= 0) {
    return 0;
  }
  return (size_t)result / size;
}

int fflush(FILE* stream) {
  if (stream == 0) {
    return 0;
  }
  if (stream_fd(stream) < 0) {
    return EOF;
  }
  return 0;
}
