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
size_t mbrtowc(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps);
size_t wcrtomb(char* s, wchar_t wc, mbstate_t* ps);
size_t mbsrtowcs(wchar_t* dst, const char** src, size_t len, mbstate_t* ps);
size_t wcsrtombs(char* dst, const wchar_t** src, size_t len, mbstate_t* ps);
size_t mbstowcs(wchar_t* dst, const char* src, size_t len);
size_t wcstombs(char* dst, const wchar_t* src, size_t len);
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

size_t wcslen(const wchar_t* s);
size_t wcsnlen(const wchar_t* s, size_t maxlen);
int wcscmp(const wchar_t* s1, const wchar_t* s2);
int wcsncmp(const wchar_t* s1, const wchar_t* s2, size_t n);
wchar_t* wcscpy(wchar_t* dst, const wchar_t* src);
wchar_t* wcsncpy(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wcscat(wchar_t* dst, const wchar_t* src);
wchar_t* wcschr(const wchar_t* s, wchar_t c);
wchar_t* wcsrchr(const wchar_t* s, wchar_t c);
wchar_t* wcsstr(const wchar_t* s, const wchar_t* find);
wchar_t* wmemchr(const wchar_t* s, wchar_t c, size_t n);
int wmemcmp(const wchar_t* s1, const wchar_t* s2, size_t n);
wchar_t* wmemcpy(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wmemmove(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wmemset(wchar_t* dst, wchar_t c, size_t n);

#ifdef __cplusplus
}
#endif

#endif
