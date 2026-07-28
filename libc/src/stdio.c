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
  int buffering_mode;
  char* buffer;
  size_t buffer_size;
  int buffer_owned;
  size_t buffer_pos;
  size_t buffer_len;
  int buffer_dirty;
  int last_op;
  int ungot;
  unsigned char ungot_char;
};

#define CRT_STDIO_NONE 0
#define CRT_STDIO_READ 1
#define CRT_STDIO_WRITE 2

long __crt_sys_unlink(const char* path);
long __crt_sys_rename(const char* old_path, const char* new_path);

static FILE stdin_storage = {0, 0, 0, 0, _IOLBF, 0, 0, 0, 0, 0, 0, CRT_STDIO_NONE, 0, 0};
static FILE stdout_storage = {1, 0, 0, 0, _IOLBF, 0, 0, 0, 0, 0, 0, CRT_STDIO_NONE, 0, 0};
static FILE stderr_storage = {2, 0, 0, 0, _IONBF, 0, 0, 0, 0, 0, 0, CRT_STDIO_NONE, 0, 0};

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

static void reset_buffer_state(FILE* stream) {
  stream->buffer_pos = 0;
  stream->buffer_len = 0;
  stream->buffer_dirty = 0;
  stream->ungot = 0;
}

static int ensure_buffer(FILE* stream) {
  if (stream->buffering_mode == _IONBF) {
    return 0;
  }
  if (stream->buffer != 0 && stream->buffer_size != 0) {
    return 0;
  }
  stream->buffer = (char*)malloc(BUFSIZ);
  if (stream->buffer == 0) {
    stream->error = 1;
    return -1;
  }
  stream->buffer_size = BUFSIZ;
  stream->buffer_owned = 1;
  return 0;
}

static int flush_write_buffer(FILE* stream) {
  size_t done = 0;

  if (!stream->buffer_dirty || stream->buffer_len == 0) {
    stream->buffer_pos = 0;
    stream->buffer_len = 0;
    stream->buffer_dirty = 0;
    return 0;
  }
  while (done < stream->buffer_len) {
    ssize_t result = write(stream->fd, stream->buffer + done, stream->buffer_len - done);
    if (result <= 0) {
      stream->error = 1;
      return -1;
    }
    done += (size_t)result;
  }
  stream->buffer_pos = 0;
  stream->buffer_len = 0;
  stream->buffer_dirty = 0;
  return 0;
}

static int prepare_read(FILE* stream) {
  if (stream->last_op == CRT_STDIO_WRITE && flush_write_buffer(stream) != 0) {
    return -1;
  }
  if (stream->last_op != CRT_STDIO_READ) {
    reset_buffer_state(stream);
  }
  stream->last_op = CRT_STDIO_READ;
  return 0;
}

static int prepare_write(FILE* stream) {
  if (stream->last_op == CRT_STDIO_READ) {
    reset_buffer_state(stream);
  }
  stream->last_op = CRT_STDIO_WRITE;
  stream->eof = 0;
  return 0;
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
  stream->buffering_mode = _IOFBF;
  stream->buffer = 0;
  stream->buffer_size = 0;
  stream->buffer_owned = 0;
  stream->buffer_pos = 0;
  stream->buffer_len = 0;
  stream->buffer_dirty = 0;
  stream->last_op = CRT_STDIO_NONE;
  stream->ungot = 0;
  stream->ungot_char = 0;

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
  if (fflush(stream) != 0) {
    result = -1;
  }
  if (stream->owned) {
    if (close(stream->fd) != 0) {
      result = -1;
    }
    if (stream->buffer_owned) {
      free(stream->buffer);
    }
    free(stream);
    if (result != 0) {
      return EOF;
    }
  }
  return 0;
}

int fseek(FILE* stream, long offset, int whence) {
  int fd = stream_fd(stream);
  long adjusted = offset;

  if (fd < 0) {
    return -1;
  }
  if (stream->last_op == CRT_STDIO_WRITE && fflush(stream) != 0) {
    return -1;
  }
  if (stream->last_op == CRT_STDIO_READ && whence == SEEK_CUR) {
    adjusted -= (long)(stream->buffer_len - stream->buffer_pos);
    if (stream->ungot) {
      --adjusted;
    }
  }
  if (lseek(fd, (off_t)adjusted, whence) < 0) {
    stream->error = 1;
    return -1;
  }
  stream->eof = 0;
  reset_buffer_state(stream);
  stream->last_op = CRT_STDIO_NONE;
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
  if (stream->last_op == CRT_STDIO_WRITE && stream->buffer_dirty) {
    result += (off_t)stream->buffer_len;
  } else if (stream->last_op == CRT_STDIO_READ) {
    result -= (off_t)(stream->buffer_len - stream->buffer_pos);
    if (stream->ungot) {
      --result;
    }
  }
  return (long)result;
}

