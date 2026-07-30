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
  FILE* next_open;
};

#define CRT_STDIO_NONE 0
#define CRT_STDIO_READ 1
#define CRT_STDIO_WRITE 2

long __crt_sys_unlink(const char* path);
long __crt_sys_rename(const char* old_path, const char* new_path);

static FILE stdin_storage = {0, 0, 0, 0, _IOLBF, 0, 0, 0, 0, 0, 0, CRT_STDIO_NONE, 0, 0, 0};
static FILE stdout_storage = {1, 0, 0, 0, _IOLBF, 0, 0, 0, 0, 0, 0, CRT_STDIO_NONE, 0, 0, 0};
static FILE stderr_storage = {2, 0, 0, 0, _IONBF, 0, 0, 0, 0, 0, 0, CRT_STDIO_NONE, 0, 0, 0};

FILE* stdin = &stdin_storage;
FILE* stdout = &stdout_storage;
FILE* stderr = &stderr_storage;

static FILE* open_streams;

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

static void init_stream(FILE* stream, int fd, int owned) {
  stream->fd = fd;
  stream->owned = owned;
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
}

static void register_stream(FILE* stream) {
  stream->next_open = open_streams;
  open_streams = stream;
}

static void unregister_stream(FILE* stream) {
  FILE** current = &open_streams;

  while (*current != 0) {
    if (*current == stream) {
      *current = stream->next_open;
      stream->next_open = 0;
      return;
    }
    current = &(*current)->next_open;
  }
}

static void reset_buffer_state(FILE* stream) {
  stream->buffer_pos = 0;
  stream->buffer_len = 0;
  stream->buffer_dirty = 0;
  stream->ungot = 0;
}

