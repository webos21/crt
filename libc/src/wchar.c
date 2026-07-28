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
