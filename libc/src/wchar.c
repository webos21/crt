#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static mbstate_t internal_mbrtowc_state;
static mbstate_t internal_wcrtomb_state;

static void reset_state(mbstate_t* ps) {
  ps->codepoint = 0;
  ps->expected = 0;
  ps->seen = 0;
}

static int valid_codepoint(uint32_t wc) {
  return wc <= 0x10ffffU && !(wc >= 0xd800U && wc <= 0xdfffU);
}

static int wchar_can_hold(uint32_t wc) {
  return (uint32_t)(wchar_t)wc == wc;
}

wint_t btowc(int c) {
  if (c == EOF || (unsigned int)c > 0x7fU) {
    return WEOF;
  }
  return (wint_t)(unsigned char)c;
}

int wctob(wint_t c) {
  if (c == WEOF || (unsigned int)c > 0x7fU) {
    return EOF;
  }
  return (int)c;
}

int mbsinit(const mbstate_t* ps) {
  return ps == 0 || (ps->expected == 0 && ps->seen == 0);
}

size_t mbrtowc(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps) {
  mbstate_t* state = ps != 0 ? ps : &internal_mbrtowc_state;
  size_t i = 0;
  uint32_t codepoint;

  if (s == 0) {
    reset_state(state);
    return 0;
  }
  if (n == 0) {
    return (size_t)-2;
  }

  codepoint = state->codepoint;
  if (state->expected == 0) {
    unsigned char first = (unsigned char)s[0];
    ++i;
    if (first == 0) {
      if (pwc != 0) {
        *pwc = 0;
      }
      reset_state(state);
      return 0;
    }
    if (first < 0x80U) {
      if (pwc != 0) {
        *pwc = (wchar_t)first;
      }
      reset_state(state);
      return 1;
    }
    if (first >= 0xc2U && first <= 0xdfU) {
      state->expected = 2;
      state->seen = 1;
      codepoint = first & 0x1fU;
    } else if (first >= 0xe0U && first <= 0xefU) {
      state->expected = 3;
      state->seen = 1;
      codepoint = first & 0x0fU;
    } else if (first >= 0xf0U && first <= 0xf4U) {
      state->expected = 4;
      state->seen = 1;
      codepoint = first & 0x07U;
    } else {
      reset_state(state);
      errno = EILSEQ;
      return (size_t)-1;
    }
  }

  while (i < n && state->seen < state->expected) {
    unsigned char ch = (unsigned char)s[i++];
    if ((ch & 0xc0U) != 0x80U) {
      reset_state(state);
      errno = EILSEQ;
      return (size_t)-1;
    }
    codepoint = (codepoint << 6) | (ch & 0x3fU);
    ++state->seen;
  }

  if (state->seen < state->expected) {
    state->codepoint = codepoint;
    return (size_t)-2;
  }

  if ((state->expected == 2 && codepoint < 0x80U) ||
      (state->expected == 3 && codepoint < 0x800U) ||
      (state->expected == 4 && codepoint < 0x10000U) ||
      !valid_codepoint(codepoint) ||
      !wchar_can_hold(codepoint)) {
    reset_state(state);
    errno = EILSEQ;
    return (size_t)-1;
  }
  if (pwc != 0) {
    *pwc = (wchar_t)codepoint;
  }
  reset_state(state);
  return i;
}

size_t wcrtomb(char* s, wchar_t wc, mbstate_t* ps) {
  uint32_t codepoint = (uint32_t)wc;
  (void)ps;

  if (s == 0) {
    return 1;
  }
  if (!valid_codepoint(codepoint)) {
    errno = EILSEQ;
    return (size_t)-1;
  }
  if (codepoint < 0x80U) {
    s[0] = (char)codepoint;
    return 1;
  }
  if (codepoint < 0x800U) {
    s[0] = (char)(0xc0U | (codepoint >> 6));
    s[1] = (char)(0x80U | (codepoint & 0x3fU));
    return 2;
  }
  if (codepoint < 0x10000U) {
    s[0] = (char)(0xe0U | (codepoint >> 12));
    s[1] = (char)(0x80U | ((codepoint >> 6) & 0x3fU));
    s[2] = (char)(0x80U | (codepoint & 0x3fU));
    return 3;
  }
  s[0] = (char)(0xf0U | (codepoint >> 18));
  s[1] = (char)(0x80U | ((codepoint >> 12) & 0x3fU));
  s[2] = (char)(0x80U | ((codepoint >> 6) & 0x3fU));
  s[3] = (char)(0x80U | (codepoint & 0x3fU));
  return 4;
}