static int discard_read_buffer(FILE* stream) {
  off_t unread = 0;

  if (stream->last_op != CRT_STDIO_READ) {
    reset_buffer_state(stream);
    return 0;
  }
  if (stream->buffer_len > stream->buffer_pos) {
    unread += (off_t)(stream->buffer_len - stream->buffer_pos);
  }
  if (stream->ungot) {
    ++unread;
  }
  if (unread != 0 && lseek(stream->fd, -unread, SEEK_CUR) < 0 && errno != ESPIPE) {
    stream->error = 1;
    return -1;
  }
  reset_buffer_state(stream);
  return 0;
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
  if (stream->last_op == CRT_STDIO_READ && discard_read_buffer(stream) != 0) {
    return -1;
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
  init_stream(stream, fd, 1);
  register_stream(stream);

  if ((flags & O_APPEND) != 0) {
    lseek(fd, 0, SEEK_END);
  }

  return stream;
}

FILE* fdopen(int fd, const char* mode) {
  int flags = parse_mode(mode);
  FILE* stream;

  if (flags < 0) {
    return 0;
  }
  if (fd < 0) {
    errno = EBADF;
    return 0;
  }
  stream = (FILE*)malloc(sizeof(FILE));
  if (stream == 0) {
    errno = ENOMEM;
    return 0;
  }
  init_stream(stream, fd, 1);
  register_stream(stream);
  if ((flags & O_APPEND) != 0) {
    lseek(fd, 0, SEEK_END);
  }
  return stream;
}

FILE* freopen(const char* path, const char* mode, FILE* stream) {
  int flags = parse_mode(mode);
  int fd;

  if (stream_fd(stream) < 0 || flags < 0) {
    return 0;
  }
  if (fflush(stream) != 0) {
    return 0;
  }
  fd = open(path, flags, 0666);
  if (fd < 0) {
    stream->error = 1;
    return 0;
  }
  if (stream->owned) {
    close(stream->fd);
  }
  if (stream->buffer_owned) {
    free(stream->buffer);
  }
  init_stream(stream, fd, 1);
  if ((flags & O_APPEND) != 0) {
    lseek(fd, 0, SEEK_END);
  }
  return stream;
}

FILE* tmpfile(void) {
  char name[] = "crt_tmpfile_XXXXXX.tmp";
  static unsigned long counter;
  unsigned long attempt;

  for (attempt = 0; attempt < 1000; ++attempt) {
    unsigned long value = counter++;
    int fd;
    FILE* stream;

    name[12] = (char)('0' + (value / 100000UL) % 10UL);
    name[13] = (char)('0' + (value / 10000UL) % 10UL);
    name[14] = (char)('0' + (value / 1000UL) % 10UL);
    name[15] = (char)('0' + (value / 100UL) % 10UL);
    name[16] = (char)('0' + (value / 10UL) % 10UL);
    name[17] = (char)('0' + value % 10UL);
    fd = open(name, O_CREAT | O_EXCL | O_RDWR | O_TRUNC, 0600);
    if (fd < 0) {
      continue;
    }
    remove(name);
    stream = (FILE*)malloc(sizeof(FILE));
    if (stream == 0) {
      close(fd);
      errno = ENOMEM;
      return 0;
    }
    init_stream(stream, fd, 1);
    register_stream(stream);
    return stream;
  }
  errno = EEXIST;
  return 0;
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
    unregister_stream(stream);
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

int fileno(FILE* stream) {
  return stream_fd(stream);
}

static int stream_seek(FILE* stream, off_t offset, int whence) {
  int fd = stream_fd(stream);
  off_t adjusted = offset;

  if (fd < 0) {
    return -1;
  }
  if (stream->last_op == CRT_STDIO_WRITE && fflush(stream) != 0) {
    return -1;
  }
  if (stream->last_op == CRT_STDIO_READ && whence == SEEK_CUR) {
    adjusted -= (off_t)(stream->buffer_len - stream->buffer_pos);
    if (stream->ungot) {
      --adjusted;
    }
  }
  if (lseek(fd, adjusted, whence) < 0) {
    stream->error = 1;
    return -1;
  }
  stream->eof = 0;
  reset_buffer_state(stream);
  stream->last_op = CRT_STDIO_NONE;
  return 0;
}

static off_t stream_tell(FILE* stream) {
  int fd = stream_fd(stream);
  off_t result;

  if (fd < 0) {
    return (off_t)-1;
  }
  result = lseek(fd, 0, SEEK_CUR);
  if (result < 0) {
    return (off_t)-1;
  }
  if (stream->last_op == CRT_STDIO_WRITE && stream->buffer_dirty) {
    result += (off_t)stream->buffer_len;
  } else if (stream->last_op == CRT_STDIO_READ) {
    result -= (off_t)(stream->buffer_len - stream->buffer_pos);
    if (stream->ungot) {
      --result;
    }
  }
  return result;
}

int fseek(FILE* stream, long offset, int whence) {
  return stream_seek(stream, (off_t)offset, whence);
}

long ftell(FILE* stream) {
  return (long)stream_tell(stream);
}

int fseeko(FILE* stream, off_t offset, int whence) {
  return stream_seek(stream, offset, whence);
}

off_t ftello(FILE* stream) {
  return stream_tell(stream);
}

void rewind(FILE* stream) {
  if (stream_seek(stream, 0, SEEK_SET) == 0) {
    clearerr(stream);
  }
}

int fgetpos(FILE* stream, fpos_t* pos) {
  off_t result;

  if (pos == 0) {
    errno = EINVAL;
    return -1;
  }
  result = stream_tell(stream);
  if (result < 0) {
    return -1;
  }
  *pos = result;
  return 0;
}

int fsetpos(FILE* stream, const fpos_t* pos) {
  if (pos == 0) {
    errno = EINVAL;
    return -1;
  }
  return stream_seek(stream, *pos, SEEK_SET);
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

char* fgets(char* s, int size, FILE* stream) {
  int i = 0;

  if (s == 0 || size <= 0) {
    errno = EINVAL;
    return 0;
  }
  while (i < size - 1) {
    int ch = fgetc(stream);
    if (ch == EOF) {
      break;
    }
    s[i++] = (char)ch;
    if (ch == '\n') {
      break;
    }
  }
  if (i == 0) {
    return 0;
  }
  s[i] = 0;
  return s;
}

ssize_t getdelim(char** lineptr, size_t* n, int delimiter, FILE* stream) {
  size_t pos = 0;
  int ch;

  if (lineptr == 0 || n == 0 || stream_fd(stream) < 0) {
    errno = EINVAL;
    return -1;
  }
  if (*lineptr == 0 || *n == 0) {
    *n = 128;
    *lineptr = (char*)malloc(*n);
    if (*lineptr == 0) {
      *n = 0;
      return -1;
    }
  }

  while ((ch = fgetc(stream)) != EOF) {
    if (pos + 1 >= *n) {
      size_t new_size = *n * 2;
      char* new_line;

      if (new_size <= *n) {
        errno = ENOMEM;
        return -1;
      }
      new_line = (char*)realloc(*lineptr, new_size);
      if (new_line == 0) {
        return -1;
      }
      *lineptr = new_line;
      *n = new_size;
    }
    (*lineptr)[pos++] = (char)ch;
    if (ch == delimiter) {
      break;
    }
  }
  if (pos == 0 && ch == EOF) {
    return -1;
  }
  (*lineptr)[pos] = 0;
  return (ssize_t)pos;
}

ssize_t getline(char** lineptr, size_t* n, FILE* stream) {
  return getdelim(lineptr, n, '\n', stream);
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
    FILE* current;

    if (fflush(stdout) != 0) {
      result = EOF;
    }
    if (fflush(stderr) != 0) {
      result = EOF;
    }
    current = open_streams;
    while (current != 0) {
      if (fflush(current) != 0) {
        result = EOF;
      }
      current = current->next_open;
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
    return discard_read_buffer(stream) == 0 ? 0 : EOF;
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
  stream->buffer = mode == _IONBF || size == 0 ? 0 : buf;
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

void perror(const char* s) {
  if (s != 0 && s[0] != 0) {
    fputs(s, stderr);
    fputs(": ", stderr);
  }
  fputs(strerror(errno), stderr);
  fputc('\n', stderr);
}
