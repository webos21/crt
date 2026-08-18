#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>
#include <xlocale.h>

float strtof_l(const char* nptr, char** endptr, locale_t locale) {
  (void)locale;
  return strtof(nptr, endptr);
}

double strtod_l(const char* nptr, char** endptr, locale_t locale) {
  (void)locale;
  return strtod(nptr, endptr);
}

long double strtold_l(const char* nptr, char** endptr, locale_t locale) {
  (void)locale;
  return strtold(nptr, endptr);
}

long long strtoll_l(const char* nptr, char** endptr, int base, locale_t locale) {
  (void)locale;
  return strtoll(nptr, endptr, base);
}

unsigned long long strtoull_l(const char* nptr, char** endptr, int base, locale_t locale) {
  (void)locale;
  return strtoull(nptr, endptr, base);
}

#define CRT_CTYPE_L_WRAPPER(name) \
  int name##_l(int ch, locale_t locale) { \
    (void)locale; \
    return name(ch); \
  }

CRT_CTYPE_L_WRAPPER(isalnum)
CRT_CTYPE_L_WRAPPER(isalpha)
CRT_CTYPE_L_WRAPPER(isblank)
CRT_CTYPE_L_WRAPPER(iscntrl)
CRT_CTYPE_L_WRAPPER(isgraph)
CRT_CTYPE_L_WRAPPER(islower)
CRT_CTYPE_L_WRAPPER(isprint)
CRT_CTYPE_L_WRAPPER(ispunct)
CRT_CTYPE_L_WRAPPER(isspace)
CRT_CTYPE_L_WRAPPER(isupper)

int isdigit_l(int ch, locale_t locale) {
  (void)locale;
  return isdigit(ch);
}

int isxdigit_l(int ch, locale_t locale) {
  (void)locale;
  return isxdigit(ch);
}

int toupper_l(int ch, locale_t locale) {
  (void)locale;
  return toupper(ch);
}

int tolower_l(int ch, locale_t locale) {
  (void)locale;
  return tolower(ch);
}

int strcoll_l(const char* s1, const char* s2, locale_t locale) {
  (void)locale;
  return strcoll(s1, s2);
}

size_t strxfrm_l(char* dst, const char* src, size_t n, locale_t locale) {
  (void)locale;
  return strxfrm(dst, src, n);
}

int iswctype_l(wint_t wc, wctype_t desc, locale_t locale) {
  (void)locale;
  return iswctype(wc, desc);
}

int iswalnum_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswalnum(wc);
}

int iswgraph_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswgraph(wc);
}

int iswspace_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswspace(wc);
}

int iswprint_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswprint(wc);
}

int iswcntrl_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswcntrl(wc);
}

int iswupper_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswupper(wc);
}

int iswlower_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswlower(wc);
}

int iswalpha_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswalpha(wc);
}

int iswblank_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswblank(wc);
}

int iswdigit_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswdigit(wc);
}

int iswpunct_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswpunct(wc);
}

int iswxdigit_l(wint_t wc, locale_t locale) {
  (void)locale;
  return iswxdigit(wc);
}

wint_t towupper_l(wint_t wc, locale_t locale) {
  (void)locale;
  return towupper(wc);
}

wint_t towlower_l(wint_t wc, locale_t locale) {
  (void)locale;
  return towlower(wc);
}

int wcscoll_l(const wchar_t* s1, const wchar_t* s2, locale_t locale) {
  (void)locale;
  return wcscoll(s1, s2);
}

size_t wcsxfrm_l(wchar_t* dst, const wchar_t* src, size_t n, locale_t locale) {
  (void)locale;
  return wcsxfrm(dst, src, n);
}

size_t strftime_l(char* s, size_t max, const char* format, const struct tm* tm, locale_t locale) {
  (void)locale;
  return strftime(s, max, format, tm);
}

wint_t btowc_l(int c, locale_t locale) {
  (void)locale;
  return btowc(c);
}

int wctob_l(wint_t c, locale_t locale) {
  (void)locale;
  return wctob(c);
}

size_t wcsnrtombs_l(
    char* dst, const wchar_t** src, size_t src_len, size_t dst_len, mbstate_t* ps, locale_t locale) {
  (void)locale;
  return wcsnrtombs(dst, src, src_len, dst_len, ps);
}

size_t wcrtomb_l(char* s, wchar_t wc, mbstate_t* ps, locale_t locale) {
  (void)locale;
  return wcrtomb(s, wc, ps);
}

size_t mbsnrtowcs_l(
    wchar_t* dst, const char** src, size_t src_len, size_t dst_len, mbstate_t* ps, locale_t locale) {
  (void)locale;
  return mbsnrtowcs(dst, src, src_len, dst_len, ps);
}

size_t mbrtowc_l(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps, locale_t locale) {
  (void)locale;
  return mbrtowc(pwc, s, n, ps);
}

int mbtowc_l(wchar_t* pwc, const char* s, size_t n, locale_t locale) {
  (void)locale;
  return mbtowc(pwc, s, n);
}

size_t mbrlen_l(const char* s, size_t n, mbstate_t* ps, locale_t locale) {
  (void)locale;
  return mbrlen(s, n, ps);
}

size_t mbsrtowcs_l(wchar_t* dst, const char** src, size_t len, mbstate_t* ps, locale_t locale) {
  (void)locale;
  return mbsrtowcs(dst, src, len, ps);
}

int snprintf_l(char* s, size_t n, locale_t locale, const char* format, ...) {
  int result;
  va_list ap;
  (void)locale;
  va_start(ap, format);
  result = vsnprintf(s, n, format, ap);
  va_end(ap);
  return result;
}

int asprintf_l(char** strp, locale_t locale, const char* format, ...) {
  int result;
  va_list ap;
  (void)locale;
  va_start(ap, format);
  result = vasprintf(strp, format, ap);
  va_end(ap);
  return result;
}

int sscanf_l(const char* s, locale_t locale, const char* format, ...) {
  int result;
  va_list ap;
  (void)locale;
  va_start(ap, format);
  result = vsscanf(s, format, ap);
  va_end(ap);
  return result;
}
