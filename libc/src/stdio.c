#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pthread.h>
#include <spawn.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wchar.h>

struct __sbuf {
  unsigned char* _base;
  size_t _size;
};

#define WCIO_UNGETWC_BUFSIZE 1

struct wchar_io_data {
  mbstate_t wcio_mbstate_in;
  mbstate_t wcio_mbstate_out;
  wchar_t wcio_ungetwc_buf[WCIO_UNGETWC_BUFSIZE];
  size_t wcio_ungetwc_inbuf;
  int wcio_mode;
};

struct __sfileext {
  struct __sbuf _ub;
  struct wchar_io_data _wcio;
  pthread_mutex_t _lock;
  int _caller_handles_locking;
  pthread_t _lock_owner;
  int _lock_count;
  off64_t (*_seek64)(void*, off64_t, int);
  pid_t _popen_pid;
};

struct __sFILE {
  unsigned char* _p;
  int _r;
  int _w;
  int _flags;
  int _file;
  struct __sbuf _bf;
  int _lbfsize;
  void* _cookie;
  int (*_close)(void*);
  int (*_read)(void*, char*, int);
  fpos_t (*_seek)(void*, fpos_t, int);
  int (*_write)(void*, const char*, int);
  struct __sbuf _ext;
  unsigned char* _up;
  int _ur;
  unsigned char _ubuf[3];
  unsigned char _nbuf[1];
  struct __sbuf _lb;
  int _blksize;
  fpos_t _unused_0;
};

#define CRT_STDIO_NONE 0
#define CRT_STDIO_READ 1
#define CRT_STDIO_WRITE 2
#define CRT_STDIO_KIND_FD 0
#define CRT_STDIO_KIND_MEMORY 1
#define CRT_STDIO_KIND_FUNOPEN 2

#define __SLBF 0x0001
#define __SNBF 0x0002
#define __SRD 0x0004
#define __SWR 0x0008
#define __SRW 0x0010
#define __SEOF 0x0020
#define __SERR 0x0040
#define __SMBF 0x0080
#define __SSTR 0x0200
#define __SALC 0x4000

#define _EXT(fp) ((struct __sfileext*)((fp)->_ext._base))
#define ORIENT_BYTES (-1)
#define ORIENT_UNKNOWN 0
#define ORIENT_CHARS 1

struct crt_stdio_cookie {
  int kind;
  int fd;
  int owned;
  int readable;
  int writable;
  int append;
  int ext_owned;
  FILE* stream;
  struct crt_stdio_cookie* next_open;
  char* mem_base;
  size_t mem_size;
  size_t mem_pos;
  size_t mem_len;
  int mem_owned;
  char** mem_open_ptr;
  size_t* mem_open_size;
  void* user_cookie;
  int (*user_close)(void*);
  int (*user_read)(void*, char*, int);
  fpos_t (*user_seek)(void*, fpos_t, int);
  int (*user_write)(void*, const char*, int);
};

long __crt_sys_unlink(const char* path);
long __crt_sys_rename(const char* old_path, const char* new_path);

static int fd_cookie_close(void* opaque);
static int fd_cookie_read(void* opaque, char* buf, int count);
static fpos_t fd_cookie_seek(void* opaque, fpos_t offset, int whence);
static int fd_cookie_write(void* opaque, const char* buf, int count);
int __sread(void* opaque, char* buf, int count);
int __swrite(void* opaque, const char* buf, int count);
fpos_t __sseek(void* opaque, fpos_t offset, int whence);
int __sclose(void* opaque);
int __srefill(FILE* stream);
int __srget(FILE* stream);
int __swsetup(FILE* stream);
int __swbuf(int c, FILE* stream);
int __sflush(FILE* stream);
static int fputc_unlocked_impl(int c, FILE* stream);
static int fgetc_unlocked_impl(FILE* stream);
static int fputs_unlocked_impl(const char* s, FILE* stream);
static char* fgets_unlocked_impl(char* s, int size, FILE* stream);
static size_t fread_unlocked_impl(void* ptr, size_t size, size_t nmemb, FILE* stream);
static size_t fwrite_unlocked_impl(const void* ptr, size_t size, size_t nmemb, FILE* stream);
static int fflush_unlocked_impl(FILE* stream);
static char* fgetln_unlocked_impl(FILE* stream, size_t* lengthp);

static struct __sfileext __sFext[3] = {
    { {0, 0}, {{0}, {0}, {0}, 0, 0}, PTHREAD_MUTEX_INITIALIZER, 0, 0, 0, 0, 0 },
    { {0, 0}, {{0}, {0}, {0}, 0, 0}, PTHREAD_MUTEX_INITIALIZER, 0, 0, 0, 0, 0 },
    { {0, 0}, {{0}, {0}, {0}, 0, 0}, PTHREAD_MUTEX_INITIALIZER, 0, 0, 0, 0, 0 },
};

