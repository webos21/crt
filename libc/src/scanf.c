#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

enum scan_length {
  SCAN_LENGTH_NONE,
  SCAN_LENGTH_HH,
  SCAN_LENGTH_H,
  SCAN_LENGTH_L,
  SCAN_LENGTH_LL,
  SCAN_LENGTH_Z,
  SCAN_LENGTH_J,
  SCAN_LENGTH_T
};

struct scan_source {
  const char* string;
  FILE* stream;
  int string_mode;
  size_t pos;
  size_t consumed;
};

static int source_getc(struct scan_source* source) {
  int ch;

  if (source->string_mode) {
    ch = (unsigned char)source->string[source->pos];
    if (ch == 0) {
      return EOF;
    }
    ++source->pos;
  } else {
    ch = fgetc(source->stream);
    if (ch == EOF) {
      return EOF;
    }
  }
  ++source->consumed;
  return ch;
}

static void source_ungetc(struct scan_source* source, int ch) {
  if (ch == EOF) {
    return;
  }
  if (source->string_mode) {
    --source->pos;
  } else {
    ungetc(ch, source->stream);
  }
  --source->consumed;
}

static void source_skip_space(struct scan_source* source) {
  int ch;

  do {
    ch = source_getc(source);
  } while (ch != EOF && isspace((unsigned char)ch));
  source_ungetc(source, ch);
}

