#ifndef CRT_STDIO_H
#define CRT_STDIO_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EOF (-1)
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
int fputs(const char* s, FILE* stream);
FILE* fopen(const char* path, const char* mode);
int fclose(FILE* stream);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
void clearerr(FILE* stream);
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

#ifdef __cplusplus
}
#endif

#endif
