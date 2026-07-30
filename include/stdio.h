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
#define FOPEN_MAX 16

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#define L_tmpnam 20
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

typedef struct __crt_FILE FILE;
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
FILE* tmpfile(void);
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
void setbuf(FILE* stream, char* buf);
int setvbuf(FILE* stream, char* buf, int mode, size_t size);
int remove(const char* path);
int rename(const char* old_path, const char* new_path);
int printf(const char* format, ...);
int fprintf(FILE* stream, const char* format, ...);
int vfprintf(FILE* stream, const char* format, va_list ap);
int vprintf(const char* format, va_list ap);
int sprintf(char* s, const char* format, ...);
int vsprintf(char* s, const char* format, va_list ap);
int snprintf(char* s, size_t n, const char* format, ...);
int vsnprintf(char* s, size_t n, const char* format, va_list ap);
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
