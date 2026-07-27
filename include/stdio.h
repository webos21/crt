#ifndef CRT_STDIO_H
#define CRT_STDIO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EOF (-1)

typedef struct __crt_FILE FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

int fputc(int c, FILE* stream);
int fputs(const char* s, FILE* stream);
int puts(const char* s);
int putchar(int c);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int fflush(FILE* stream);

#ifdef __cplusplus
}
#endif

#endif