size_t mbsnrtowcs(wchar_t* dst, const char** src, size_t src_len, size_t dst_len, mbstate_t* ps) {
  const char* in;
  const char* end;
  size_t count = 0;
  mbstate_t local_state = {0, 0, 0};
  mbstate_t* state;

  if (src == 0 || *src == 0) {
    errno = EINVAL;
    return (size_t)-1;
  }
  in = *src;
  end = src_len == (size_t)-1 ? 0 : in + src_len;
  state = ps != 0 ? ps : &local_state;
  while ((end == 0 || in < end) && *in != '\0') {
    wchar_t wc;
    size_t available = end == 0 ? strlen(in) : (size_t)(end - in);
    size_t consumed = mbrtowc(&wc, in, available, state);
    if (consumed == (size_t)-1 || consumed == (size_t)-2) {
      return (size_t)-1;
    }
    if (dst != 0) {
      if (count == dst_len) {
        *src = in;
        return count;
      }
      dst[count] = wc;
    }
    ++count;
    in += consumed;
  }
  if (dst != 0) {
    if ((end == 0 || in < end) && *in == 0 && count < dst_len) {
      dst[count] = 0;
      *src = 0;
    } else {
      *src = in;
    }
  }
  return count;
}

size_t mbsrtowcs(wchar_t* dst, const char** src, size_t len, mbstate_t* ps) {
  return mbsnrtowcs(dst, src, (size_t)-1, len, ps);
}

size_t mbrlen(const char* s, size_t n, mbstate_t* ps) {
  return mbrtowc(0, s, n, ps);
}

size_t wcsnrtombs(char* dst, const wchar_t** src, size_t src_len, size_t dst_len, mbstate_t* ps) {
  const wchar_t* in;
  size_t count = 0;
  size_t converted = 0;
  char buffer[4];
  (void)ps;

  if (src == 0 || *src == 0) {
    errno = EINVAL;
    return (size_t)-1;
  }
  in = *src;
  while (converted < src_len && *in != 0) {
    size_t produced = wcrtomb(buffer, *in, &internal_wcrtomb_state);
    size_t i;
    if (produced == (size_t)-1) {
      return (size_t)-1;
    }
    if (dst != 0) {
      if (count + produced > dst_len) {
        *src = in;
        return count;
      }
      for (i = 0; i < produced; ++i) {
        dst[count + i] = buffer[i];
      }
    }
    count += produced;
    ++in;
    ++converted;
  }
  if (dst != 0) {
    if (converted < src_len && *in == 0 && count < dst_len) {
      dst[count] = '\0';
      *src = 0;
    } else {
      *src = in;
    }
  }
  return count;
}

size_t wcsrtombs(char* dst, const wchar_t** src, size_t len, mbstate_t* ps) {
  return wcsnrtombs(dst, src, (size_t)-1, len, ps);
}

size_t mbstowcs(wchar_t* dst, const char* src, size_t len) {
  const char* in = src;
  return mbsrtowcs(dst, &in, len, 0);
}

size_t wcstombs(char* dst, const wchar_t* src, size_t len) {
  const wchar_t* in = src;
  return wcsrtombs(dst, &in, len, 0);
}

size_t wcsftime(wchar_t* s, size_t max, const wchar_t* format, const struct tm* tm) {
  char* narrow_format;
  char* narrow_result;
  const wchar_t* format_source;
  const char* result_source;
  size_t format_length;
  size_t result_capacity;
  size_t result_length;

  if (s == 0 || format == 0 || tm == 0 || max == 0) return 0;
  format_source = format;
  format_length = wcsrtombs(0, &format_source, 0, 0);
  if (format_length == (size_t)-1) return 0;
  narrow_format = (char*)malloc(format_length + 1);
  if (narrow_format == 0) return 0;
  format_source = format;
  if (wcsrtombs(narrow_format, &format_source, format_length + 1, 0) == (size_t)-1) {
    free(narrow_format);
    return 0;
  }
  result_capacity = max * MB_CUR_MAX;
  narrow_result = (char*)malloc(result_capacity);
  if (narrow_result == 0) {
    free(narrow_format);
    return 0;
  }
  result_length = strftime(narrow_result, result_capacity, narrow_format, tm);
  free(narrow_format);
  if (result_length == 0) {
    free(narrow_result);
    return 0;
  }
  result_source = narrow_result;
  result_length = mbsrtowcs(s, &result_source, max, 0);
  free(narrow_result);
  return result_length == (size_t)-1 || result_source != 0 ? 0 : result_length;
}

int mblen(const char* s, size_t n) {
  size_t result;

  if (s == 0) {
    reset_state(&internal_mbrtowc_state);
    return 0;
  }
  result = mbrtowc(0, s, n, &internal_mbrtowc_state);
  if (result == (size_t)-1 || result == (size_t)-2) {
    return -1;
  }
  return (int)result;
}

int mbtowc(wchar_t* pwc, const char* s, size_t n) {
  size_t result;

  if (s == 0) {
    reset_state(&internal_mbrtowc_state);
    return 0;
  }
  result = mbrtowc(pwc, s, n, &internal_mbrtowc_state);
  if (result == (size_t)-1 || result == (size_t)-2) {
    return -1;
  }
  return (int)result;
}

int wctomb(char* s, wchar_t wc) {
  size_t result;

  if (s == 0) {
    reset_state(&internal_wcrtomb_state);
    return 0;
  }
  result = wcrtomb(s, wc, &internal_wcrtomb_state);
  if (result == (size_t)-1) {
    return -1;
  }
  return (int)result;
}

