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

#if defined(CRT_TARGET_OS_WINDOWS)
/* Real mingw-w64/MSVC CRT compatibility symbols, Windows only, declared
 * here rather than in <malloc.h> to match where a real Windows C library
 * puts them: MSVC's own <stdlib.h> declares _aligned_malloc/_aligned_free
 * directly (no separate <malloc.h> include needed), and LLVM's libc++/
 * libc++abi source relies on exactly that -- confirmed directly,
 * stdlib_new_delete.cpp/fallback_malloc.cpp/libcxx's own src/new.cpp all
 * call _aligned_malloc()/_aligned_free() under `#if defined(
 * _LIBCPP_WIN32API)`/`_WIN32` with only <cstdlib>/<new> included, never
 * <malloc.h> itself. This project's own <stdlib.h> only ever declared
 * the portable C11 aligned_alloc(), so building libc++abi/libc++ against
 * this project's headers on Windows failed outright with "use of
 * undeclared identifier '_aligned_malloc'" even after adding the
 * declarations to <malloc.h> (never reached, since nothing here
 * includes that header). Implementation deliberately routes through
 * posix_memalign(), not aligned_alloc(): the real _aligned_malloc()
 * contract (any size, power-of-two alignment) is looser than C11
 * aligned_alloc()'s (requires size to be a multiple of alignment), and
 * libc++/libc++abi's own operator new(size, align_val_t) callers do not
 * guarantee that relationship -- aligned_alloc() would wrongly EINVAL/
 * return null on a perfectly valid request. */
void* _aligned_malloc(size_t size, size_t alignment);
void _aligned_free(void* ptr);
#endif

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