static wint_t source_getwc_utf8(struct scan_source* source) {
  char bytes[4];
  mbstate_t state = {0, 0, 0};
  size_t used;

  for (used = 0; used < sizeof(bytes); ++used) {
    wchar_t wc;
    size_t result;
    int ch = source_getc(source);

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
  return WEOF;
}

static int parse_width(const char** format) {
  int width = 0;

  while (isdigit((unsigned char)**format)) {
    width = width * 10 + (**format - '0');
    ++*format;
  }
  return width;
}

static enum scan_length parse_length(const char** format) {
  if ((*format)[0] == 'h' && (*format)[1] == 'h') {
    *format += 2;
    return SCAN_LENGTH_HH;
  }
  if ((*format)[0] == 'l' && (*format)[1] == 'l') {
    *format += 2;
    return SCAN_LENGTH_LL;
  }
  if (**format == 'h') {
    ++*format;
    return SCAN_LENGTH_H;
  }
  if (**format == 'l') {
    ++*format;
    return SCAN_LENGTH_L;
  }
  if (**format == 'z') {
    ++*format;
    return SCAN_LENGTH_Z;
  }
  if (**format == 'j') {
    ++*format;
    return SCAN_LENGTH_J;
  }
  if (**format == 't') {
    ++*format;
    return SCAN_LENGTH_T;
  }
  return SCAN_LENGTH_NONE;
}

static void assign_signed(va_list* ap, enum scan_length length, long long value) {
  if (length == SCAN_LENGTH_HH) {
    *va_arg(*ap, signed char*) = (signed char)value;
  } else if (length == SCAN_LENGTH_H) {
    *va_arg(*ap, short*) = (short)value;
  } else if (length == SCAN_LENGTH_L) {
    *va_arg(*ap, long*) = (long)value;
  } else if (length == SCAN_LENGTH_LL || length == SCAN_LENGTH_J) {
    *va_arg(*ap, long long*) = value;
  } else if (length == SCAN_LENGTH_Z) {
    *va_arg(*ap, ssize_t*) = (ssize_t)value;
  } else if (length == SCAN_LENGTH_T) {
    *va_arg(*ap, ptrdiff_t*) = (ptrdiff_t)value;
  } else {
    *va_arg(*ap, int*) = (int)value;
  }
}

static void assign_unsigned(va_list* ap, enum scan_length length, unsigned long long value) {
  if (length == SCAN_LENGTH_HH) {
    *va_arg(*ap, unsigned char*) = (unsigned char)value;
  } else if (length == SCAN_LENGTH_H) {
    *va_arg(*ap, unsigned short*) = (unsigned short)value;
  } else if (length == SCAN_LENGTH_L) {
    *va_arg(*ap, unsigned long*) = (unsigned long)value;
  } else if (length == SCAN_LENGTH_LL || length == SCAN_LENGTH_J) {
    *va_arg(*ap, unsigned long long*) = value;
  } else if (length == SCAN_LENGTH_Z) {
    *va_arg(*ap, size_t*) = (size_t)value;
  } else if (length == SCAN_LENGTH_T) {
    *va_arg(*ap, ptrdiff_t*) = (ptrdiff_t)value;
  } else {
    *va_arg(*ap, unsigned int*) = (unsigned int)value;
  }
}

static int read_token(struct scan_source* source, char* token, size_t token_size, int width, int skip_ws) {
  size_t used = 0;
  int ch;

  if (skip_ws) {
    source_skip_space(source);
  }
  while (used + 1 < token_size && (width == 0 || (int)used < width)) {
    ch = source_getc(source);
    if (ch == EOF) {
      break;
    }
    if (isspace((unsigned char)ch)) {
      source_ungetc(source, ch);
      break;
    }
    token[used++] = (char)ch;
  }
  token[used] = 0;
  return (int)used;
}

static int scan_integer(struct scan_source* source, int width, int base, int is_signed,
                        enum scan_length length, int suppress, va_list* ap) {
  char token[256];
  char* end = 0;
  int used = read_token(source, token, sizeof(token), width, 1);
  int extra;

  if (used == 0) {
    return -1;
  }
  if (is_signed) {
    long long value = strtoll(token, &end, base);
    if (end == token) {
      extra = used - (int)(end - token);
      while (extra-- > 0) {
        source_ungetc(source, token[--used]);
      }
      return -1;
    }
    extra = used - (int)(end - token);
    while (extra-- > 0) {
      source_ungetc(source, token[--used]);
    }
    if (!suppress) {
      assign_signed(ap, length, value);
      return 1;
    }
  } else {
    unsigned long long value = strtoull(token, &end, base);
    if (end == token) {
      extra = used - (int)(end - token);
      while (extra-- > 0) {
        source_ungetc(source, token[--used]);
      }
      return -1;
    }
    extra = used - (int)(end - token);
    while (extra-- > 0) {
      source_ungetc(source, token[--used]);
    }
    if (!suppress) {
      assign_unsigned(ap, length, value);
      return 1;
    }
  }
  return 0;
}

static int scan_float(struct scan_source* source, int width, enum scan_length length, int suppress, va_list* ap) {
  char token[256];
  char* end = 0;
  int used = read_token(source, token, sizeof(token), width, 1);
  int extra;
  double value;

  if (used == 0) {
    return -1;
  }
  value = strtod(token, &end);
  if (end == token) {
    return -1;
  }
  extra = used - (int)(end - token);
  while (extra-- > 0) {
    source_ungetc(source, token[--used]);
  }
  if (!suppress) {
    if (length == SCAN_LENGTH_L) {
      *va_arg(*ap, double*) = value;
    } else {
      *va_arg(*ap, float*) = (float)value;
    }
    return 1;
  }
  return 0;
}

static int scan_string(struct scan_source* source, int width, int suppress, va_list* ap) {
  char* out = suppress ? 0 : va_arg(*ap, char*);
  int count = 0;
  int ch;

  source_skip_space(source);
  while (width == 0 || count < width) {
    ch = source_getc(source);
    if (ch == EOF) {
      break;
    }
    if (isspace((unsigned char)ch)) {
      source_ungetc(source, ch);
      break;
    }
    if (!suppress) {
      out[count] = (char)ch;
    }
    ++count;
  }
  if (count == 0) {
    return -1;
  }
  if (!suppress) {
    out[count] = 0;
    return 1;
  }
  return 0;
}

static int scan_wide_string(struct scan_source* source, int width, int suppress, va_list* ap) {
  wchar_t* out = suppress ? 0 : va_arg(*ap, wchar_t*);
  int count = 0;
  wint_t wc;

  source_skip_space(source);
  while (width == 0 || count < width) {
    wc = source_getwc_utf8(source);
    if (wc == WEOF) {
      break;
    }
    if (wc >= 0 && wc <= 0x7f && isspace((unsigned char)wc)) {
      source_ungetc(source, (int)wc);
      break;
    }
    if (!suppress) {
      out[count] = (wchar_t)wc;
    }
    ++count;
  }
  if (count == 0) {
    return -1;
  }
  if (!suppress) {
    out[count] = 0;
    return 1;
  }
  return 0;
}

static int scan_chars(struct scan_source* source, int width, int suppress, va_list* ap) {
  char* out = suppress ? 0 : va_arg(*ap, char*);
  int count;
  int ch;

  if (width == 0) {
    width = 1;
  }
  for (count = 0; count < width; ++count) {
    ch = source_getc(source);
    if (ch == EOF) {
      return count == 0 ? -1 : 0;
    }
    if (!suppress) {
      out[count] = (char)ch;
    }
  }
  return suppress ? 0 : 1;
}

static int scan_wide_chars(struct scan_source* source, int width, int suppress, va_list* ap) {
  wchar_t* out = suppress ? 0 : va_arg(*ap, wchar_t*);
  int count;

  if (width == 0) {
    width = 1;
  }
  for (count = 0; count < width; ++count) {
    wint_t wc = source_getwc_utf8(source);
    if (wc == WEOF) {
      return count == 0 ? -1 : 0;
    }
    if (!suppress) {
      out[count] = (wchar_t)wc;
    }
  }
  return suppress ? 0 : 1;
}

static int scan_set(struct scan_source* source, const char** format, int width, int suppress, va_list* ap) {
  int table[256] = {0};
  int invert = 0;
  int count = 0;
  int ch;
  char* out = suppress ? 0 : va_arg(*ap, char*);

  if (**format == '^') {
    invert = 1;
    ++*format;
  }
  if (**format == ']') {
    table[(unsigned char)']'] = 1;
    ++*format;
  }
  while (**format != 0 && **format != ']') {
    table[(unsigned char)**format] = 1;
    ++*format;
  }
  if (**format == ']') {
    ++*format;
  }

  while (width == 0 || count < width) {
    ch = source_getc(source);
    if (ch == EOF) {
      break;
    }
    if ((table[(unsigned char)ch] != 0) == invert) {
      source_ungetc(source, ch);
      break;
    }
    if (!suppress) {
      out[count] = (char)ch;
    }
    ++count;
  }
  if (count == 0) {
    return -1;
  }
  if (!suppress) {
    out[count] = 0;
    return 1;
  }
  return 0;
}

static int scan_wide_set(struct scan_source* source, const char** format, int width, int suppress, va_list* ap) {
  int table[256] = {0};
  int invert = 0;
  int count = 0;
  wchar_t* out = suppress ? 0 : va_arg(*ap, wchar_t*);

  if (**format == '^') {
    invert = 1;
    ++*format;
  }
  if (**format == ']') {
    table[(unsigned char)']'] = 1;
    ++*format;
  }
  while (**format != 0 && **format != ']') {
    table[(unsigned char)**format] = 1;
    ++*format;
  }
  if (**format == ']') {
    ++*format;
  }

  while (width == 0 || count < width) {
    wint_t wc = source_getwc_utf8(source);

    if (wc == WEOF) {
      break;
    }
    if ((wc < 0 || wc > 255 || table[(unsigned char)wc] == 0) != invert) {
      if (wc >= 0 && wc <= 255) {
        source_ungetc(source, (int)wc);
      }
      break;
    }
    if (!suppress) {
      out[count] = (wchar_t)wc;
    }
    ++count;
  }
  if (count == 0) {
    return -1;
  }
  if (!suppress) {
    out[count] = 0;
    return 1;
  }
  return 0;
}

static int vscan_core(struct scan_source* source, const char* format, va_list ap) {
  int assigned = 0;

  while (*format != 0) {
    int suppress = 0;
    int width = 0;
    enum scan_length length;
    int result = 0;
    char spec;
    int ch;

    if (isspace((unsigned char)*format)) {
      while (isspace((unsigned char)*format)) {
        ++format;
      }
      source_skip_space(source);
      continue;
    }
    if (*format != '%') {
      ch = source_getc(source);
      if (ch != (unsigned char)*format) {
        source_ungetc(source, ch);
        break;
      }
      ++format;
      continue;
    }

    ++format;
    if (*format == '%') {
      ch = source_getc(source);
      if (ch != '%') {
        source_ungetc(source, ch);
        break;
      }
      ++format;
      continue;
    }
    if (*format == '*') {
      suppress = 1;
      ++format;
    }
    width = parse_width(&format);
    length = parse_length(&format);
    spec = *format++;

    if (spec == 'd') {
      result = scan_integer(source, width, 10, 1, length, suppress, &ap);
    } else if (spec == 'i') {
      result = scan_integer(source, width, 0, 1, length, suppress, &ap);
    } else if (spec == 'u') {
      result = scan_integer(source, width, 10, 0, length, suppress, &ap);
    } else if (spec == 'x' || spec == 'X' || spec == 'p') {
      result = scan_integer(source, width, 16, 0, length, suppress, &ap);
    } else if (spec == 'o') {
      result = scan_integer(source, width, 8, 0, length, suppress, &ap);
    } else if (spec == 'a' || spec == 'A' || spec == 'e' || spec == 'E' ||
               spec == 'f' || spec == 'F' || spec == 'g' || spec == 'G') {
      result = scan_float(source, width, length, suppress, &ap);
    } else if (spec == 's') {
      if (length == SCAN_LENGTH_L) {
        result = scan_wide_string(source, width, suppress, &ap);
      } else {
        result = scan_string(source, width, suppress, &ap);
      }
    } else if (spec == 'c') {
      if (length == SCAN_LENGTH_L) {
        result = scan_wide_chars(source, width, suppress, &ap);
      } else {
        result = scan_chars(source, width, suppress, &ap);
      }
    } else if (spec == '[') {
      if (length == SCAN_LENGTH_L) {
        result = scan_wide_set(source, &format, width, suppress, &ap);
      } else {
        result = scan_set(source, &format, width, suppress, &ap);
      }
    } else if (spec == 'n') {
      if (!suppress) {
        assign_signed(&ap, length, (long long)source->consumed);
      }
      result = 0;
    } else {
      break;
    }

    if (result < 0) {
      return assigned == 0 ? EOF : assigned;
    }
    assigned += result;
  }
  return assigned;
}

int vfscanf(FILE* stream, const char* format, va_list ap) {
  struct scan_source source;

  source.string = 0;
  source.stream = stream;
  source.string_mode = 0;
  source.pos = 0;
  source.consumed = 0;
  return vscan_core(&source, format, ap);
}

int vscanf(const char* format, va_list ap) {
  return vfscanf(stdin, format, ap);
}

int vsscanf(const char* s, const char* format, va_list ap) {
  struct scan_source source;

  source.string = s;
  source.stream = 0;
  source.string_mode = 1;
  source.pos = 0;
  source.consumed = 0;
  return vscan_core(&source, format, ap);
}

int fscanf(FILE* stream, const char* format, ...) {
  va_list ap;
  int result;

  va_start(ap, format);
  result = vfscanf(stream, format, ap);
  va_end(ap);
  return result;
}

int scanf(const char* format, ...) {
  va_list ap;
  int result;

  va_start(ap, format);
  result = vscanf(format, ap);
  va_end(ap);
  return result;
}

int sscanf(const char* s, const char* format, ...) {
  va_list ap;
  int result;

  va_start(ap, format);
  result = vsscanf(s, format, ap);
  va_end(ap);
  return result;
}