size_t wcslen(const wchar_t* s) {
  const wchar_t* p = s;
  while (*p != 0) {
    ++p;
  }
  return (size_t)(p - s);
}

size_t wcsnlen(const wchar_t* s, size_t maxlen) {
  size_t i;
  for (i = 0; i < maxlen && s[i] != 0; ++i) {
  }
  return i;
}

static int wide_ascii_tolower(wchar_t wc) {
  return wc >= L'A' && wc <= L'Z' ? (int)(wc - L'A' + L'a') : (int)wc;
}

int wcscmp(const wchar_t* s1, const wchar_t* s2) {
  while (*s1 != 0 && *s1 == *s2) {
    ++s1;
    ++s2;
  }
  return *s1 < *s2 ? -1 : (*s1 > *s2 ? 1 : 0);
}

int wcsncmp(const wchar_t* s1, const wchar_t* s2, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) {
    if (s1[i] != s2[i] || s1[i] == 0) {
      return s1[i] < s2[i] ? -1 : (s1[i] > s2[i] ? 1 : 0);
    }
  }
  return 0;
}

int wcscasecmp(const wchar_t* s1, const wchar_t* s2) {
  while (*s1 != 0 && wide_ascii_tolower(*s1) == wide_ascii_tolower(*s2)) {
    ++s1;
    ++s2;
  }
  return wide_ascii_tolower(*s1) < wide_ascii_tolower(*s2) ? -1 :
      (wide_ascii_tolower(*s1) > wide_ascii_tolower(*s2) ? 1 : 0);
}

int wcsncasecmp(const wchar_t* s1, const wchar_t* s2, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) {
    int c1 = wide_ascii_tolower(s1[i]);
    int c2 = wide_ascii_tolower(s2[i]);
    if (c1 != c2 || s1[i] == 0) {
      return c1 < c2 ? -1 : (c1 > c2 ? 1 : 0);
    }
  }
  return 0;
}

wchar_t* wcscpy(wchar_t* dst, const wchar_t* src) {
  wchar_t* result = dst;
  while ((*dst++ = *src++) != 0) {
  }
  return result;
}

wchar_t* wcpcpy(wchar_t* dst, const wchar_t* src) {
  while ((*dst = *src) != 0) {
    ++dst;
    ++src;
  }
  return dst;
}

wchar_t* wcsncpy(wchar_t* dst, const wchar_t* src, size_t n) {
  size_t i;
  for (i = 0; i < n && src[i] != 0; ++i) {
    dst[i] = src[i];
  }
  while (i < n) {
    dst[i++] = 0;
  }
  return dst;
}

wchar_t* wcpncpy(wchar_t* dst, const wchar_t* src, size_t n) {
  size_t i;
  wchar_t* end;

  for (i = 0; i < n && src[i] != 0; ++i) {
    dst[i] = src[i];
  }
  if (i == n) {
    return dst + n;
  }
  end = dst + i;
  dst[i++] = 0;
  while (i < n) {
    dst[i++] = 0;
  }
  return end;
}

wchar_t* wcscat(wchar_t* dst, const wchar_t* src) {
  wcscpy(dst + wcslen(dst), src);
  return dst;
}

wchar_t* wcsncat(wchar_t* dst, const wchar_t* src, size_t n) {
  wchar_t* d = dst + wcslen(dst);
  size_t i;

  for (i = 0; i < n && src[i] != 0; ++i) {
    d[i] = src[i];
  }
  d[i] = 0;
  return dst;
}

wchar_t* wcschr(const wchar_t* s, wchar_t c) {
  while (*s != 0) {
    if (*s == c) {
      return (wchar_t*)s;
    }
    ++s;
  }
  return c == 0 ? (wchar_t*)s : 0;
}

wchar_t* wcspbrk(const wchar_t* s, const wchar_t* accept) {
  while (*s != 0) {
    if (wcschr(accept, *s) != 0) {
      return (wchar_t*)s;
    }
    ++s;
  }
  return 0;
}

wchar_t* wcsrchr(const wchar_t* s, wchar_t c) {
  const wchar_t* last = 0;
  do {
    if (*s == c) {
      last = s;
    }
  } while (*s++ != 0);
  return (wchar_t*)last;
}

size_t wcscspn(const wchar_t* s, const wchar_t* reject) {
  size_t n = 0;

  while (s[n] != 0 && wcschr(reject, s[n]) == 0) {
    ++n;
  }
  return n;
}

size_t wcsspn(const wchar_t* s, const wchar_t* accept) {
  size_t n = 0;

  while (s[n] != 0 && wcschr(accept, s[n]) != 0) {
    ++n;
  }
  return n;
}

wchar_t* wcsstr(const wchar_t* s, const wchar_t* find) {
  size_t find_len = wcslen(find);
  if (find_len == 0) {
    return (wchar_t*)s;
  }
  while (*s != 0) {
    if (*s == *find && wcsncmp(s, find, find_len) == 0) {
      return (wchar_t*)s;
    }
    ++s;
  }
  return 0;
}

wchar_t* wcstok(wchar_t* s, const wchar_t* delimiter, wchar_t** ptr) {
  wchar_t* token;

  if (ptr == 0) {
    errno = EINVAL;
    return 0;
  }
  if (s == 0) {
    s = *ptr;
  }
  if (s == 0) {
    return 0;
  }
  s += wcsspn(s, delimiter);
  if (*s == 0) {
    *ptr = 0;
    return 0;
  }
  token = s;
  s += wcscspn(s, delimiter);
  if (*s != 0) {
    *s++ = 0;
    *ptr = s;
  } else {
    *ptr = 0;
  }
  return token;
}

int wcscoll(const wchar_t* s1, const wchar_t* s2) {
  return wcscmp(s1, s2);
}

size_t wcsxfrm(wchar_t* dst, const wchar_t* src, size_t n) {
  size_t len = wcslen(src);
  size_t copy = len;

  if (dst != 0 && n != 0) {
    size_t i;
    if (copy >= n) {
      copy = n - 1;
    }
    for (i = 0; i < copy; ++i) {
      dst[i] = src[i];
    }
    dst[copy] = 0;
  }
  return len;
}

size_t wcslcpy(wchar_t* dst, const wchar_t* src, size_t n) {
  size_t len = wcslen(src);

  if (n != 0) {
    size_t copy = len >= n ? n - 1 : len;
    wmemcpy(dst, src, copy);
    dst[copy] = 0;
  }
  return len;
}

size_t wcslcat(wchar_t* dst, const wchar_t* src, size_t n) {
  size_t dst_len = wcsnlen(dst, n);
  size_t src_len = wcslen(src);

  if (dst_len == n) {
    return n + src_len;
  }
  if (n > dst_len + 1) {
    size_t copy = src_len >= n - dst_len ? n - dst_len - 1 : src_len;
    wmemcpy(dst + dst_len, src, copy);
    dst[dst_len + copy] = 0;
  }
  return dst_len + src_len;
}

wchar_t* wcsdup(const wchar_t* s) {
  size_t len = wcslen(s) + 1;
  wchar_t* copy = (wchar_t*)malloc(len * sizeof(wchar_t));

  if (copy == 0) {
    return 0;
  }
  return wmemcpy(copy, s, len);
}

wchar_t* wmemchr(const wchar_t* s, wchar_t c, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) {
    if (s[i] == c) {
      return (wchar_t*)&s[i];
    }
  }
  return 0;
}

int wmemcmp(const wchar_t* s1, const wchar_t* s2, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) {
    if (s1[i] != s2[i]) {
      return s1[i] < s2[i] ? -1 : 1;
    }
  }
  return 0;
}

wchar_t* wmemcpy(wchar_t* dst, const wchar_t* src, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) {
    dst[i] = src[i];
  }
  return dst;
}

wchar_t* wmempcpy(wchar_t* dst, const wchar_t* src, size_t n) {
  return wmemcpy(dst, src, n) + n;
}

wchar_t* wmemmove(wchar_t* dst, const wchar_t* src, size_t n) {
  size_t i;
  if (dst < src) {
    for (i = 0; i < n; ++i) {
      dst[i] = src[i];
    }
  } else if (dst > src) {
    for (i = n; i > 0; --i) {
      dst[i - 1] = src[i - 1];
    }
  }
  return dst;
}

wchar_t* wmemset(wchar_t* dst, wchar_t c, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) {
    dst[i] = c;
  }
  return dst;
}

int __crt_stdio_get_orientation(FILE* stream);
int __crt_stdio_set_orientation(FILE* stream, int mode);
mbstate_t* __crt_stdio_get_mbstate_in(FILE* stream);
mbstate_t* __crt_stdio_get_mbstate_out(FILE* stream);
int __crt_stdio_pop_ungetwc(FILE* stream, wchar_t* wc);
int __crt_stdio_push_ungetwc(FILE* stream, wchar_t wc);

#define ORIENT_BYTES (-1)
#define ORIENT_UNKNOWN 0
#define ORIENT_CHARS 1

static int ensure_wide_orientation(FILE* stream) {
  int orientation = __crt_stdio_get_orientation(stream);

  if (orientation == ORIENT_BYTES) {
    errno = EILSEQ;
    return -1;
  }
  if (orientation == ORIENT_UNKNOWN) {
    orientation = __crt_stdio_set_orientation(stream, ORIENT_CHARS);
  }
  return orientation == ORIENT_CHARS ? 0 : -1;
}

static char* wide_to_multibyte_alloc(const wchar_t* s) {
  const wchar_t* src = s;
  size_t len;
  char* out;

  if (s == 0) {
    errno = EINVAL;
    return 0;
  }
  len = wcsrtombs(0, &src, 0, 0);
  if (len == (size_t)-1) {
    return 0;
  }
  out = (char*)malloc(len + 1);
  if (out == 0) {
    errno = ENOMEM;
    return 0;
  }
  src = s;
  if (wcsrtombs(out, &src, len + 1, 0) == (size_t)-1) {
    free(out);
    return 0;
  }
  out[len] = 0;
  return out;
}

static wchar_t* wide_end_from_narrow_offset(const wchar_t* s, size_t target_offset) {
  size_t offset = 0;

  while (*s != 0 && offset < target_offset) {
    char mb[4];
    size_t n = wcrtomb(mb, *s, 0);

    if (n == (size_t)-1 || offset + n > target_offset) {
      break;
    }
    offset += n;
    ++s;
  }
  return (wchar_t*)s;
}

double wcstod(const wchar_t* s, wchar_t** endptr) {
  char* narrow = wide_to_multibyte_alloc(s);
  char* narrow_end = 0;
  double result;

  if (narrow == 0) {
    if (endptr != 0) {
      *endptr = (wchar_t*)s;
    }
    return 0.0;
  }
  result = strtod(narrow, &narrow_end);
  if (endptr != 0) {
    *endptr = wide_end_from_narrow_offset(s, (size_t)(narrow_end - narrow));
  }
  free(narrow);
  return result;
}

float wcstof(const wchar_t* s, wchar_t** endptr) {
  char* narrow = wide_to_multibyte_alloc(s);
  char* narrow_end = 0;
  float result;

  if (narrow == 0) {
    if (endptr != 0) {
      *endptr = (wchar_t*)s;
    }
    return 0.0f;
  }
  result = strtof(narrow, &narrow_end);
  if (endptr != 0) {
    *endptr = wide_end_from_narrow_offset(s, (size_t)(narrow_end - narrow));
  }
  free(narrow);
  return result;
}

long double wcstold(const wchar_t* s, wchar_t** endptr) {
  char* narrow = wide_to_multibyte_alloc(s);
  char* narrow_end = 0;
  long double result;

  if (narrow == 0) {
    if (endptr != 0) {
      *endptr = (wchar_t*)s;
    }
    return 0.0L;
  }
  result = strtold(narrow, &narrow_end);
  if (endptr != 0) {
    *endptr = wide_end_from_narrow_offset(s, (size_t)(narrow_end - narrow));
  }
  free(narrow);
  return result;
}

long wcstol(const wchar_t* s, wchar_t** endptr, int base) {
  char* narrow = wide_to_multibyte_alloc(s);
  char* narrow_end = 0;
  long result;

  if (narrow == 0) {
    if (endptr != 0) {
      *endptr = (wchar_t*)s;
    }
    return 0;
  }
  result = strtol(narrow, &narrow_end, base);
  if (endptr != 0) {
    *endptr = wide_end_from_narrow_offset(s, (size_t)(narrow_end - narrow));
  }
  free(narrow);
  return result;
}

long long wcstoll(const wchar_t* s, wchar_t** endptr, int base) {
  char* narrow = wide_to_multibyte_alloc(s);
  char* narrow_end = 0;
  long long result;

  if (narrow == 0) {
    if (endptr != 0) {
      *endptr = (wchar_t*)s;
    }
    return 0;
  }
  result = strtoll(narrow, &narrow_end, base);
  if (endptr != 0) {
    *endptr = wide_end_from_narrow_offset(s, (size_t)(narrow_end - narrow));
  }
  free(narrow);
  return result;
}

unsigned long wcstoul(const wchar_t* s, wchar_t** endptr, int base) {
  char* narrow = wide_to_multibyte_alloc(s);
  char* narrow_end = 0;
  unsigned long result;

  if (narrow == 0) {
    if (endptr != 0) {
      *endptr = (wchar_t*)s;
    }
    return 0;
  }
  result = strtoul(narrow, &narrow_end, base);
  if (endptr != 0) {
    *endptr = wide_end_from_narrow_offset(s, (size_t)(narrow_end - narrow));
  }
  free(narrow);
  return result;
}

unsigned long long wcstoull(const wchar_t* s, wchar_t** endptr, int base) {
  char* narrow = wide_to_multibyte_alloc(s);
  char* narrow_end = 0;
  unsigned long long result;

  if (narrow == 0) {
    if (endptr != 0) {
      *endptr = (wchar_t*)s;
    }
    return 0;
  }
  result = strtoull(narrow, &narrow_end, base);
  if (endptr != 0) {
    *endptr = wide_end_from_narrow_offset(s, (size_t)(narrow_end - narrow));
  }
  free(narrow);
  return result;
}

int wcwidth(wchar_t wc) {
  uint32_t value = (uint32_t)wc;

  if (wc == 0) {
    return 0;
  }
  if (value < 0x20U || (value >= 0x7fU && value < 0xa0U) || !valid_codepoint(value)) {
    return -1;
  }
  return 1;
}

int wcswidth(const wchar_t* s, size_t n) {
  int total = 0;
  size_t i;

  for (i = 0; i < n && s[i] != 0; ++i) {
    int width = wcwidth(s[i]);
    if (width < 0) {
      return -1;
    }
    total += width;
  }
  return total;
}

struct wmemstream_cookie {
  wchar_t** ptr;
  size_t* sizep;
  wchar_t* buffer;
  size_t capacity;
  size_t length;
  size_t position;
  mbstate_t state;
};

static int wmemstream_sync(struct wmemstream_cookie* cookie) {
  if (cookie == 0) {
    errno = EINVAL;
    return -1;
  }
  if (cookie->length >= cookie->capacity) {
    size_t capacity = cookie->capacity == 0 ? 16 : cookie->capacity * 2;
    wchar_t* grown;

    while (capacity <= cookie->length) {
      size_t next = capacity * 2;
      if (next <= capacity) {
        errno = ENOMEM;
        return -1;
      }
      capacity = next;
    }
    grown = (wchar_t*)realloc(cookie->buffer, (capacity + 1) * sizeof(wchar_t));
    if (grown == 0) {
      return -1;
    }
    cookie->buffer = grown;
    cookie->capacity = capacity;
  }
  cookie->buffer[cookie->length] = 0;
  *cookie->ptr = cookie->buffer;
  *cookie->sizep = cookie->length;
  return 0;
}

static int wmemstream_ensure_capacity(struct wmemstream_cookie* cookie, size_t needed) {
  size_t capacity;
  wchar_t* grown;

  if (needed <= cookie->capacity) {
    return 0;
  }
  capacity = cookie->capacity == 0 ? 16 : cookie->capacity;
  while (capacity < needed) {
    size_t next = capacity * 2;
    if (next <= capacity) {
      errno = ENOMEM;
      return -1;
    }
    capacity = next;
  }
  grown = (wchar_t*)realloc(cookie->buffer, (capacity + 1) * sizeof(wchar_t));
  if (grown == 0) {
    return -1;
  }
  cookie->buffer = grown;
  cookie->capacity = capacity;
  *cookie->ptr = cookie->buffer;
  return 0;
}

static int wmemstream_write(void* opaque, const char* buf, int count) {
  struct wmemstream_cookie* cookie = (struct wmemstream_cookie*)opaque;
  int i;

  if (cookie == 0 || count < 0) {
    errno = EINVAL;
    return -1;
  }
  for (i = 0; i < count; ++i) {
    wchar_t wc;
    size_t result = mbrtowc(&wc, buf + i, 1, &cookie->state);

    if (result == (size_t)-1) {
      return -1;
    }
    if (result == (size_t)-2) {
      continue;
    }
    if (wmemstream_ensure_capacity(cookie, cookie->position + 1) != 0) {
      return -1;
    }
    cookie->buffer[cookie->position++] = wc;
    if (cookie->position > cookie->length) {
      cookie->length = cookie->position;
    }
  }
  if (wmemstream_sync(cookie) != 0) {
    return -1;
  }
  return count;
}

static fpos_t wmemstream_seek(void* opaque, fpos_t offset, int whence) {
  struct wmemstream_cookie* cookie = (struct wmemstream_cookie*)opaque;
  fpos_t base;
  fpos_t target;

  if (cookie == 0) {
    errno = EINVAL;
    return (fpos_t)-1;
  }
  if (whence == SEEK_SET) {
    base = 0;
  } else if (whence == SEEK_CUR) {
    base = (fpos_t)cookie->position;
  } else if (whence == SEEK_END) {
    base = (fpos_t)cookie->length;
  } else {
    errno = EINVAL;
    return (fpos_t)-1;
  }
  target = base + offset;
  if (target < 0) {
    errno = EINVAL;
    return (fpos_t)-1;
  }
  if (wmemstream_ensure_capacity(cookie, (size_t)target) != 0) {
    return (fpos_t)-1;
  }
  while (cookie->length < (size_t)target) {
    cookie->buffer[cookie->length++] = 0;
  }
  cookie->position = (size_t)target;
  (void)wmemstream_sync(cookie);
  return target;
}

static int wmemstream_close(void* opaque) {
  struct wmemstream_cookie* cookie = (struct wmemstream_cookie*)opaque;

  if (cookie == 0) {
    return 0;
  }
  (void)wmemstream_sync(cookie);
  free(cookie);
  return 0;
}

FILE* open_wmemstream(wchar_t** ptr, size_t* sizep) {
  struct wmemstream_cookie* cookie;
  FILE* stream;

  if (ptr == 0 || sizep == 0) {
    errno = EINVAL;
    return 0;
  }
  cookie = (struct wmemstream_cookie*)malloc(sizeof(*cookie));
  if (cookie == 0) {
    errno = ENOMEM;
    return 0;
  }
  memset(cookie, 0, sizeof(*cookie));
  cookie->ptr = ptr;
  cookie->sizep = sizep;
  cookie->capacity = 16;
  cookie->buffer = (wchar_t*)malloc((cookie->capacity + 1) * sizeof(wchar_t));
  if (cookie->buffer == 0) {
    free(cookie);
    errno = ENOMEM;
    return 0;
  }
  cookie->buffer[0] = 0;
  *ptr = cookie->buffer;
  *sizep = 0;
  stream = funopen(cookie, 0, wmemstream_write, wmemstream_seek, wmemstream_close);
  if (stream == 0) {
    free(cookie->buffer);
    free(cookie);
    return 0;
  }
  (void)fwide(stream, 1);
  return stream;
}

static int is_format_flag(wchar_t wc) {
  return wc == L'-' || wc == L'+' || wc == L' ' || wc == L'#' || wc == L'0';
}

static int is_format_digit(wchar_t wc) {
  return wc >= L'0' && wc <= L'9';
}

static char* wide_format_to_narrow_alloc(const wchar_t* format, int wide_io) {
  size_t cap = wcslen(format) * 5 + 1;
  char* out = (char*)malloc(cap);
  size_t pos = 0;
  size_t i = 0;

  if (out == 0) {
    errno = ENOMEM;
    return 0;
  }
  while (format[i] != 0) {
    wchar_t wc = format[i++];

    if (wc != L'%') {
      char mb[4];
      size_t n = wcrtomb(mb, wc, 0);
      size_t j;
      if (n == (size_t)-1) {
        free(out);
        return 0;
      }
      for (j = 0; j < n; ++j) {
        out[pos++] = mb[j];
      }
      continue;
    }

    out[pos++] = '%';
    if (format[i] == L'%') {
      out[pos++] = '%';
      ++i;
      continue;
    }
    while (is_format_flag(format[i])) {
      out[pos++] = (char)format[i++];
    }
    if (format[i] == L'*') {
      out[pos++] = '*';
      ++i;
    } else {
      while (is_format_digit(format[i])) {
        out[pos++] = (char)format[i++];
      }
    }
    if (format[i] == L'.') {
      out[pos++] = '.';
      ++i;
      if (format[i] == L'*') {
        out[pos++] = '*';
        ++i;
      } else {
        while (is_format_digit(format[i])) {
          out[pos++] = (char)format[i++];
        }
      }
    }
    if (wide_io && format[i] == L'm' &&
        (format[i + 1] == L's' || format[i + 1] == L'c' || format[i + 1] == L'[')) {
      out[pos++] = 'm';
      ++i;
    } else if (wide_io && format[i] == L'a' &&
               (format[i + 1] == L's' || format[i + 1] == L'c' || format[i + 1] == L'[')) {
      out[pos++] = 'a';
      ++i;
    }
    if (format[i] == L'h' || format[i] == L'l' || format[i] == L'j' ||
        format[i] == L'z' || format[i] == L't') {
      out[pos++] = (char)format[i++];
      if ((out[pos - 1] == 'h' && format[i] == L'h') ||
          (out[pos - 1] == 'l' && format[i] == L'l')) {
        out[pos++] = (char)format[i++];
      }
    } else if (wide_io && (format[i] == L's' || format[i] == L'c' || format[i] == L'[')) {
      out[pos++] = 'l';
    }
    if (format[i] != 0) {
      out[pos++] = (char)format[i++];
    }
  }
  out[pos] = 0;
  return out;
}

wint_t fputwc(wchar_t wc, FILE* stream) {
  char mb[4];
  size_t n;
  mbstate_t* state;

  if (ensure_wide_orientation(stream) != 0) {
    return WEOF;
  }
  state = __crt_stdio_get_mbstate_out(stream);
  if (state == 0) {
    return WEOF;
  }
  n = wcrtomb(mb, wc, state);
  if (n == (size_t)-1) {
    return WEOF;
  }
  return fwrite(mb, 1, n, stream) == n ? (wint_t)wc : WEOF;
}

wint_t putwc(wchar_t wc, FILE* stream) {
  return fputwc(wc, stream);
}

wint_t putwchar(wchar_t wc) {
  return fputwc(wc, stdout);
}

int fputws(const wchar_t* s, FILE* stream) {
  while (*s != 0) {
    if (fputwc(*s++, stream) == WEOF) {
      return EOF;
    }
  }
  return 0;
}

wint_t fgetwc(FILE* stream) {
  char bytes[4];
  mbstate_t* state;
  size_t used;
  int ch;
  wchar_t ungot;

  if (ensure_wide_orientation(stream) != 0) {
    return WEOF;
  }
  if (__crt_stdio_pop_ungetwc(stream, &ungot)) {
    return (wint_t)ungot;
  }
  state = __crt_stdio_get_mbstate_in(stream);
  if (state == 0) {
    return WEOF;
  }
  for (used = 0; used < sizeof(bytes); ++used) {
    wchar_t wc;
    size_t result;

    ch = fgetc(stream);
    if (ch == EOF) {
      return WEOF;
    }
    bytes[used] = (char)ch;
    result = mbrtowc(&wc, bytes + used, 1, state);
    if (result == (size_t)-1) {
      return WEOF;
    }
    if (result != (size_t)-2) {
      return (wint_t)wc;
    }
  }
  errno = EILSEQ;
  return WEOF;
}

wint_t getwc(FILE* stream) {
  return fgetwc(stream);
}

wint_t getwchar(void) {
  return fgetwc(stdin);
}

wint_t ungetwc(wint_t wc, FILE* stream) {
  if (wc == WEOF || ensure_wide_orientation(stream) != 0) {
    return WEOF;
  }
  return __crt_stdio_push_ungetwc(stream, (wchar_t)wc) == 0 ? wc : WEOF;
}

