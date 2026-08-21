#ifndef CRT_XLOCALE_H
#define CRT_XLOCALE_H

#include <ctype.h>
#include <locale.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_CUR_MAX_L(locale) MB_CUR_MAX

float strtof_l(const char* nptr, char** endptr, locale_t locale);
double strtod_l(const char* nptr, char** endptr, locale_t locale);
long double strtold_l(const char* nptr, char** endptr, locale_t locale);
long long strtoll_l(const char* nptr, char** endptr, int base, locale_t locale);
unsigned long long strtoull_l(const char* nptr, char** endptr, int base, locale_t locale);

int isalnum_l(int ch, locale_t locale);
int isalpha_l(int ch, locale_t locale);
int isblank_l(int ch, locale_t locale);
int iscntrl_l(int ch, locale_t locale);
int isdigit_l(int ch, locale_t locale);
int isgraph_l(int ch, locale_t locale);
int islower_l(int ch, locale_t locale);
int isprint_l(int ch, locale_t locale);
int ispunct_l(int ch, locale_t locale);
int isspace_l(int ch, locale_t locale);
int isupper_l(int ch, locale_t locale);
int isxdigit_l(int ch, locale_t locale);
int toupper_l(int ch, locale_t locale);
int tolower_l(int ch, locale_t locale);

int strcoll_l(const char* s1, const char* s2, locale_t locale);
size_t strxfrm_l(char* dst, const char* src, size_t n, locale_t locale);

int iswctype_l(wint_t wc, wctype_t desc, locale_t locale);
int iswalnum_l(wint_t wc, locale_t locale);
int iswgraph_l(wint_t wc, locale_t locale);
int iswspace_l(wint_t wc, locale_t locale);
int iswprint_l(wint_t wc, locale_t locale);
int iswcntrl_l(wint_t wc, locale_t locale);
int iswupper_l(wint_t wc, locale_t locale);
int iswlower_l(wint_t wc, locale_t locale);
int iswalpha_l(wint_t wc, locale_t locale);
int iswblank_l(wint_t wc, locale_t locale);
int iswdigit_l(wint_t wc, locale_t locale);
int iswpunct_l(wint_t wc, locale_t locale);
int iswxdigit_l(wint_t wc, locale_t locale);
wint_t towupper_l(wint_t wc, locale_t locale);
wint_t towlower_l(wint_t wc, locale_t locale);
int wcscoll_l(const wchar_t* s1, const wchar_t* s2, locale_t locale);
size_t wcsxfrm_l(wchar_t* dst, const wchar_t* src, size_t n, locale_t locale);

size_t strftime_l(char* s, size_t max, const char* format, const struct tm* tm, locale_t locale);
wint_t btowc_l(int c, locale_t locale);
int wctob_l(wint_t c, locale_t locale);
size_t wcsnrtombs_l(
    char* dst, const wchar_t** src, size_t src_len, size_t dst_len, mbstate_t* ps, locale_t locale);
size_t wcrtomb_l(char* s, wchar_t wc, mbstate_t* ps, locale_t locale);
size_t mbsnrtowcs_l(
    wchar_t* dst, const char** src, size_t src_len, size_t dst_len, mbstate_t* ps, locale_t locale);
size_t mbrtowc_l(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps, locale_t locale);
int mbtowc_l(wchar_t* pwc, const char* s, size_t n, locale_t locale);
size_t mbrlen_l(const char* s, size_t n, mbstate_t* ps, locale_t locale);
size_t mbsrtowcs_l(wchar_t* dst, const char** src, size_t len, mbstate_t* ps, locale_t locale);

int snprintf_l(char* s, size_t n, locale_t locale, const char* format, ...);
int asprintf_l(char** strp, locale_t locale, const char* format, ...);
int sscanf_l(const char* s, locale_t locale, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif
