#ifndef CRT_STDLIB_H
#define CRT_STDLIB_H

#include <stddef.h>

#define MB_CUR_MAX 4

#ifdef __cplusplus
extern "C" {
#endif

void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
int atexit(void (*function)(void));
int atoi(const char* s);
long atol(const char* s);
long strtol(const char* nptr, char** endptr, int base);
unsigned long strtoul(const char* nptr, char** endptr, int base);
long long strtoll(const char* nptr, char** endptr, int base);
unsigned long long strtoull(const char* nptr, char** endptr, int base);
void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*));
void* bsearch(
    const void* key,
    const void* base,
    size_t nmemb,
    size_t size,
    int (*compar)(const void*, const void*));
char* getenv(const char* name);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
char* realpath(const char* path, char* resolved_path);
int mkstemp(char* template_path);
int mblen(const char* s, size_t n);
int mbtowc(wchar_t* pwc, const char* s, size_t n);
int wctomb(char* s, wchar_t wc);

#ifdef __cplusplus
}
#endif

#endif