static struct crt_stdio_cookie std_cookies[3] = {
    { CRT_STDIO_KIND_FD, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { CRT_STDIO_KIND_FD, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { CRT_STDIO_KIND_FD, 2, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

FILE __sF[3] = {
    {
        ._flags = __SRD,
        ._file = 0,
        ._cookie = &std_cookies[0],
        ._close = __sclose,
        ._read = __sread,
        ._seek = __sseek,
        ._write = __swrite,
        ._ext = { (unsigned char*)&__sFext[0], sizeof(__sFext[0]) },
    },
    {
        ._flags = __SWR,
        ._file = 1,
        ._cookie = &std_cookies[1],
        ._close = __sclose,
        ._read = __sread,
        ._seek = __sseek,
        ._write = __swrite,
        ._ext = { (unsigned char*)&__sFext[1], sizeof(__sFext[1]) },
    },
    {
        ._flags = __SWR | __SNBF,
        ._file = 2,
        ._cookie = &std_cookies[2],
        ._close = __sclose,
        ._read = __sread,
        ._seek = __sseek,
        ._write = __swrite,
        ._ext = { (unsigned char*)&__sFext[2], sizeof(__sFext[2]) },
    },
};

FILE* stdin = &__sF[0];
FILE* stdout = &__sF[1];
FILE* stderr = &__sF[2];

static struct crt_stdio_cookie* open_streams;

static int stream_valid(FILE* stream) {
  if (stream == 0) {
    errno = EBADF;
    return 0;
  }
  return 1;
}

static struct crt_stdio_cookie* stream_cookie(FILE* stream) {
  if (stream == 0 || stream->_cookie == 0) {
    errno = EBADF;
    return 0;
  }
  return (struct crt_stdio_cookie*)stream->_cookie;
}

static int stream_fd(FILE* stream) {
  struct crt_stdio_cookie* cookie = stream_cookie(stream);

  if (cookie == 0 || cookie->kind != CRT_STDIO_KIND_FD || cookie->fd < 0) {
    errno = EBADF;
    return -1;
  }
  return cookie->fd;
}

static int is_memory_stream(FILE* stream) {
  struct crt_stdio_cookie* cookie;

  if (stream == 0 || stream->_cookie == 0) {
    return 0;
  }
  cookie = (struct crt_stdio_cookie*)stream->_cookie;
  return cookie->kind == CRT_STDIO_KIND_MEMORY;
}

static struct __sfileext* stream_ext(FILE* stream) {
  if (stream == 0 || stream->_ext._base == 0) {
    return 0;
  }
  return (struct __sfileext*)stream->_ext._base;
}

static int stream_needs_implicit_lock(FILE* stream) {
  struct __sfileext* ext = stream_ext(stream);

  return ext != 0 && !ext->_caller_handles_locking;
}

static void lock_stream_if_needed(FILE* stream) {
  if (stream_needs_implicit_lock(stream)) {
    flockfile(stream);
  }
}

static void unlock_stream_if_needed(FILE* stream) {
  if (stream_needs_implicit_lock(stream)) {
    funlockfile(stream);
  }
}

static void reset_stream_lock_after_fork(FILE* stream) {
  struct __sfileext* ext = stream_ext(stream);

  if (ext == 0) {
    return;
  }
  pthread_mutex_init(&ext->_lock, 0);
  ext->_lock_owner = 0;
  ext->_lock_count = 0;
}

void __crt_stdio_after_fork_child(void) {
  struct crt_stdio_cookie* current;

  reset_stream_lock_after_fork(stdin);
  reset_stream_lock_after_fork(stdout);
  reset_stream_lock_after_fork(stderr);
  for (current = open_streams; current != 0; current = current->next_open) {
    reset_stream_lock_after_fork(current->stream);
  }
}

static void set_stream_error(FILE* stream) {
  stream->_flags |= __SERR;
}

static void set_stream_eof(FILE* stream) {
  stream->_flags |= __SEOF;
}

static void clear_stream_eof(FILE* stream) {
  stream->_flags &= ~__SEOF;
}

static int pop_ungetc(FILE* stream) {
  int ch;

  if (stream == 0 || stream->_ur == 0 || stream->_up == 0) {
    return EOF;
  }
  ch = *stream->_up++;
  --stream->_ur;
  if (stream->_ur == 0) {
    struct __sfileext* ext = stream_ext(stream);

    stream->_up = 0;
    if (ext != 0 && ext->_ub._base != 0) {
      stream->_up = 0;
    }
  }
  clear_stream_eof(stream);
  return ch;
}

static int stream_buffering_mode(FILE* stream) {
  if ((stream->_flags & __SNBF) != 0) {
    return _IONBF;
  }
  if ((stream->_flags & __SLBF) != 0) {
    return _IOLBF;
  }
  return _IOFBF;
}

static int stream_last_op(FILE* stream) {
  if ((stream->_flags & __SWR) != 0) {
    return CRT_STDIO_WRITE;
  }
  if ((stream->_flags & __SRD) != 0) {
    return CRT_STDIO_READ;
  }
  return CRT_STDIO_NONE;
}

static void sync_bionic_buffer_fields(FILE* stream) {
  if (stream == 0) {
    return;
  }
  if (stream->_bf._base == 0) {
    stream->_p = 0;
    stream->_r = 0;
    stream->_w = 0;
  }
  stream->_lbfsize = stream_buffering_mode(stream) == _IOLBF ? -(int)stream->_bf._size : 0;
}

static int bionic_flags_from_mode_flags(int flags) {
  if ((flags & O_RDWR) == O_RDWR) {
    return __SRW;
  }
  if ((flags & O_WRONLY) == O_WRONLY) {
    return __SWR;
  }
  return __SRD;
}

static void set_byte_orientation(FILE* stream) {
  struct __sfileext* ext = stream_ext(stream);

  if (ext != 0 && ext->_wcio.wcio_mode == ORIENT_UNKNOWN) {
    ext->_wcio.wcio_mode = ORIENT_BYTES;
  }
}

int __crt_stdio_get_orientation(FILE* stream) {
  struct __sfileext* ext = stream_ext(stream);

  if (ext == 0) {
    errno = EBADF;
    return 0;
  }
  return ext->_wcio.wcio_mode;
}

int __crt_stdio_set_orientation(FILE* stream, int mode) {
  struct __sfileext* ext = stream_ext(stream);

  if (ext == 0) {
    errno = EBADF;
    return 0;
  }
  if (ext->_wcio.wcio_mode == ORIENT_UNKNOWN && mode != ORIENT_UNKNOWN) {
    ext->_wcio.wcio_mode = mode;
  }
  return ext->_wcio.wcio_mode;
}

mbstate_t* __crt_stdio_get_mbstate_in(FILE* stream) {
  struct __sfileext* ext = stream_ext(stream);

  if (ext == 0) {
    errno = EBADF;
    return 0;
  }
  return &ext->_wcio.wcio_mbstate_in;
}

mbstate_t* __crt_stdio_get_mbstate_out(FILE* stream) {
  struct __sfileext* ext = stream_ext(stream);

  if (ext == 0) {
    errno = EBADF;
    return 0;
  }
  return &ext->_wcio.wcio_mbstate_out;
}

int __crt_stdio_pop_ungetwc(FILE* stream, wchar_t* wc) {
  struct __sfileext* ext = stream_ext(stream);

  if (ext == 0 || wc == 0 || ext->_wcio.wcio_ungetwc_inbuf == 0) {
    return 0;
  }
  *wc = ext->_wcio.wcio_ungetwc_buf[--ext->_wcio.wcio_ungetwc_inbuf];
  clear_stream_eof(stream);
  return 1;
}

int __crt_stdio_push_ungetwc(FILE* stream, wchar_t wc) {
  struct __sfileext* ext = stream_ext(stream);

  if (ext == 0 || ext->_wcio.wcio_ungetwc_inbuf >= WCIO_UNGETWC_BUFSIZE) {
    return -1;
  }
  ext->_wcio.wcio_ungetwc_buf[ext->_wcio.wcio_ungetwc_inbuf++] = wc;
  clear_stream_eof(stream);
  return 0;
}

static int fd_cookie_close(void* opaque) {
  struct crt_stdio_cookie* cookie = (struct crt_stdio_cookie*)opaque;

  if (cookie == 0 || cookie->fd < 0) {
    errno = EBADF;
    return -1;
  }
  return close(cookie->fd);
}

static int fd_cookie_read(void* opaque, char* buf, int count) {
  struct crt_stdio_cookie* cookie = (struct crt_stdio_cookie*)opaque;
  ssize_t result;

  if (cookie == 0 || cookie->fd < 0) {
    errno = EBADF;
    return -1;
  }
  result = read(cookie->fd, buf, (size_t)count);
  return result < 0 ? -1 : (int)result;
}

static fpos_t fd_cookie_seek(void* opaque, fpos_t offset, int whence) {
  struct crt_stdio_cookie* cookie = (struct crt_stdio_cookie*)opaque;

  if (cookie == 0 || cookie->fd < 0) {
    errno = EBADF;
    return (fpos_t)-1;
  }
  return lseek(cookie->fd, offset, whence);
}

static int fd_cookie_write(void* opaque, const char* buf, int count) {
  struct crt_stdio_cookie* cookie = (struct crt_stdio_cookie*)opaque;
  ssize_t result;

  if (cookie == 0 || cookie->fd < 0) {
    errno = EBADF;
    return -1;
  }
  result = write(cookie->fd, buf, (size_t)count);
  return result < 0 ? -1 : (int)result;
}

int __sread(void* opaque, char* buf, int count) {
  return fd_cookie_read(opaque, buf, count);
}

int __swrite(void* opaque, const char* buf, int count) {
  return fd_cookie_write(opaque, buf, count);
}

fpos_t __sseek(void* opaque, fpos_t offset, int whence) {
  return fd_cookie_seek(opaque, offset, whence);
}

int __sclose(void* opaque) {
  return fd_cookie_close(opaque);
}

static int funopen_cookie_close(void* opaque) {
  struct crt_stdio_cookie* cookie = (struct crt_stdio_cookie*)opaque;

  if (cookie == 0 || cookie->user_close == 0) {
    return 0;
  }
  return cookie->user_close(cookie->user_cookie);
}

static int funopen_cookie_read(void* opaque, char* buf, int count) {
  struct crt_stdio_cookie* cookie = (struct crt_stdio_cookie*)opaque;

  if (cookie == 0 || cookie->user_read == 0) {
    errno = EBADF;
    return -1;
  }
  return cookie->user_read(cookie->user_cookie, buf, count);
}

static fpos_t funopen_cookie_seek(void* opaque, fpos_t offset, int whence) {
  struct crt_stdio_cookie* cookie = (struct crt_stdio_cookie*)opaque;

  if (cookie == 0 || cookie->user_seek == 0) {
    errno = ESPIPE;
    return (fpos_t)-1;
  }
  return cookie->user_seek(cookie->user_cookie, offset, whence);
}

static int funopen_cookie_write(void* opaque, const char* buf, int count) {
  struct crt_stdio_cookie* cookie = (struct crt_stdio_cookie*)opaque;

  if (cookie == 0 || cookie->user_write == 0) {
    errno = EBADF;
    return -1;
  }
  return cookie->user_write(cookie->user_cookie, buf, count);
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

static void init_stream(FILE* stream, int fd, int owned, int readable, int writable, int append) {
  struct __sfileext* ext;
  struct crt_stdio_cookie* cookie;

  memset(stream, 0, sizeof(FILE));
  ext = (struct __sfileext*)malloc(sizeof(struct __sfileext));
  cookie = (struct crt_stdio_cookie*)malloc(sizeof(struct crt_stdio_cookie));
  if (cookie == 0) {
    free(ext);
    return;
  }
  memset(cookie, 0, sizeof(*cookie));
  cookie->kind = CRT_STDIO_KIND_FD;
  cookie->fd = fd;
  cookie->owned = owned;
  cookie->readable = readable;
  cookie->writable = writable;
  cookie->append = append;
  cookie->stream = stream;
  if (ext != 0) {
    memset(ext, 0, sizeof(*ext));
    pthread_mutex_init(&ext->_lock, 0);
    stream->_ext._base = (unsigned char*)ext;
    stream->_ext._size = sizeof(*ext);
    cookie->ext_owned = 1;
  }
  stream->_file = fd;
  stream->_cookie = cookie;
  stream->_close = __sclose;
  stream->_read = __sread;
  stream->_seek = __sseek;
  stream->_write = __swrite;
  sync_bionic_buffer_fields(stream);
}

static void register_stream(FILE* stream) {
  struct crt_stdio_cookie* cookie = stream_cookie(stream);

  if (cookie == 0) {
    return;
  }
  cookie->next_open = open_streams;
  open_streams = cookie;
}

static void unregister_stream(FILE* stream) {
  struct crt_stdio_cookie* cookie = stream_cookie(stream);
  struct crt_stdio_cookie** current = &open_streams;

  if (cookie == 0) {
    return;
  }

  while (*current != 0) {
    if (*current == cookie) {
      *current = cookie->next_open;
      cookie->next_open = 0;
      return;
    }
    current = &(*current)->next_open;
  }
}

static void reset_buffer_state(FILE* stream) {
  stream->_p = stream->_bf._base;
  stream->_r = 0;
  stream->_w = stream->_bf._base != 0 && stream_last_op(stream) == CRT_STDIO_WRITE ?
      (int)stream->_bf._size : 0;
  stream->_ur = 0;
  stream->_up = 0;
  sync_bionic_buffer_fields(stream);
}

static void free_extension_buffers(FILE* stream) {
  struct __sfileext* ext = stream_ext(stream);

  if (stream == 0) {
    return;
  }
  if (ext != 0 && ext->_ub._base != 0) {
    free(ext->_ub._base);
    ext->_ub._base = 0;
    ext->_ub._size = 0;
  }
  if (stream->_lb._base != 0) {
    free(stream->_lb._base);
    stream->_lb._base = 0;
    stream->_lb._size = 0;
  }
}

static int prepare_read(FILE* stream);
static int prepare_write(FILE* stream);

static size_t bounded_strlen(const char* s, size_t max) {
  size_t len = 0;

  while (len < max && s[len] != 0) {
    ++len;
  }
  return len;
}

static void sync_memory_stream(FILE* stream) {
  struct crt_stdio_cookie* cookie = stream_cookie(stream);

  if (cookie == 0 || cookie->kind != CRT_STDIO_KIND_MEMORY) {
    return;
  }
  if (cookie->mem_open_ptr != 0) {
    *cookie->mem_open_ptr = cookie->mem_base;
  }
  if (cookie->mem_open_size != 0) {
    *cookie->mem_open_size = cookie->mem_len;
  }
}

static int ensure_memory_capacity(FILE* stream, size_t needed) {
  struct crt_stdio_cookie* cookie = stream_cookie(stream);
  size_t capacity;
  char* grown;

  if (cookie == 0) {
    return -1;
  }
  if (needed <= cookie->mem_size) {
    return 0;
  }
  if (cookie->mem_open_ptr == 0) {
    errno = ENOSPC;
    set_stream_error(stream);
    return -1;
  }
  capacity = cookie->mem_size == 0 ? 64 : cookie->mem_size;
  while (capacity < needed) {
    size_t next = capacity * 2;
    if (next <= capacity) {
      errno = ENOMEM;
      set_stream_error(stream);
      return -1;
    }
    capacity = next;
  }
  grown = (char*)realloc(cookie->mem_base, capacity);
  if (grown == 0) {
    set_stream_error(stream);
    return -1;
  }
  cookie->mem_base = grown;
  cookie->mem_size = capacity;
  sync_memory_stream(stream);
  return 0;
}

static int memory_putc(FILE* stream, int c) {
  struct crt_stdio_cookie* cookie = stream_cookie(stream);
  unsigned char byte = (unsigned char)c;
  size_t needed;

  if (cookie == 0 || !cookie->writable) {
    errno = EBADF;
    set_stream_error(stream);
    return EOF;
  }
  if (prepare_write(stream) != 0) {
    return EOF;
  }
  set_byte_orientation(stream);
  if (cookie->append) {
    cookie->mem_pos = cookie->mem_len;
  }
  needed = cookie->mem_pos + 1;
  if (cookie->mem_open_ptr != 0 || byte != 0) {
    ++needed;
  }
  if (ensure_memory_capacity(stream, needed) != 0) {
    return EOF;
  }
  cookie->mem_base[cookie->mem_pos++] = (char)byte;
  if (cookie->mem_pos > cookie->mem_len) {
    cookie->mem_len = cookie->mem_pos;
  }
  if (cookie->mem_len < cookie->mem_size) {
    cookie->mem_base[cookie->mem_len] = 0;
  } else if (cookie->mem_size != 0) {
    cookie->mem_base[cookie->mem_size - 1] = 0;
  }
  sync_memory_stream(stream);
  return byte;
}

static int memory_getc(FILE* stream) {
  struct crt_stdio_cookie* cookie = stream_cookie(stream);

  if (cookie == 0 || !cookie->readable) {
    errno = EBADF;
    set_stream_error(stream);
    return EOF;
  }
  if (prepare_read(stream) != 0) {
    return EOF;
  }
  set_byte_orientation(stream);
  if (stream->_ur != 0) {
    return pop_ungetc(stream);
  }
  if (cookie->mem_pos >= cookie->mem_len) {
    set_stream_eof(stream);
    return EOF;
  }
  clear_stream_eof(stream);
  return (unsigned char)cookie->mem_base[cookie->mem_pos++];
}

static int discard_read_buffer(FILE* stream) {
  off_t unread = 0;

  if (is_memory_stream(stream)) {
    stream->_ur = 0;
    stream->_up = 0;
    stream->_r = 0;
    return 0;
  }
  if (stream_last_op(stream) != CRT_STDIO_READ) {
    reset_buffer_state(stream);
    return 0;
  }
  unread += (off_t)stream->_r;
  if (stream->_ur != 0) {
    unread += (off_t)stream->_ur;
  }
  if (stream->_seek == 0) {
    errno = ESPIPE;
    return -1;
  }
  if (unread != 0 && stream->_seek(stream->_cookie, -unread, SEEK_CUR) < 0 && errno != ESPIPE) {
    set_stream_error(stream);
    return -1;
  }
  reset_buffer_state(stream);
  return 0;
}

static int ensure_buffer(FILE* stream) {
  if (stream_buffering_mode(stream) == _IONBF) {
    return 0;
  }
  if (stream->_bf._base != 0 && stream->_bf._size != 0) {
    sync_bionic_buffer_fields(stream);
    return 0;
  }
  stream->_bf._base = (unsigned char*)malloc(BUFSIZ);
  if (stream->_bf._base == 0) {
    set_stream_error(stream);
    return -1;
  }
  stream->_bf._size = BUFSIZ;
  stream->_p = stream->_bf._base;
  stream->_flags |= __SMBF;
  sync_bionic_buffer_fields(stream);
  return 0;
}

static int flush_write_buffer(FILE* stream) {
  size_t pending;
  size_t done = 0;

  if (is_memory_stream(stream)) {
    sync_memory_stream(stream);
    reset_buffer_state(stream);
    return 0;
  }
  pending = stream->_p != 0 && stream->_bf._base != 0 && stream->_p >= stream->_bf._base ?
      (size_t)(stream->_p - stream->_bf._base) : 0;
  if (pending == 0) {
    reset_buffer_state(stream);
    return 0;
  }
  if (stream->_write == 0) {
    errno = EBADF;
    set_stream_error(stream);
    return -1;
  }
  while (done < pending) {
    int chunk = pending - done > (size_t)0x7fffffff ? 0x7fffffff : (int)(pending - done);
    int result = stream->_write(stream->_cookie, (const char*)stream->_bf._base + done, chunk);
    if (result <= 0) {
      set_stream_error(stream);
      return -1;
    }
    done += (size_t)result;
  }
  reset_buffer_state(stream);
  return 0;
}

static int prepare_read(FILE* stream) {
  struct crt_stdio_cookie* cookie = stream_cookie(stream);

  if (cookie == 0 || !cookie->readable || stream->_read == 0) {
    errno = EBADF;
    set_stream_error(stream);
    return -1;
  }
  if (stream_last_op(stream) == CRT_STDIO_WRITE && flush_write_buffer(stream) != 0) {
    return -1;
  }
  if (stream_last_op(stream) != CRT_STDIO_READ) {
    reset_buffer_state(stream);
  }
  stream->_flags &= ~__SWR;
  stream->_flags |= __SRD;
  sync_bionic_buffer_fields(stream);
  return 0;
}

static int prepare_write(FILE* stream) {
  struct crt_stdio_cookie* cookie = stream_cookie(stream);

  if (cookie == 0 || !cookie->writable || stream->_write == 0) {
    errno = EBADF;
    set_stream_error(stream);
    return -1;
  }
  if (stream_last_op(stream) == CRT_STDIO_READ && discard_read_buffer(stream) != 0) {
    return -1;
  }
  stream->_flags &= ~__SRD;
  stream->_flags |= __SWR;
  clear_stream_eof(stream);
  sync_bionic_buffer_fields(stream);
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
  init_stream(stream, fd, 1, (flags & O_WRONLY) == 0, (flags & (O_WRONLY | O_RDWR)) != 0, (flags & O_APPEND) != 0);
  if (stream->_cookie == 0) {
    free(stream);
    close(fd);
    errno = ENOMEM;
    return 0;
  }
  stream->_flags = bionic_flags_from_mode_flags(flags);
  register_stream(stream);

  if ((flags & O_APPEND) != 0) {
    stream->_seek(stream->_cookie, 0, SEEK_END);
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
  init_stream(stream, fd, 1, (flags & O_WRONLY) == 0, (flags & (O_WRONLY | O_RDWR)) != 0, (flags & O_APPEND) != 0);
  if (stream->_cookie == 0) {
    free(stream);
    errno = ENOMEM;
    return 0;
  }
  stream->_flags = bionic_flags_from_mode_flags(flags);
  register_stream(stream);
  if ((flags & O_APPEND) != 0) {
    stream->_seek(stream->_cookie, 0, SEEK_END);
  }
  return stream;
}

// popen()/pclose(): pclose() needs to know which child process belongs to
// a given popen()-returned FILE* in order to wait for it, so every open
// popen() stream is tracked here until pclose() (or process exit) retires
// it. No locking, matching this file's own open_streams list (register_
// stream()/unregister_stream() above) -- neither is meant to be thread-safe
// today.
#define CRT_POPEN_MAX 32
static FILE* popen_streams[CRT_POPEN_MAX];
static pid_t popen_pids[CRT_POPEN_MAX];

FILE* popen(const char* command, const char* type) {
  int pipe_fds[2];
  int parent_fd;
  int child_fd;
  int child_std_fd;
  posix_spawn_file_actions_t actions;
  pid_t pid;
  char* argv[4];
  FILE* stream;
  int slot;
  int saved_errno;

  if (command == 0 || type == 0 || (type[0] != 'r' && type[0] != 'w')) {
    errno = EINVAL;
    return 0;
  }
  for (slot = 0; slot < CRT_POPEN_MAX; ++slot) {
    if (popen_streams[slot] == 0) break;
  }
  if (slot == CRT_POPEN_MAX) {
    errno = EMFILE;
    return 0;
  }

  if (pipe(pipe_fds) != 0) {
    return 0;
  }
  if (type[0] == 'r') {
    parent_fd = pipe_fds[0];
    child_fd = pipe_fds[1];
    child_std_fd = 1;
  } else {
    parent_fd = pipe_fds[1];
    child_fd = pipe_fds[0];
    child_std_fd = 0;
  }

  if (posix_spawn_file_actions_init(&actions) != 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return 0;
  }
  if (posix_spawn_file_actions_adddup2(&actions, child_fd, child_std_fd) != 0 ||
      posix_spawn_file_actions_addclose(&actions, parent_fd) != 0 ||
      posix_spawn_file_actions_addclose(&actions, child_fd) != 0) {
    posix_spawn_file_actions_destroy(&actions);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return 0;
  }

  argv[0] = (char*)_PATH_BSHELL;
  argv[1] = "-c";
  argv[2] = (char*)command;
  argv[3] = 0;
  if (posix_spawn(&pid, _PATH_BSHELL, &actions, 0, argv, environ) != 0) {
    saved_errno = errno;
    posix_spawn_file_actions_destroy(&actions);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    errno = saved_errno;
    return 0;
  }
  posix_spawn_file_actions_destroy(&actions);
  close(child_fd);

  stream = fdopen(parent_fd, type);
  if (stream == 0) {
    saved_errno = errno;
    close(parent_fd);
    waitpid(pid, 0, 0);
    errno = saved_errno;
    return 0;
  }
  popen_streams[slot] = stream;
  popen_pids[slot] = pid;
  return stream;
}

int pclose(FILE* stream) {
  int slot;
  pid_t pid = -1;
  int status;

  for (slot = 0; slot < CRT_POPEN_MAX; ++slot) {
    if (popen_streams[slot] == stream) {
      pid = popen_pids[slot];
      popen_streams[slot] = 0;
      break;
    }
  }
  if (pid < 0) {
    errno = ECHILD;
    return -1;
  }
  if (fclose(stream) != 0) {
    return -1;
  }
  if (waitpid(pid, &status, 0) < 0) {
    return -1;
  }
  return status;
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
    set_stream_error(stream);
    return 0;
  }
  {
    struct crt_stdio_cookie* cookie = stream_cookie(stream);
    if (cookie != 0 && cookie->owned) {
      stream->_close(stream->_cookie);
    }
  }
  if ((stream->_flags & __SMBF) != 0) {
    free(stream->_bf._base);
  }
  {
    struct crt_stdio_cookie* cookie = stream_cookie(stream);
    if (cookie != 0 && cookie->ext_owned && stream_ext(stream) != 0) {
      pthread_mutex_destroy(&stream_ext(stream)->_lock);
      free(stream->_ext._base);
    }
    free(cookie);
  }
  init_stream(stream, fd, 1, (flags & O_WRONLY) == 0, (flags & (O_WRONLY | O_RDWR)) != 0, (flags & O_APPEND) != 0);
  if (stream->_cookie == 0) {
    close(fd);
    errno = ENOMEM;
    return 0;
  }
  stream->_flags = bionic_flags_from_mode_flags(flags);
  if ((flags & O_APPEND) != 0) {
    stream->_seek(stream->_cookie, 0, SEEK_END);
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
    init_stream(stream, fd, 1, 1, 1, 0);
    if (stream->_cookie == 0) {
      close(fd);
      free(stream);
      errno = ENOMEM;
      return 0;
    }
    stream->_flags = __SRW;
    register_stream(stream);
    return stream;
  }
  errno = EEXIST;
  return 0;
}

static const char* get_tmpdir(void) {
  const char* tmpdir = getenv("TMPDIR");

  return tmpdir != 0 ? tmpdir : "/data/local/tmp";
}

char* tmpnam(char* s) {
  static char buffer[L_tmpnam];

  if (s == 0) {
    s = buffer;
  }
  if (snprintf(s, L_tmpnam, "%s/tmpnam.XXXXXXXXXX", get_tmpdir()) < 0) {
    return 0;
  }
  return mktemp(s);
}

char* tempnam(const char* dir, const char* prefix) {
  char* path;

  if (getenv("TMPDIR") != 0) {
    dir = getenv("TMPDIR");
  }
  if (dir == 0) {
    dir = "/data/local/tmp";
  }
  if (prefix == 0) {
    prefix = "tempnam.";
  }
  if (asprintf(&path, "%s/%sXXXXXXXXXX", dir, prefix) < 0) {
    return 0;
  }
  if (mktemp(path) == 0) {
    free(path);
    return 0;
  }
  return path;
}

char* ctermid(char* s) {
  if (s == 0) {
    return (char*)_PATH_TTY;
  }
  strcpy(s, _PATH_TTY);
  return s;
}

static int parse_memory_mode(
    const char* mode,
    int* readable,
    int* writable,
    int* append,
    int* truncate) {
  if (mode == 0 || mode[0] == 0) {
    errno = EINVAL;
    return -1;
  }
  *readable = mode[0] == 'r';
  *writable = mode[0] == 'w' || mode[0] == 'a';
  *append = mode[0] == 'a';
  *truncate = mode[0] == 'w';
  if (mode[0] != 'r' && mode[0] != 'w' && mode[0] != 'a') {
    errno = EINVAL;
    return -1;
  }
  for (++mode; *mode != 0; ++mode) {
    if (*mode == '+') {
      *readable = 1;
      *writable = 1;
    }
  }
  return 0;
}

static FILE* create_memory_stream(
    char* base,
    size_t size,
    int base_owned,
    int readable,
    int writable,
    int append,
    size_t len,
    char** open_ptr,
    size_t* open_size) {
  FILE* stream = (FILE*)malloc(sizeof(FILE));
  struct crt_stdio_cookie* cookie;

  if (stream == 0) {
    errno = ENOMEM;
    return 0;
  }
  init_stream(stream, -1, 1, readable, writable, append);
  if (stream->_cookie == 0) {
    free(stream);
    errno = ENOMEM;
    return 0;
  }
  stream->_flags = __SSTR;
  if (readable && writable) {
    stream->_flags |= __SRW;
  } else if (readable) {
    stream->_flags |= __SRD;
  } else if (writable) {
    stream->_flags |= __SWR;
  }
  if (open_ptr != 0) {
    stream->_flags |= __SALC;
  }
  cookie = stream_cookie(stream);
  if (cookie == 0) {
    free(stream);
    errno = ENOMEM;
    return 0;
  }
  cookie->kind = CRT_STDIO_KIND_MEMORY;
  cookie->fd = -1;
  cookie->readable = readable;
  cookie->writable = writable;
  cookie->append = append;
  cookie->mem_base = base;
  cookie->mem_size = size;
  cookie->mem_pos = append ? len : 0;
  cookie->mem_len = len;
  cookie->mem_owned = base_owned;
  cookie->mem_open_ptr = open_ptr;
  cookie->mem_open_size = open_size;
  if (cookie->mem_len < cookie->mem_size) {
    cookie->mem_base[cookie->mem_len] = 0;
  }
  sync_memory_stream(stream);
  register_stream(stream);
  return stream;
}

FILE* fmemopen(void* buf, size_t size, const char* mode) {
  int readable;
  int writable;
  int append;
  int truncate;
  char* base = (char*)buf;
  int owned = 0;
  size_t len;

  if (parse_memory_mode(mode, &readable, &writable, &append, &truncate) != 0) {
    return 0;
  }
  if (buf == 0) {
    base = size == 0 ? 0 : (char*)malloc(size);
    if (size != 0 && base == 0) {
      errno = ENOMEM;
      return 0;
    }
    owned = 1;
    if (size != 0) {
      base[0] = 0;
    }
  }
  if (truncate || buf == 0) {
    len = 0;
  } else if (mode[0] == 'r') {
    len = size;
  } else {
    len = bounded_strlen(base, size);
  }
  return create_memory_stream(base, size, owned, readable, writable, append, len, 0, 0);
}

FILE* open_memstream(char** ptr, size_t* sizep) {
  char* base;

  if (ptr == 0 || sizep == 0) {
    errno = EINVAL;
    return 0;
  }
  base = (char*)malloc(1);
  if (base == 0) {
    errno = ENOMEM;
    return 0;
  }
  base[0] = 0;
  *ptr = base;
  *sizep = 0;
  return create_memory_stream(base, 1, 1, 0, 1, 0, 0, ptr, sizep);
}

FILE* funopen(const void* cookie_arg,
              int (*read_fn)(void*, char*, int),
              int (*write_fn)(void*, const char*, int),
              fpos_t (*seek_fn)(void*, fpos_t, int),
              int (*close_fn)(void*)) {
  FILE* stream;
  struct crt_stdio_cookie* cookie;

  if (read_fn == 0 && write_fn == 0) {
    errno = EINVAL;
    return 0;
  }
  stream = (FILE*)malloc(sizeof(FILE));
  if (stream == 0) {
    errno = ENOMEM;
    return 0;
  }
  init_stream(stream, -1, 1, read_fn != 0, write_fn != 0, 0);
  if (stream->_cookie == 0) {
    free(stream);
    errno = ENOMEM;
    return 0;
  }
  cookie = stream_cookie(stream);
  if (cookie == 0) {
    free(stream);
    errno = ENOMEM;
    return 0;
  }
  cookie->kind = CRT_STDIO_KIND_FUNOPEN;
  cookie->user_cookie = (void*)cookie_arg;
  cookie->user_read = read_fn;
  cookie->user_write = write_fn;
  cookie->user_seek = seek_fn;
  cookie->user_close = close_fn;
  stream->_file = -1;
  stream->_close = funopen_cookie_close;
  stream->_read = read_fn != 0 ? funopen_cookie_read : 0;
  stream->_seek = seek_fn != 0 ? funopen_cookie_seek : 0;
  stream->_write = write_fn != 0 ? funopen_cookie_write : 0;
  if (read_fn != 0 && write_fn != 0) {
    stream->_flags = __SRW;
  } else if (read_fn != 0) {
    stream->_flags = __SRD;
  } else {
    stream->_flags = __SWR;
  }
  register_stream(stream);
  return stream;
}

int fclose(FILE* stream) {
  struct crt_stdio_cookie* cookie;
  int result = 0;

  if (!stream_valid(stream)) {
    return EOF;
  }
  cookie = stream_cookie(stream);
  if (cookie == 0) {
    return EOF;
  }
  if (fflush(stream) != 0) {
    result = -1;
  }
  if (cookie->kind == CRT_STDIO_KIND_MEMORY) {
    unregister_stream(stream);
    if (cookie->mem_open_ptr != 0) {
      cookie->mem_owned = 0;
    }
    if (cookie->mem_owned) {
      free(cookie->mem_base);
    }
    if ((stream->_flags & __SMBF) != 0) {
      free(stream->_bf._base);
    }
    free_extension_buffers(stream);
    if (cookie->ext_owned && stream_ext(stream) != 0) {
      pthread_mutex_destroy(&stream_ext(stream)->_lock);
      free(stream->_ext._base);
    }
    free(cookie);
    free(stream);
    return result == 0 ? 0 : EOF;
  }
  if (cookie->owned) {
    if (stream->_close(stream->_cookie) != 0) {
      result = -1;
    }
    unregister_stream(stream);
    if ((stream->_flags & __SMBF) != 0) {
      free(stream->_bf._base);
    }
    free_extension_buffers(stream);
    if (cookie->ext_owned && stream_ext(stream) != 0) {
      pthread_mutex_destroy(&stream_ext(stream)->_lock);
      free(stream->_ext._base);
    }
    free(cookie);
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
  struct crt_stdio_cookie* cookie = stream_cookie(stream);
  off_t adjusted = offset;
  off_t base;
  off_t target;

  if (cookie == 0) {
    return -1;
  }
  if (cookie->kind == CRT_STDIO_KIND_MEMORY) {
    if (whence == SEEK_SET) {
      base = 0;
    } else if (whence == SEEK_CUR) {
      base = (off_t)cookie->mem_pos;
      if (stream->_ur != 0) {
        --base;
      }
    } else if (whence == SEEK_END) {
      base = (off_t)cookie->mem_len;
    } else {
      errno = EINVAL;
      set_stream_error(stream);
      return -1;
    }
    target = base + offset;
    if (target < 0) {
      errno = EINVAL;
      set_stream_error(stream);
      return -1;
    }
    if ((size_t)target > cookie->mem_size) {
      if (cookie->mem_open_ptr == 0) {
        errno = EINVAL;
        set_stream_error(stream);
        return -1;
      }
      if (ensure_memory_capacity(stream, (size_t)target + 1) != 0) {
        return -1;
      }
    }
    if (cookie->mem_open_ptr != 0 && (size_t)target > cookie->mem_len) {
      memset(cookie->mem_base + cookie->mem_len, 0, (size_t)target - cookie->mem_len);
      cookie->mem_len = (size_t)target;
      if (cookie->mem_len < cookie->mem_size) {
        cookie->mem_base[cookie->mem_len] = 0;
      }
      sync_memory_stream(stream);
    }
    cookie->mem_pos = (size_t)target;
    clear_stream_eof(stream);
    reset_buffer_state(stream);
    stream->_flags &= ~(__SRD | __SWR);
    return 0;
  }
  if (stream->_seek == 0) {
    errno = ESPIPE;
    set_stream_error(stream);
    return -1;
  }
  if (stream_last_op(stream) == CRT_STDIO_WRITE && fflush(stream) != 0) {
    return -1;
  }
  if (stream_last_op(stream) == CRT_STDIO_READ && whence == SEEK_CUR) {
    adjusted -= (off_t)stream->_r;
    if (stream->_ur != 0) {
      adjusted -= (off_t)stream->_ur;
    }
  }
  if (stream->_seek(stream->_cookie, adjusted, whence) < 0) {
    set_stream_error(stream);
    return -1;
  }
  clear_stream_eof(stream);
  reset_buffer_state(stream);
  stream->_flags &= ~(__SRD | __SWR);
  return 0;
}

static off_t stream_tell(FILE* stream) {
  struct crt_stdio_cookie* cookie = stream_cookie(stream);
  off_t result;

  if (cookie == 0) {
    return (off_t)-1;
  }
  if (cookie->kind == CRT_STDIO_KIND_MEMORY) {
    result = (off_t)cookie->mem_pos;
    if (stream->_ur != 0) {
      result -= (off_t)stream->_ur;
    }
    return result;
  }
  if (stream->_seek == 0) {
    errno = ESPIPE;
    return (off_t)-1;
  }
  result = stream->_seek(stream->_cookie, 0, SEEK_CUR);
  if (result < 0) {
    return (off_t)-1;
  }
  if (stream_last_op(stream) == CRT_STDIO_WRITE && stream->_p != 0 && stream->_bf._base != 0) {
    result += (off_t)(stream->_p - stream->_bf._base);
  } else if (stream_last_op(stream) == CRT_STDIO_READ) {
    result -= (off_t)stream->_r;
    if (stream->_ur != 0) {
      result -= (off_t)stream->_ur;
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

int __srefill(FILE* stream) {
  ssize_t result;

  if (!stream_valid(stream)) {
    return EOF;
  }
  if (is_memory_stream(stream)) {
    struct crt_stdio_cookie* cookie = stream_cookie(stream);

    if (cookie == 0 || !cookie->readable) {
      errno = EBADF;
      set_stream_error(stream);
      return EOF;
    }
    if (cookie->mem_pos >= cookie->mem_len) {
      set_stream_eof(stream);
      return EOF;
    }
    return 0;
  }
  if (prepare_read(stream) != 0) {
    return EOF;
  }
  set_byte_orientation(stream);
  if (stream_buffering_mode(stream) == _IONBF) {
    stream->_bf._base = stream->_nbuf;
    stream->_bf._size = sizeof(stream->_nbuf);
  } else if (ensure_buffer(stream) != 0) {
    return EOF;
  }
  result = stream->_read(stream->_cookie, (char*)stream->_bf._base, (int)stream->_bf._size);
  if (result <= 0) {
    if (result == 0) {
      set_stream_eof(stream);
    } else {
      set_stream_error(stream);
    }
    stream->_r = 0;
    stream->_p = stream->_bf._base;
    return EOF;
  }
  stream->_p = stream->_bf._base;
  stream->_r = (int)result;
  clear_stream_eof(stream);
  sync_bionic_buffer_fields(stream);
  return 0;
}

int __srget(FILE* stream) {
  unsigned char byte;

  if (is_memory_stream(stream)) {
    return memory_getc(stream);
  }
  if (__srefill(stream) != 0) {
    return EOF;
  }
  if (stream->_r <= 0 || stream->_p == 0) {
    set_stream_eof(stream);
    return EOF;
  }
  byte = *stream->_p++;
  --stream->_r;
  sync_bionic_buffer_fields(stream);
  return byte;
}

int __swsetup(FILE* stream) {
  if (!stream_valid(stream) || prepare_write(stream) != 0) {
    return EOF;
  }
  set_byte_orientation(stream);
  if (stream_buffering_mode(stream) == _IONBF) {
    stream->_bf._base = stream->_nbuf;
    stream->_bf._size = sizeof(stream->_nbuf);
    stream->_p = stream->_bf._base;
    stream->_w = 0;
    return 0;
  }
  if (ensure_buffer(stream) != 0) {
    return EOF;
  }
  if (stream->_p == 0) {
    stream->_p = stream->_bf._base;
  }
  stream->_w = stream->_bf._size >= (size_t)(stream->_p - stream->_bf._base) ?
      (int)(stream->_bf._size - (size_t)(stream->_p - stream->_bf._base)) : 0;
  sync_bionic_buffer_fields(stream);
  return 0;
}

int __sflush(FILE* stream) {
  if (!stream_valid(stream)) {
    return EOF;
  }
  if (is_memory_stream(stream)) {
    sync_memory_stream(stream);
    reset_buffer_state(stream);
    return 0;
  }
  if (stream_last_op(stream) == CRT_STDIO_WRITE) {
    return flush_write_buffer(stream) == 0 ? 0 : EOF;
  }
  if (stream_last_op(stream) == CRT_STDIO_READ) {
    return discard_read_buffer(stream) == 0 ? 0 : EOF;
  }
  return 0;
}

int __swbuf(int c, FILE* stream) {
  unsigned char byte = (unsigned char)c;

  if (!stream_valid(stream)) {
    return EOF;
  }
  if (is_memory_stream(stream)) {
    return memory_putc(stream, c);
  }
  if (__swsetup(stream) != 0) {
    return EOF;
  }
  if (stream_buffering_mode(stream) == _IONBF) {
    if (stream->_write(stream->_cookie, (const char*)&byte, 1) != 1) {
      set_stream_error(stream);
      return EOF;
    }
    return byte;
  }
  if (stream->_p == 0 || stream->_bf._base == 0 || stream->_bf._size == 0) {
    errno = EBADF;
    set_stream_error(stream);
    return EOF;
  }
  if ((size_t)(stream->_p - stream->_bf._base) == stream->_bf._size && flush_write_buffer(stream) != 0) {
    return EOF;
  }
  *stream->_p++ = byte;
  stream->_w = stream->_bf._size >= (size_t)(stream->_p - stream->_bf._base) ?
      (int)(stream->_bf._size - (size_t)(stream->_p - stream->_bf._base)) : 0;
  sync_bionic_buffer_fields(stream);
  if ((size_t)(stream->_p - stream->_bf._base) == stream->_bf._size ||
      (stream_buffering_mode(stream) == _IOLBF && byte == '\n')) {
    if (flush_write_buffer(stream) != 0) {
      return EOF;
    }
  }
  return byte;
}

static int fputc_unlocked_impl(int c, FILE* stream) {
  unsigned char byte = (unsigned char)c;

  if (!stream_valid(stream)) {
    return EOF;
  }
  if (is_memory_stream(stream)) {
    return memory_putc(stream, c);
  }
  if (stream_last_op(stream) == CRT_STDIO_WRITE &&
      stream_buffering_mode(stream) != _IONBF &&
      stream->_p != 0 &&
      stream->_bf._base != 0 &&
      stream->_w > 0) {
    --stream->_w;
    *stream->_p++ = byte;
    if (stream_buffering_mode(stream) == _IOLBF && byte == '\n') {
      return __sflush(stream) == 0 ? byte : EOF;
    }
    return byte;
  }
  return __swbuf(c, stream);
}

int fputc(int c, FILE* stream) {
  int result;

  lock_stream_if_needed(stream);
  result = fputc_unlocked_impl(c, stream);
  unlock_stream_if_needed(stream);
  return result;
}

int putc(int c, FILE* stream) {
  return fputc(c, stream);
}

static int fputs_unlocked_impl(const char* s, FILE* stream) {
  size_t i;
  size_t length;

  if (!stream_valid(stream)) {
    return EOF;
  }
  length = strlen(s);
  for (i = 0; i < length; ++i) {
    if (fputc_unlocked_impl((unsigned char)s[i], stream) == EOF) {
      return EOF;
    }
  }
  return 0;
}

int fputs(const char* s, FILE* stream) {
  int result;

  lock_stream_if_needed(stream);
  result = fputs_unlocked_impl(s, stream);
  unlock_stream_if_needed(stream);
  return result;
}

int puts(const char* s) {
  int result = 0;

  lock_stream_if_needed(stdout);
  if (fputs_unlocked_impl(s, stdout) == EOF) {
    result = EOF;
  } else if (fputc_unlocked_impl('\n', stdout) == EOF) {
    result = EOF;
  }
  unlock_stream_if_needed(stdout);
  return result;
}

int putchar(int c) {
  return fputc(c, stdout);
}

static int fgetc_unlocked_impl(FILE* stream) {
  if (!stream_valid(stream)) {
    return EOF;
  }
  if (stream->_ur != 0) {
    return pop_ungetc(stream);
  }
  if (is_memory_stream(stream)) {
    return memory_getc(stream);
  }
  if (stream_last_op(stream) == CRT_STDIO_READ && stream->_r > 0 && stream->_p != 0) {
    unsigned char byte;

    clear_stream_eof(stream);
    byte = *stream->_p++;
    --stream->_r;
    sync_bionic_buffer_fields(stream);
    return byte;
  }
  return __srget(stream);
}

int fgetc(FILE* stream) {
  int result;

  lock_stream_if_needed(stream);
  result = fgetc_unlocked_impl(stream);
  unlock_stream_if_needed(stream);
  return result;
}

static char* fgets_unlocked_impl(char* s, int size, FILE* stream) {
  int i = 0;

  if (s == 0 || size <= 0) {
    errno = EINVAL;
    return 0;
  }
  while (i < size - 1) {
    int ch = fgetc_unlocked_impl(stream);
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

char* fgets(char* s, int size, FILE* stream) {
  char* result;

  lock_stream_if_needed(stream);
  result = fgets_unlocked_impl(s, size, stream);
  unlock_stream_if_needed(stream);
  return result;
}

ssize_t getdelim(char** lineptr, size_t* n, int delimiter, FILE* stream) {
  size_t pos = 0;
  int ch;

  lock_stream_if_needed(stream);
  if (lineptr == 0 || n == 0 || !stream_valid(stream)) {
    unlock_stream_if_needed(stream);
    errno = EINVAL;
    return -1;
  }
  if (*lineptr == 0 || *n == 0) {
    *n = 128;
    *lineptr = (char*)malloc(*n);
    if (*lineptr == 0) {
      *n = 0;
      unlock_stream_if_needed(stream);
      return -1;
    }
  }

  while ((ch = fgetc_unlocked_impl(stream)) != EOF) {
    if (pos + 1 >= *n) {
      size_t new_size = *n * 2;
      char* new_line;

      if (new_size <= *n) {
        errno = ENOMEM;
        unlock_stream_if_needed(stream);
        return -1;
      }
      new_line = (char*)realloc(*lineptr, new_size);
      if (new_line == 0) {
        unlock_stream_if_needed(stream);
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
  unlock_stream_if_needed(stream);
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
  struct crt_stdio_cookie* cookie;
  struct __sfileext* ext;
  unsigned char* base;
  size_t capacity;

  lock_stream_if_needed(stream);
  if (!stream_valid(stream) || c == EOF) {
    unlock_stream_if_needed(stream);
    return EOF;
  }
  cookie = stream_cookie(stream);
  if (cookie == 0 || !cookie->readable) {
    errno = EBADF;
    set_stream_error(stream);
    unlock_stream_if_needed(stream);
    return EOF;
  }
  ext = stream_ext(stream);
  base = stream->_ubuf;
  capacity = sizeof(stream->_ubuf);
  if (ext != 0 && ext->_ub._base != 0) {
    base = ext->_ub._base;
    capacity = ext->_ub._size;
  }
  if (stream->_ur == 0) {
    stream->_up = base + capacity;
  }
  if ((size_t)stream->_ur == capacity) {
    size_t new_capacity = capacity < 8 ? 8 : capacity * 2;
    unsigned char* grown = (unsigned char*)malloc(new_capacity);
    size_t i;

    if (grown == 0) {
      unlock_stream_if_needed(stream);
      return EOF;
    }
    for (i = 0; i < (size_t)stream->_ur; ++i) {
      grown[new_capacity - (size_t)stream->_ur + i] = stream->_up[i];
    }
    if (ext != 0 && ext->_ub._base != 0) {
      free(ext->_ub._base);
    }
    if (ext == 0) {
      free(grown);
      unlock_stream_if_needed(stream);
      return EOF;
    }
    ext->_ub._base = grown;
    ext->_ub._size = new_capacity;
    stream->_up = grown + new_capacity - stream->_ur;
    base = ext->_ub._base;
  }
  (void)base;
  *--stream->_up = (unsigned char)c;
  ++stream->_ur;
  clear_stream_eof(stream);
  unlock_stream_if_needed(stream);
  return (unsigned char)c;
}

static size_t fread_unlocked_impl(void* ptr, size_t size, size_t nmemb, FILE* stream) {
  size_t total;
  size_t done = 0;
  unsigned char* out = (unsigned char*)ptr;

  if (!stream_valid(stream) || size == 0 || nmemb == 0) {
    return 0;
  }
  if (nmemb > ((size_t)-1) / size) {
    errno = EINVAL;
    return 0;
  }

  total = size * nmemb;
  while (done < total) {
    int ch = fgetc_unlocked_impl(stream);
    if (ch == EOF) {
      break;
    }
    out[done++] = (unsigned char)ch;
  }
  return done / size;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
  size_t result;

  lock_stream_if_needed(stream);
  result = fread_unlocked_impl(ptr, size, nmemb, stream);
  unlock_stream_if_needed(stream);
  return result;
}

static size_t fwrite_unlocked_impl(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
  size_t total;
  size_t done = 0;
  const unsigned char* in = (const unsigned char*)ptr;

  if (!stream_valid(stream) || size == 0 || nmemb == 0) {
    return 0;
  }
  if (nmemb > ((size_t)-1) / size) {
    errno = EINVAL;
    return 0;
  }

  total = size * nmemb;
  while (done < total) {
    if (fputc_unlocked_impl(in[done], stream) == EOF) {
      break;
    }
    ++done;
  }
  return done / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
  size_t result;

  lock_stream_if_needed(stream);
  result = fwrite_unlocked_impl(ptr, size, nmemb, stream);
  unlock_stream_if_needed(stream);
  return result;
}

static int fflush_unlocked_impl(FILE* stream) {
  if (stream == 0) {
    int result = 0;
    struct crt_stdio_cookie* current;

    if (fflush_unlocked_impl(stdout) != 0) {
      result = EOF;
    }
    if (fflush_unlocked_impl(stderr) != 0) {
      result = EOF;
    }
    current = open_streams;
    while (current != 0) {
      if (fflush_unlocked_impl(current->stream) != 0) {
        result = EOF;
      }
      current = current->next_open;
    }
    return result;
  }
  if (!stream_valid(stream)) {
    return EOF;
  }
  return __sflush(stream);
}

int fflush(FILE* stream) {
  int result;

  if (stream == 0) {
    lock_stream_if_needed(stdout);
    lock_stream_if_needed(stderr);
    result = fflush_unlocked_impl(0);
    unlock_stream_if_needed(stderr);
    unlock_stream_if_needed(stdout);
    return result;
  }
  lock_stream_if_needed(stream);
  result = fflush_unlocked_impl(stream);
  unlock_stream_if_needed(stream);
  return result;
}

void setbuf(FILE* stream, char* buf) {
  (void)setvbuf(stream, buf, buf != 0 ? _IOFBF : _IONBF, BUFSIZ);
}

int setvbuf(FILE* stream, char* buf, int mode, size_t size) {
  if (!stream_valid(stream)) {
    return EOF;
  }
  if (mode != _IOFBF && mode != _IOLBF && mode != _IONBF) {
    errno = EINVAL;
    return EOF;
  }
  if (fflush(stream) != 0) {
    return EOF;
  }
  if ((stream->_flags & __SMBF) != 0) {
    free(stream->_bf._base);
  }
  stream->_flags &= ~(__SMBF | __SNBF | __SLBF);
  if (mode == _IONBF) {
    stream->_flags |= __SNBF;
    stream->_bf._base = 0;
    stream->_bf._size = 0;
  } else {
    if (mode == _IOLBF) {
      stream->_flags |= __SLBF;
    }
    stream->_bf._base = size == 0 ? 0 : (unsigned char*)buf;
    stream->_bf._size = size;
  }
  reset_buffer_state(stream);
  stream->_flags &= ~(__SRD | __SWR);
  return 0;
}

int feof(FILE* stream) {
  if (!stream_valid(stream)) {
    return 0;
  }
  return (stream->_flags & __SEOF) != 0;
}

int ferror(FILE* stream) {
  if (!stream_valid(stream)) {
    return 1;
  }
  return (stream->_flags & __SERR) != 0;
}

void clearerr(FILE* stream) {
  if (stream == 0) {
    return;
  }
  stream->_flags &= ~(__SEOF | __SERR);
}

int fpurge(FILE* stream) {
  struct __sfileext* ext;

  if (!stream_valid(stream)) {
    return EOF;
  }
  ext = stream_ext(stream);
  if (ext != 0 && ext->_ub._base != 0) {
    free(ext->_ub._base);
    ext->_ub._base = 0;
    ext->_ub._size = 0;
  }
  if (is_memory_stream(stream)) {
    reset_buffer_state(stream);
    return 0;
  }
  stream->_r = 0;
  stream->_w = 0;
  stream->_p = stream->_bf._base;
  stream->_ur = 0;
  stream->_up = 0;
  stream->_flags &= ~(__SRD | __SWR);
  return 0;
}

static char* fgetln_unlocked_impl(FILE* stream, size_t* lengthp) {
  size_t used = 0;
  int ch;

  if (lengthp == 0 || !stream_valid(stream)) {
    errno = EINVAL;
    return 0;
  }
  *lengthp = 0;
  while ((ch = fgetc(stream)) != EOF) {
    if (used == stream->_lb._size) {
      size_t new_size = stream->_lb._size == 0 ? 128 : stream->_lb._size * 2;
      unsigned char* grown;

      if (new_size <= stream->_lb._size) {
        errno = ENOMEM;
        set_stream_error(stream);
        return 0;
      }
      grown = (unsigned char*)realloc(stream->_lb._base, new_size);
      if (grown == 0) {
        set_stream_error(stream);
        return 0;
      }
      stream->_lb._base = grown;
      stream->_lb._size = new_size;
    }
    stream->_lb._base[used++] = (unsigned char)ch;
    if (ch == '\n') {
      break;
    }
  }
  if (used == 0) {
    return 0;
  }
  *lengthp = used;
  return (char*)stream->_lb._base;
}

char* fgetln(FILE* stream, size_t* lengthp) {
  char* result;

  lock_stream_if_needed(stream);
  result = fgetln_unlocked_impl(stream, lengthp);
  unlock_stream_if_needed(stream);
  return result;
}

size_t __fbufsize(FILE* stream) {
  if (!stream_valid(stream)) {
    return 0;
  }
  return stream->_bf._size;
}

int __freadable(FILE* stream) {
  struct crt_stdio_cookie* cookie = stream_cookie(stream);

  return cookie != 0 && cookie->readable;
}

int __freading(FILE* stream) {
  if (!stream_valid(stream)) {
    return 0;
  }
  return (stream->_flags & __SRD) != 0;
}

int __fwritable(FILE* stream) {
  struct crt_stdio_cookie* cookie = stream_cookie(stream);

  return cookie != 0 && cookie->writable;
}

int __fwriting(FILE* stream) {
  if (!stream_valid(stream)) {
    return 0;
  }
  return (stream->_flags & __SWR) != 0;
}

int __flbf(FILE* stream) {
  if (!stream_valid(stream)) {
    return 0;
  }
  return (stream->_flags & __SLBF) != 0;
}

void __fpurge(FILE* stream) {
  (void)fpurge(stream);
}

size_t __fpending(FILE* stream) {
  if (!stream_valid(stream) || stream->_p == 0 || stream->_bf._base == 0 || stream->_p < stream->_bf._base) {
    return 0;
  }
  if (stream_last_op(stream) != CRT_STDIO_WRITE) {
    return 0;
  }
  return (size_t)(stream->_p - stream->_bf._base);
}

size_t __freadahead(FILE* stream) {
  if (!stream_valid(stream)) {
    return 0;
  }
  return (stream->_r > 0 ? (size_t)stream->_r : 0) + (stream->_ur > 0 ? (size_t)stream->_ur : 0);
}

void _flushlbf(void) {
  struct crt_stdio_cookie* current;

  if (__flbf(stdout)) {
    (void)fflush(stdout);
  }
  if (__flbf(stderr)) {
    (void)fflush(stderr);
  }
  current = open_streams;
  while (current != 0) {
    if (__flbf(current->stream)) {
      (void)fflush(current->stream);
    }
    current = current->next_open;
  }
}

void __fseterr(FILE* stream) {
  if (stream != 0) {
    stream->_flags |= __SERR;
  }
}

int __fsetlocking(FILE* stream, int type) {
  struct __sfileext* ext = stream_ext(stream);
  int old_state;

  if (ext == 0) {
    errno = EBADF;
    return FSETLOCKING_INTERNAL;
  }
  old_state = ext->_caller_handles_locking ? FSETLOCKING_BYCALLER : FSETLOCKING_INTERNAL;
  if (type == FSETLOCKING_QUERY) {
    return old_state;
  }
  if (type == FSETLOCKING_INTERNAL) {
    ext->_caller_handles_locking = 0;
  } else if (type == FSETLOCKING_BYCALLER) {
    ext->_caller_handles_locking = 1;
  }
  return old_state;
}

int feof_unlocked(FILE* stream) {
  if (!stream_valid(stream)) {
    return 0;
  }
  return (stream->_flags & __SEOF) != 0;
}

int ferror_unlocked(FILE* stream) {
  if (!stream_valid(stream)) {
    return 1;
  }
  return (stream->_flags & __SERR) != 0;
}

void clearerr_unlocked(FILE* stream) {
  if (stream != 0) {
    stream->_flags &= ~(__SEOF | __SERR);
  }
}

int fileno_unlocked(FILE* stream) {
  return fileno(stream);
}

int fflush_unlocked(FILE* stream) {
  return fflush_unlocked_impl(stream);
}

int fgetc_unlocked(FILE* stream) {
  return fgetc_unlocked_impl(stream);
}

int getc_unlocked(FILE* stream) {
  return fgetc_unlocked(stream);
}

int getchar_unlocked(void) {
  return fgetc_unlocked(stdin);
}

int fputc_unlocked(int c, FILE* stream) {
  return fputc_unlocked_impl(c, stream);
}

int putc_unlocked(int c, FILE* stream) {
  return fputc_unlocked(c, stream);
}

int putchar_unlocked(int c) {
  return fputc_unlocked(c, stdout);
}

char* fgets_unlocked(char* s, int size, FILE* stream) {
  return fgets_unlocked_impl(s, size, stream);
}

int fputs_unlocked(const char* s, FILE* stream) {
  return fputs_unlocked_impl(s, stream);
}

size_t fread_unlocked(void* ptr, size_t size, size_t nmemb, FILE* stream) {
  return fread_unlocked_impl(ptr, size, nmemb, stream);
}

size_t fwrite_unlocked(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
  return fwrite_unlocked_impl(ptr, size, nmemb, stream);
}

void flockfile(FILE* stream) {
  struct __sfileext* ext;
  pthread_t self;

  if (stream == 0) {
    return;
  }
  ext = stream_ext(stream);
  if (ext == 0) {
    return;
  }
  self = pthread_self();
  if (ext->_lock_count != 0 && pthread_equal(ext->_lock_owner, self)) {
    ++ext->_lock_count;
    return;
  }
  pthread_mutex_lock(&ext->_lock);
  ext->_lock_owner = self;
  ext->_lock_count = 1;
}

int ftrylockfile(FILE* stream) {
  struct __sfileext* ext;
  pthread_t self;
  int result;

  if (stream == 0) {
    errno = EBADF;
    return EBADF;
  }
  ext = stream_ext(stream);
  if (ext == 0) {
    errno = EBADF;
    return EBADF;
  }
  self = pthread_self();
  if (ext->_lock_count != 0 && pthread_equal(ext->_lock_owner, self)) {
    ++ext->_lock_count;
    return 0;
  }
  result = pthread_mutex_trylock(&ext->_lock);
  if (result != 0) {
    return result;
  }
  ext->_lock_owner = self;
  ext->_lock_count = 1;
  return 0;
}

void funlockfile(FILE* stream) {
  struct __sfileext* ext;

  if (stream == 0) {
    return;
  }
  ext = stream_ext(stream);
  if (ext == 0 || ext->_lock_count == 0) {
    return;
  }
  if (!pthread_equal(ext->_lock_owner, pthread_self())) {
    return;
  }
  --ext->_lock_count;
  if (ext->_lock_count == 0) {
    ext->_lock_owner = 0;
    pthread_mutex_unlock(&ext->_lock);
  }
}

void setbuffer(FILE* stream, char* buf, int size) {
  (void)setvbuf(stream, buf, buf != 0 ? _IOFBF : _IONBF, size < 0 ? 0 : (size_t)size);
}

int setlinebuf(FILE* stream) {
  return setvbuf(stream, 0, _IOLBF, 0);
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
