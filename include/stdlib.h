#ifndef CRT_STDLIB_H
#define CRT_STDLIB_H

#include <stddef.h>
#include <locale.h>

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
int posix_memalign(void** memptr, size_t alignment, size_t size);
void* aligned_alloc(size_t alignment, size_t size);
int system(const char* command);

#define RAND_MAX 0x7fffffff
typedef struct {
  int quot;
  int rem;
} div_t;

typedef struct {
  long quot;
  long rem;
} ldiv_t;

typedef struct {
  long long quot;
  long long rem;
} lldiv_t;

int rand(void);
void srand(unsigned int seed);
long random(void);
void srandom(unsigned int seed);
void exit(int status) __attribute__((noreturn));
void _Exit(int status) __attribute__((noreturn));
void quick_exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
int atexit(void (*function)(void));
int at_quick_exit(void (*function)(void));
int atoi(const char* s);
long atol(const char* s);
long long atoll(const char* s);
int abs(int n);
long labs(long n);
long long llabs(long long n);
div_t div(int numerator, int denominator);
ldiv_t ldiv(long numerator, long denominator);
lldiv_t lldiv(long long numerator, long long denominator);
double atof(const char* s);
double strtod(const char* nptr, char** endptr);
float strtof(const char* nptr, char** endptr);
long double strtold(const char* nptr, char** endptr);
long strtol(const char* nptr, char** endptr, int base);
unsigned long strtoul(const char* nptr, char** endptr, int base);
long long strtoll(const char* nptr, char** endptr, int base);
unsigned long long strtoull(const char* nptr, char** endptr, int base);
float strtof_l(const char* nptr, char** endptr, locale_t locale);
double strtod_l(const char* nptr, char** endptr, locale_t locale);
long double strtold_l(const char* nptr, char** endptr, locale_t locale);
long long strtoll_l(const char* nptr, char** endptr, int base, locale_t locale);
unsigned long long strtoull_l(const char* nptr, char** endptr, int base, locale_t locale);
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
size_t mbstowcs(wchar_t* dst, const char* src, size_t n);
int mbtowc(wchar_t* pwc, const char* s, size_t n);
int wctomb(char* s, wchar_t wc);
size_t wcstombs(char* dst, const wchar_t* src, size_t n);

#ifdef __cplusplus
}
#endif

#endif
