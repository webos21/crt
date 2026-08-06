#ifndef CRT_STDLIB_H
#define CRT_STDLIB_H

#include <stddef.h>

#define MB_CUR_MAX 4
#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#ifdef __cplusplus
extern "C" {
#endif

void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void* reallocarray(void* ptr, size_t nmemb, size_t size);
void exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
int atexit(void (*function)(void));
int atoi(const char* s);
long atol(const char* s);
long long atoll(const char* s);
int abs(int n);
long labs(long n);
double atof(const char* s);
double strtod(const char* nptr, char** endptr);
float strtof(const char* nptr, char** endptr);
long double strtold(const char* nptr, char** endptr);
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
int putenv(char* entry);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
char* realpath(const char* path, char* resolved_path);
char* mktemp(char* template_path);
int mkstemp(char* template_path);
char* mkdtemp(char* template_path);
int mblen(const char* s, size_t n);
int mbtowc(wchar_t* pwc, const char* s, size_t n);
int wctomb(char* s, wchar_t wc);

#ifdef __cplusplus
}
#endif

#endif