wchar_t* fgetws(wchar_t* s, int size, FILE* stream) {
  int i = 0;

  if (s == 0 || size <= 0) {
    errno = EINVAL;
    return 0;
  }
  while (i < size - 1) {
    wint_t wc = fgetwc(stream);
    if (wc == WEOF) {
      break;
    }
    s[i++] = (wchar_t)wc;
    if (wc == L'\n') {
      break;
    }
  }
  if (i == 0) {
    return 0;
  }
  s[i] = 0;
  return s;
}

int fwide(FILE* stream, int mode) {
  int orientation = __crt_stdio_get_orientation(stream);

  if (mode > 0 && orientation == ORIENT_UNKNOWN) {
    orientation = __crt_stdio_set_orientation(stream, ORIENT_CHARS);
  } else if (mode < 0 && orientation == ORIENT_UNKNOWN) {
    orientation = __crt_stdio_set_orientation(stream, ORIENT_BYTES);
  }
  return orientation;
}

int vfwprintf(FILE* stream, const wchar_t* format, va_list ap) {
  char* narrow_format;
  int result;

  if (ensure_wide_orientation(stream) != 0) {
    return -1;
  }
  narrow_format = wide_format_to_narrow_alloc(format, 1);
  if (narrow_format == 0) {
    return -1;
  }
  result = vfprintf(stream, narrow_format, ap);
  free(narrow_format);
  return result;
}

int fwprintf(FILE* stream, const wchar_t* format, ...) {
  va_list ap;
  int result;

  va_start(ap, format);
  result = vfwprintf(stream, format, ap);
  va_end(ap);
  return result;
}

int vwprintf(const wchar_t* format, va_list ap) {
  return vfwprintf(stdout, format, ap);
}

int wprintf(const wchar_t* format, ...) {
  va_list ap;
  int result;

  va_start(ap, format);
  result = vwprintf(format, ap);
  va_end(ap);
  return result;
}

int vswprintf(wchar_t* s, size_t n, const wchar_t* format, va_list ap) {
  char stack_buffer[1024];
  char* narrow_buffer = stack_buffer;
  char* narrow_format = wide_format_to_narrow_alloc(format, 1);
  va_list copy;
  int length;
  const char* src;
  size_t converted;

  if (n == 0 || s == 0 || narrow_format == 0) {
    free(narrow_format);
    errno = EINVAL;
    return -1;
  }
  va_copy(copy, ap);
  length = vsnprintf(stack_buffer, sizeof(stack_buffer), narrow_format, copy);
  va_end(copy);
  if (length < 0) {
    free(narrow_format);
    return -1;
  }
  if ((size_t)length >= sizeof(stack_buffer)) {
    narrow_buffer = (char*)malloc((size_t)length + 1);
    if (narrow_buffer == 0) {
      free(narrow_format);
      errno = ENOMEM;
      return -1;
    }
    length = vsnprintf(narrow_buffer, (size_t)length + 1, narrow_format, ap);
  }
  free(narrow_format);
  src = narrow_buffer;
  converted = mbsrtowcs(s, &src, n, 0);
  if (narrow_buffer != stack_buffer) {
    free(narrow_buffer);
  }
  if (converted == (size_t)-1 || converted >= n) {
    if (n != 0) {
      s[n - 1] = 0;
    }
    return -1;
  }
  (void)length;
  return (int)converted;
}

int swprintf(wchar_t* s, size_t n, const wchar_t* format, ...) {
  va_list ap;
  int result;

  va_start(ap, format);
  result = vswprintf(s, n, format, ap);
  va_end(ap);
  return result;
}

int vfwscanf(FILE* stream, const wchar_t* format, va_list ap) {
  char* narrow_format;
  int result;

  if (ensure_wide_orientation(stream) != 0) {
    return EOF;
  }
  narrow_format = wide_format_to_narrow_alloc(format, 1);
  if (narrow_format == 0) {
    return EOF;
  }
  result = vfscanf(stream, narrow_format, ap);
  free(narrow_format);
  return result;
}

int fwscanf(FILE* stream, const wchar_t* format, ...) {
  va_list ap;
  int result;

  va_start(ap, format);
  result = vfwscanf(stream, format, ap);
  va_end(ap);
  return result;
}

int vwscanf(const wchar_t* format, va_list ap) {
  return vfwscanf(stdin, format, ap);
}

int wscanf(const wchar_t* format, ...) {
  va_list ap;
  int result;

  va_start(ap, format);
  result = vwscanf(format, ap);
  va_end(ap);
  return result;
}

int vswscanf(const wchar_t* s, const wchar_t* format, va_list ap) {
  char* narrow_input = wide_to_multibyte_alloc(s);
  char* narrow_format = wide_format_to_narrow_alloc(format, 1);
  int result;

  if (narrow_input == 0 || narrow_format == 0) {
    free(narrow_input);
    free(narrow_format);
    return EOF;
  }
  result = vsscanf(narrow_input, narrow_format, ap);
  free(narrow_input);
  free(narrow_format);
  return result;
}

int swscanf(const wchar_t* s, const wchar_t* format, ...) {
  va_list ap;
  int result;

  va_start(ap, format);
  result = vswscanf(s, format, ap);
  va_end(ap);
  return result;
}
