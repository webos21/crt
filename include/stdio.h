#ifndef CRT_STDIO_H
#define CRT_STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EOF (-1)
#define BUFSIZ 1024
#define FILENAME_MAX 4096
#define FOPEN_MAX 20
#define L_tmpnam 4096
#define TMP_MAX 308915776
#define P_tmpdir "/tmp/"
#define L_ctermid 1024

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

struct __sFILE;
typedef struct __sFILE FILE;
typedef off_t fpos_t;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

int fputc(int c, FILE* stream);
int putc(int c, FILE* stream);
int fputs(const char* s, FILE* stream);
FILE* fopen(const char* path, const char* mode);
FILE* fdopen(int fd, const char* mode);
FILE* freopen(const char* path, const char* mode, FILE* stream);
FILE* popen(const char* command, const char* type);
int pclose(FILE* stream);
FILE* tmpfile(void);
char* tmpnam(char* s);
char* tempnam(const char* dir, const char* prefix);
char* ctermid(char* s);
int fclose(FILE* stream);
int fileno(FILE* stream);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
int fseeko(FILE* stream, off_t offset, int whence);
off_t ftello(FILE* stream);
void rewind(FILE* stream);
int fgetpos(FILE* stream, fpos_t* pos);
int fsetpos(FILE* stream, const fpos_t* pos);
int fgetc(FILE* stream);
char* fgets(char* s, int size, FILE* stream);
ssize_t getdelim(char** lineptr, size_t* n, int delimiter, FILE* stream);
ssize_t getline(char** lineptr, size_t* n, FILE* stream);
int getc(FILE* stream);
int getchar(void);
int ungetc(int c, FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
void clearerr(FILE* stream);
int feof_unlocked(FILE* stream);
int ferror_unlocked(FILE* stream);
void clearerr_unlocked(FILE* stream);
int fileno_unlocked(FILE* stream);
int fflush_unlocked(FILE* stream);
int fgetc_unlocked(FILE* stream);
int getc_unlocked(FILE* stream);
int getchar_unlocked(void);
int fputc_unlocked(int c, FILE* stream);
int putc_unlocked(int c, FILE* stream);
int putchar_unlocked(int c);
char* fgets_unlocked(char* s, int size, FILE* stream);
int fputs_unlocked(const char* s, FILE* stream);
size_t fread_unlocked(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite_unlocked(const void* ptr, size_t size, size_t nmemb, FILE* stream);
void flockfile(FILE* stream);
int ftrylockfile(FILE* stream);
void funlockfile(FILE* stream);
void setbuf(FILE* stream, char* buf);
int setvbuf(FILE* stream, char* buf, int mode, size_t size);
void setbuffer(FILE* stream, char* buf, int size);
int setlinebuf(FILE* stream);
FILE* fmemopen(void* buf, size_t size, const char* mode);
FILE* open_memstream(char** ptr, size_t* sizep);
FILE* funopen(const void* cookie,
              int (*read_fn)(void*, char*, int),
              int (*write_fn)(void*, const char*, int),
              fpos_t (*seek_fn)(void*, fpos_t, int),
              int (*close_fn)(void*));
#define fropen(cookie, fn) funopen(cookie, fn, 0, 0, 0)
#define fwopen(cookie, fn) funopen(cookie, 0, fn, 0, 0)
char* fgetln(FILE* stream, size_t* lengthp);
int fpurge(FILE* stream);
int remove(const char* path);
int rename(const char* old_path, const char* new_path);
int printf(const char* format, ...);
int fprintf(FILE* stream, const char* format, ...);
int vfprintf(FILE* stream, const char* format, va_list ap);
int dprintf(int fd, const char* format, ...);
int vdprintf(int fd, const char* format, va_list ap);
int vprintf(const char* format, va_list ap);
int sprintf(char* s, const char* format, ...);
int vsprintf(char* s, const char* format, va_list ap);
int snprintf(char* s, size_t n, const char* format, ...);
int vsnprintf(char* s, size_t n, const char* format, va_list ap);
int asprintf(char** strp, const char* format, ...);
int vasprintf(char** strp, const char* format, va_list ap);
int scanf(const char* format, ...);
int fscanf(FILE* stream, const char* format, ...);
int vfscanf(FILE* stream, const char* format, va_list ap);
int vscanf(const char* format, va_list ap);
int sscanf(const char* s, const char* format, ...);
int vsscanf(const char* s, const char* format, va_list ap);
int puts(const char* s);
int putchar(int c);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int fflush(FILE* stream);
void perror(const char* s);

#ifdef __cplusplus
}
#endif

#endif