int fputc(int c, FILE* stream) {
  unsigned char byte = (unsigned char)c;
  int fd = stream_fd(stream);

  if (fd < 0) {
    return EOF;
  }
  if (prepare_write(stream) != 0) {
    return EOF;
  }
  if (stream->buffering_mode == _IONBF) {
    if (write(fd, &byte, 1) != 1) {
      stream->error = 1;
      return EOF;
    }
    return byte;
  }
  if (ensure_buffer(stream) != 0) {
    return EOF;
  }
  if (stream->buffer_len == stream->buffer_size && flush_write_buffer(stream) != 0) {
    return EOF;
  }
  stream->buffer[stream->buffer_len++] = (char)byte;
  stream->buffer_dirty = 1;
  if (stream->buffer_len == stream->buffer_size ||
      (stream->buffering_mode == _IOLBF && byte == '\n')) {
    if (flush_write_buffer(stream) != 0) {
      return EOF;
    }
  }
  return byte;
}

int putc(int c, FILE* stream) {
  return fputc(c, stream);
}

int fputs(const char* s, FILE* stream) {
  size_t i;
  size_t length;

  if (stream_fd(stream) < 0) {
    return EOF;
  }
  length = strlen(s);
  for (i = 0; i < length; ++i) {
    if (fputc((unsigned char)s[i], stream) == EOF) {
      return EOF;
    }
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

int fgetc(FILE* stream) {
  unsigned char byte;
  int fd = stream_fd(stream);
  ssize_t result;

  if (fd < 0) {
    return EOF;
  }
  if (prepare_read(stream) != 0) {
    return EOF;
  }
  if (stream->ungot) {
    stream->ungot = 0;
    stream->eof = 0;
    return stream->ungot_char;
  }
  if (stream->buffering_mode != _IONBF) {
    if (ensure_buffer(stream) != 0) {
      return EOF;
    }
    if (stream->buffer_pos >= stream->buffer_len) {
      result = read(fd, stream->buffer, stream->buffer_size);
      if (result > 0) {
        stream->buffer_pos = 0;
        stream->buffer_len = (size_t)result;
      } else {
        if (result == 0) {
          stream->eof = 1;
        } else {
          stream->error = 1;
        }
        return EOF;
      }
    }
    stream->eof = 0;
    return (unsigned char)stream->buffer[stream->buffer_pos++];
  }
  result = read(fd, &byte, 1);
  if (result == 1) {
    stream->eof = 0;
    return byte;
  }
  if (result == 0) {
    stream->eof = 1;
  } else {
    stream->error = 1;
  }
  return EOF;
}

int getc(FILE* stream) {
  return fgetc(stream);
}

int getchar(void) {
  return fgetc(stdin);
}

int ungetc(int c, FILE* stream) {
  if (stream_fd(stream) < 0 || c == EOF || stream->ungot) {
    return EOF;
  }
  stream->ungot = 1;
  stream->ungot_char = (unsigned char)c;
  stream->eof = 0;
  return (unsigned char)c;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
  size_t total;
  size_t done = 0;
  unsigned char* out = (unsigned char*)ptr;

  if (stream_fd(stream) < 0 || size == 0 || nmemb == 0) {
    return 0;
  }
  if (nmemb > ((size_t)-1) / size) {
    errno = EINVAL;
    return 0;
  }

  total = size * nmemb;
  while (done < total) {
    int ch = fgetc(stream);
    if (ch == EOF) {
      break;
    }
    out[done++] = (unsigned char)ch;
  }
  return done / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
  size_t total;
  size_t done = 0;
  const unsigned char* in = (const unsigned char*)ptr;

  if (stream_fd(stream) < 0 || size == 0 || nmemb == 0) {
    return 0;
  }
  if (nmemb > ((size_t)-1) / size) {
    errno = EINVAL;
    return 0;
  }

  total = size * nmemb;
  while (done < total) {
    if (fputc(in[done], stream) == EOF) {
      break;
    }
    ++done;
  }
  return done / size;
}

int fflush(FILE* stream) {
  if (stream == 0) {
    int result = 0;
    if (fflush(stdout) != 0) {
      result = EOF;
    }
    if (fflush(stderr) != 0) {
      result = EOF;
    }
    return result;
  }
  if (stream_fd(stream) < 0) {
    return EOF;
  }
  if (stream->last_op == CRT_STDIO_WRITE) {
    return flush_write_buffer(stream) == 0 ? 0 : EOF;
  }
  if (stream->last_op == CRT_STDIO_READ) {
    reset_buffer_state(stream);
  }
  return 0;
}

void setbuf(FILE* stream, char* buf) {
  (void)setvbuf(stream, buf, buf != 0 ? _IOFBF : _IONBF, BUFSIZ);
}

int setvbuf(FILE* stream, char* buf, int mode, size_t size) {
  if (stream_fd(stream) < 0) {
    return EOF;
  }
  if (mode != _IOFBF && mode != _IOLBF && mode != _IONBF) {
    errno = EINVAL;
    return EOF;
  }
  if (fflush(stream) != 0) {
    return EOF;
  }
  if (stream->buffer_owned) {
    free(stream->buffer);
  }
  stream->buffering_mode = mode;
  stream->buffer = buf;
  stream->buffer_size = mode == _IONBF ? 0 : size;
  stream->buffer_owned = 0;
  reset_buffer_state(stream);
  stream->last_op = CRT_STDIO_NONE;
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
