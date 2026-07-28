#ifndef CRT_STDIO_H
#define CRT_STDIO_H

#include <stdarg.h>
#include <stddef.h>

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
int fgetc(FILE* stream);
char* fgets(char* s, int size, FILE* stream);
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
int snprintf(char* s, size_t n, const char* format, ...);
int vsnprintf(char* s, size_t n, const char* format, va_list ap);
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
