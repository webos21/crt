#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct __crt_FILE {
  int fd;
  int owned;
  int eof;
  int error;
};

long __crt_sys_unlink(const char* path);
long __crt_sys_rename(const char* old_path, const char* new_path);

static FILE stdin_storage = {0, 0, 0, 0};
static FILE stdout_storage = {1, 0, 0, 0};
static FILE stderr_storage = {2, 0, 0, 0};

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

static int parse_mode(const char* mode) {
  int flags;

  if (mode == 0 || mode[0] == 0) {
    errno = EINVAL;
    return -1;
  }

  if (mode[0] == 'r') {
    flags = O_RDONLY;
  } else if (mode[0] == 'w') {
    flags = O_WRONLY | O_CREAT | O_TRUNC;
  } else if (mode[0] == 'a') {
    flags = O_WRONLY | O_CREAT | O_APPEND;
  } else {
    errno = EINVAL;
    return -1;
  }

  for (++mode; *mode != 0; ++mode) {
    if (*mode == '+') {
      flags &= ~(O_RDONLY | O_WRONLY | O_RDWR);
      flags |= O_RDWR;
    }
  }

  return flags;
}

FILE* fopen(const char* path, const char* mode) {
  int flags = parse_mode(mode);
  int fd;
  FILE* stream;

  if (flags < 0) {
    return 0;
  }

  fd = open(path, flags, 0666);
  if (fd < 0) {
    return 0;
  }

  stream = (FILE*)malloc(sizeof(FILE));
  if (stream == 0) {
    close(fd);
    return 0;
  }
  stream->fd = fd;
  stream->owned = 1;
  stream->eof = 0;
  stream->error = 0;

  if ((flags & O_APPEND) != 0) {
    lseek(fd, 0, SEEK_END);
  }

  return stream;
}

int fclose(FILE* stream) {
  int result = 0;

  if (stream_fd(stream) < 0) {
    return EOF;
  }
  if (stream->owned) {
    result = close(stream->fd);
    free(stream);
    if (result != 0) {
      return EOF;
    }
  }
  return 0;
}

int fseek(FILE* stream, long offset, int whence) {
  int fd = stream_fd(stream);

  if (fd < 0) {
    return -1;
  }
  if (lseek(fd, (off_t)offset, whence) < 0) {
    stream->error = 1;
    return -1;
  }
  stream->eof = 0;
  return 0;
}

long ftell(FILE* stream) {
  int fd = stream_fd(stream);
  off_t result;

  if (fd < 0) {
    return -1;
  }
  result = lseek(fd, 0, SEEK_CUR);
  if (result < 0) {
    return -1;
  }
  return (long)result;
}

int fputc(int c, FILE* stream) {
  unsigned char byte = (unsigned char)c;
  int fd = stream_fd(stream);

  if (fd < 0) {
    return EOF;
  }
  if (write(fd, &byte, 1) != 1) {
    stream->error = 1;
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
    stream->error = 1;
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
    if (result == 0) {
      stream->eof = 1;
    } else {
      stream->error = 1;
    }
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
    stream->error = 1;
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

int feof(FILE* stream) {
  if (stream_fd(stream) < 0) {
    return 0;
  }
  return stream->eof;
}

int ferror(FILE* stream) {
  if (stream_fd(stream) < 0) {
    return 1;
  }
  return stream->error;
}

void clearerr(FILE* stream) {
  if (stream == 0) {
    return;
  }
  stream->eof = 0;
  stream->error = 0;
}

int remove(const char* path) {
  long result;

  if (path == 0) {
    errno = EINVAL;
    return -1;
  }
  result = __crt_sys_unlink(path);
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return (int)result;
}

int rename(const char* old_path, const char* new_path) {
  long result;

  if (old_path == 0 || new_path == 0) {
    errno = EINVAL;
    return -1;
  }
  result = __crt_sys_rename(old_path, new_path);
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return (int)result;
}
