#ifndef CRT_WCHAR_H
#define CRT_WCHAR_H

#include <stddef.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef __WINT_TYPE__ wint_t;
typedef struct {
  unsigned int codepoint;
  unsigned char expected;
  unsigned char seen;
} mbstate_t;

#define WCHAR_MAX __WCHAR_MAX__
#ifdef __WCHAR_UNSIGNED__
#define WCHAR_MIN 0U
#else
#define WCHAR_MIN (-WCHAR_MAX - 1)
#endif
#define WEOF ((wint_t)-1)

wint_t btowc(int c);
int wctob(wint_t c);
int mbsinit(const mbstate_t* ps);
size_t mbrlen(const char* s, size_t n, mbstate_t* ps);
size_t mbrtowc(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps);
size_t wcrtomb(char* s, wchar_t wc, mbstate_t* ps);
size_t mbsrtowcs(wchar_t* dst, const char** src, size_t len, mbstate_t* ps);
size_t mbsnrtowcs(wchar_t* dst, const char** src, size_t src_len, size_t dst_len, mbstate_t* ps);
size_t wcsrtombs(char* dst, const wchar_t** src, size_t len, mbstate_t* ps);
size_t wcsnrtombs(char* dst, const wchar_t** src, size_t src_len, size_t dst_len, mbstate_t* ps);
size_t mbstowcs(wchar_t* dst, const char* src, size_t len);
size_t wcstombs(char* dst, const wchar_t* src, size_t len);
size_t wcsftime(wchar_t* s, size_t max, const wchar_t* format, const struct tm* tm);
int mblen(const char* s, size_t n);
int mbtowc(wchar_t* pwc, const char* s, size_t n);
int wctomb(char* s, wchar_t wc);
wint_t fgetwc(FILE* stream);
wchar_t* fgetws(wchar_t* s, int size, FILE* stream);
wint_t fputwc(wchar_t wc, FILE* stream);
int fputws(const wchar_t* s, FILE* stream);
int fwide(FILE* stream, int mode);
wint_t getwc(FILE* stream);
wint_t getwchar(void);
wint_t putwc(wchar_t wc, FILE* stream);
wint_t putwchar(wchar_t wc);
wint_t ungetwc(wint_t wc, FILE* stream);
int fwprintf(FILE* stream, const wchar_t* format, ...);
int vfwprintf(FILE* stream, const wchar_t* format, va_list ap);
int wprintf(const wchar_t* format, ...);
int vwprintf(const wchar_t* format, va_list ap);
int swprintf(wchar_t* s, size_t n, const wchar_t* format, ...);
int vswprintf(wchar_t* s, size_t n, const wchar_t* format, va_list ap);
int fwscanf(FILE* stream, const wchar_t* format, ...);
int vfwscanf(FILE* stream, const wchar_t* format, va_list ap);
int wscanf(const wchar_t* format, ...);
int vwscanf(const wchar_t* format, va_list ap);
int swscanf(const wchar_t* s, const wchar_t* format, ...);
int vswscanf(const wchar_t* s, const wchar_t* format, va_list ap);
FILE* open_wmemstream(wchar_t** ptr, size_t* sizep);

size_t wcslen(const wchar_t* s);
size_t wcsnlen(const wchar_t* s, size_t maxlen);
int wcscmp(const wchar_t* s1, const wchar_t* s2);
int wcsncmp(const wchar_t* s1, const wchar_t* s2, size_t n);
wchar_t* wcpcpy(wchar_t* dst, const wchar_t* src);
wchar_t* wcpncpy(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wcscpy(wchar_t* dst, const wchar_t* src);
wchar_t* wcsncpy(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wcscat(wchar_t* dst, const wchar_t* src);
wchar_t* wcsncat(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wcschr(const wchar_t* s, wchar_t c);
wchar_t* wcsrchr(const wchar_t* s, wchar_t c);
wchar_t* wcspbrk(const wchar_t* s, const wchar_t* accept);
wchar_t* wcsstr(const wchar_t* s, const wchar_t* find);
size_t wcscspn(const wchar_t* s, const wchar_t* reject);
size_t wcsspn(const wchar_t* s, const wchar_t* accept);
wchar_t* wcstok(wchar_t* s, const wchar_t* delimiter, wchar_t** ptr);
int wcscasecmp(const wchar_t* s1, const wchar_t* s2);
int wcsncasecmp(const wchar_t* s1, const wchar_t* s2, size_t n);
int wcscoll(const wchar_t* s1, const wchar_t* s2);
size_t wcsxfrm(wchar_t* dst, const wchar_t* src, size_t n);
size_t wcslcpy(wchar_t* dst, const wchar_t* src, size_t n);
size_t wcslcat(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wcsdup(const wchar_t* s);
double wcstod(const wchar_t* s, wchar_t** endptr);
float wcstof(const wchar_t* s, wchar_t** endptr);
long double wcstold(const wchar_t* s, wchar_t** endptr);
long wcstol(const wchar_t* s, wchar_t** endptr, int base);
long long wcstoll(const wchar_t* s, wchar_t** endptr, int base);
unsigned long wcstoul(const wchar_t* s, wchar_t** endptr, int base);
unsigned long long wcstoull(const wchar_t* s, wchar_t** endptr, int base);
int wcwidth(wchar_t wc);
int wcswidth(const wchar_t* s, size_t n);
wchar_t* wmemchr(const wchar_t* s, wchar_t c, size_t n);
int wmemcmp(const wchar_t* s1, const wchar_t* s2, size_t n);
wchar_t* wmemcpy(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wmempcpy(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wmemmove(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wmemset(wchar_t* dst, wchar_t c, size_t n);

#ifdef __cplusplus
}
#endif

#endif
