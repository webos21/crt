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

size_t mbsrtowcs(wchar_t* dst, const char** src, size_t len, mbstate_t* ps) {
  const char* in;
  size_t count = 0;
  mbstate_t local_state = {0, 0, 0};
  mbstate_t* state;

  if (src == 0 || *src == 0) {
    errno = EINVAL;
    return (size_t)-1;
  }
  in = *src;
  state = ps != 0 ? ps : &local_state;
  while (*in != '\0') {
    wchar_t wc;
    size_t consumed = mbrtowc(&wc, in, strlen(in), state);
    if (consumed == (size_t)-1 || consumed == (size_t)-2) {
      return (size_t)-1;
    }
    if (dst != 0) {
      if (count == len) {
        *src = in;
        return count;
      }
      dst[count] = wc;
    }
    ++count;
    in += consumed;
  }
  if (dst != 0) {
    if (count < len) {
      dst[count] = 0;
      *src = 0;
    } else {
      *src = in;
    }
  }
  return count;
}

size_t wcsrtombs(char* dst, const wchar_t** src, size_t len, mbstate_t* ps) {
  const wchar_t* in;
  size_t count = 0;
  char buffer[4];
  (void)ps;

  if (src == 0 || *src == 0) {
    errno = EINVAL;
    return (size_t)-1;
  }
  in = *src;
  while (*in != 0) {
    size_t produced = wcrtomb(buffer, *in, &internal_wcrtomb_state);
    size_t i;
    if (produced == (size_t)-1) {
      return (size_t)-1;
    }
    if (dst != 0) {
      if (count + produced > len) {
        *src = in;
        return count;
      }
      for (i = 0; i < produced; ++i) {
        dst[count + i] = buffer[i];
      }
    }
    count += produced;
    ++in;
  }
  if (dst != 0) {
    if (count < len) {
      dst[count] = '\0';
      *src = 0;
    } else {
      *src = in;
    }
  }
  return count;
}

size_t mbstowcs(wchar_t* dst, const char* src, size_t len) {
  const char* in = src;
  return mbsrtowcs(dst, &in, len, 0);
}

size_t wcstombs(char* dst, const wchar_t* src, size_t len) {
  const wchar_t* in = src;
  return wcsrtombs(dst, &in, len, 0);
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

wchar_t* wcscpy(wchar_t* dst, const wchar_t* src) {
  wchar_t* result = dst;
  while ((*dst++ = *src++) != 0) {
  }
  return result;
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

wchar_t* wcscat(wchar_t* dst, const wchar_t* src) {
  wcscpy(dst + wcslen(dst), src);
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

wchar_t* wcsrchr(const wchar_t* s, wchar_t c) {
  const wchar_t* last = 0;
  do {
    if (*s == c) {
      last = s;
    }
  } while (*s++ != 0);
  return (wchar_t*)last;
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

  if (ensure_wide_orientation(stream) != 0) {
    return WEOF;
  }
  n = wcrtomb(mb, wc, 0);
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
  mbstate_t state = {0, 0, 0};
  size_t used;
  int ch;

  if (ensure_wide_orientation(stream) != 0) {
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
    result = mbrtowc(&wc, bytes + used, 1, &state);
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
  char mb[4];
  size_t n;

  if (wc == WEOF || ensure_wide_orientation(stream) != 0) {
    return WEOF;
  }
  n = wcrtomb(mb, (wchar_t)wc, 0);
  if (n == (size_t)-1) {
    return WEOF;
  }
  while (n > 0) {
    if (ungetc((unsigned char)mb[--n], stream) == EOF) {
      return WEOF;
    }
  }
  return wc;
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
