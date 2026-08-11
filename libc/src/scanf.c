#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <wchar.h>

enum scan_length {
  SCAN_LENGTH_NONE,
  SCAN_LENGTH_HH,
  SCAN_LENGTH_H,
  SCAN_LENGTH_L,
  SCAN_LENGTH_CAPITAL_L,
  SCAN_LENGTH_LL,
  SCAN_LENGTH_Z,
  SCAN_LENGTH_J,
  SCAN_LENGTH_T,
  SCAN_LENGTH_POINTER
};

#define SCANF_SIGNOK 0x01
#define SCANF_HAVESIGN 0x02
#define SCANF_NDIGITS 0x04
#define SCANF_PFXOK 0x08
#define SCANF_PFBOK 0x10
#define SCANF_NZDIGITS 0x20

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

static int source_peekc(struct scan_source* source) {
  int ch = source_getc(source);

  source_ungetc(source, ch);
  return ch;
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

static enum scan_length scan_w_to_length(int size, int fast) {
  enum scan_length fast_length = sizeof(void*) == 8 ? SCAN_LENGTH_LL : SCAN_LENGTH_NONE;

  if (size == 8) {
    return SCAN_LENGTH_HH;
  }
  if (size == 16) {
    return fast ? fast_length : SCAN_LENGTH_H;
  }
  if (size == 32) {
    return fast ? fast_length : SCAN_LENGTH_NONE;
  }
  if (size == 64) {
    return SCAN_LENGTH_LL;
  }
  return SCAN_LENGTH_NONE;
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
  if (**format == 'L') {
    ++*format;
    return SCAN_LENGTH_CAPITAL_L;
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
  if (length == SCAN_LENGTH_POINTER) {
    *va_arg(*ap, void**) = (void*)(uintptr_t)value;
  } else if (length == SCAN_LENGTH_HH) {
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

static int scan_integer_digit_value(int ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'A' && ch <= 'Z') {
    return ch - 'A' + 10;
  }
  if (ch >= 'a' && ch <= 'z') {
    return ch - 'a' + 10;
  }
  return -1;
}

static int scan_integer_token(
    struct scan_source* source,
    char* token,
    size_t token_size,
    int width,
    int base,
    int* final_base) {
  char* p = token;
  size_t remaining;
  int flags = SCANF_SIGNOK | SCANF_NDIGITS | SCANF_NZDIGITS;
  int ch = 0;

  if (width == 0 || (size_t)width > token_size - 1) {
    remaining = token_size - 1;
  } else {
    remaining = (size_t)width;
  }
  if (base == 0) {
    flags |= SCANF_PFXOK | SCANF_PFBOK;
  } else if (base == 16) {
    flags |= SCANF_PFXOK;
  } else if (base == 2) {
    flags |= SCANF_PFBOK;
  }

  while (remaining != 0) {
    int digit;

    ch = source_getc(source);
    if (ch == EOF) {
      break;
    }
    switch (ch) {
      case '0':
        if (base == 0) {
          base = 8;
          flags |= SCANF_PFBOK | SCANF_PFXOK;
        }
        if (flags & SCANF_NZDIGITS) {
          flags &= ~(SCANF_SIGNOK | SCANF_NZDIGITS | SCANF_NDIGITS);
        } else {
          flags &= ~(SCANF_SIGNOK | SCANF_PFBOK | SCANF_PFXOK | SCANF_NDIGITS);
        }
        break;
      case 'b':
      case 'B':
        if ((flags & SCANF_PFBOK) && p == token + 1 + ((flags & SCANF_HAVESIGN) != 0)) {
          base = 2;
          flags &= ~SCANF_PFBOK;
          break;
        }
        digit = scan_integer_digit_value(ch);
        if (base == 0) {
          base = 10;
        }
        if (digit < 0 || digit >= base) {
          source_ungetc(source, ch);
          goto done;
        }
        flags &= ~(SCANF_SIGNOK | SCANF_PFBOK | SCANF_PFXOK | SCANF_NDIGITS);
        break;
      case 'x':
      case 'X':
        if ((flags & SCANF_PFXOK) && p == token + 1 + ((flags & SCANF_HAVESIGN) != 0)) {
          base = 16;
          flags &= ~SCANF_PFXOK;
          break;
        }
        digit = scan_integer_digit_value(ch);
        if (base == 0) {
          base = 10;
        }
        if (digit < 0 || digit >= base) {
          source_ungetc(source, ch);
          goto done;
        }
        flags &= ~(SCANF_SIGNOK | SCANF_PFBOK | SCANF_PFXOK | SCANF_NDIGITS);
        break;
      case '+':
      case '-':
        if (flags & SCANF_SIGNOK) {
          flags &= ~SCANF_SIGNOK;
          flags |= SCANF_HAVESIGN;
          break;
        }
        source_ungetc(source, ch);
        goto done;
      default:
        digit = scan_integer_digit_value(ch);
        if (base == 0) {
          base = 10;
        }
        if (digit < 0 || digit >= base) {
          source_ungetc(source, ch);
          goto done;
        }
        flags &= ~(SCANF_SIGNOK | SCANF_PFBOK | SCANF_PFXOK | SCANF_NDIGITS);
        break;
    }
    *p++ = (char)ch;
    --remaining;
  }

done:
  if (base == 0) {
    base = 10;
  }
  if (flags & SCANF_NDIGITS) {
    if (p > token) {
      source_ungetc(source, (unsigned char)*--p);
    }
    return -1;
  }
  if (p > token) {
    ch = (unsigned char)p[-1];
    if ((base == 2 && (ch == 'b' || ch == 'B')) || ch == 'x' || ch == 'X') {
      --p;
      source_ungetc(source, ch);
    }
  }
  *p = 0;
  *final_base = base;
  return (int)(p - token);
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
  char converted_token[256];
  char* parse_token = token;
  int used;
  int final_base = base;

  source_skip_space(source);
  if (source_peekc(source) == EOF) {
    return -2;
  }
  used = scan_integer_token(source, token, sizeof(token), width, base, &final_base);
  if (used == 0) {
    return -2;
  }
  if (used < 0) {
    return -1;
  }
  if (final_base == 2) {
    const char* in = token;
    char* out = converted_token;

    if (*in == '+' || *in == '-') {
      *out++ = *in++;
    }
    if (in[0] == '0' && (in[1] == 'b' || in[1] == 'B')) {
      in += 2;
    }
    while (*in != 0) {
      *out++ = *in++;
    }
    *out = 0;
    parse_token = converted_token;
  }
  if (is_signed) {
    intmax_t value = strtoimax(parse_token, 0, final_base);

    if (!suppress) {
      assign_signed(ap, length, (long long)value);
      return 1;
    }
  } else {
    uintmax_t value = strtoumax(parse_token, 0, final_base);

    if (!suppress) {
      assign_unsigned(ap, length, (unsigned long long)value);
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

  if (used == 0) {
    return -2;
  }
  if (length == SCAN_LENGTH_CAPITAL_L) {
    long double value = strtold(token, &end);

    if (end == token) {
      return -1;
    }
    extra = used - (int)(end - token);
    while (extra-- > 0) {
      source_ungetc(source, token[--used]);
    }
    if (!suppress) {
      *va_arg(*ap, long double*) = value;
      return 1;
    }
  } else {
    double value = strtod(token, &end);

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
  }
  return 0;
}

static int scan_string(struct scan_source* source, int width, int suppress, int allocate, va_list* ap) {
  char stack_buffer[256];
  char* heap_buffer = 0;
  char* out = 0;
  size_t capacity = sizeof(stack_buffer);
  int count = 0;
  int ch;

  if (!suppress) {
    if (allocate) {
      out = stack_buffer;
    } else {
      out = va_arg(*ap, char*);
    }
  }
  source_skip_space(source);
  if (source_peekc(source) == EOF) {
    return -2;
  }
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
      if (allocate && (size_t)count + 2 > capacity) {
        char* grown;
        capacity *= 2;
        grown = heap_buffer == 0 ? (char*)malloc(capacity) : (char*)realloc(heap_buffer, capacity);
        if (grown == 0) {
          free(heap_buffer);
          errno = ENOMEM;
          return -1;
        }
        if (heap_buffer == 0) {
          memcpy(grown, stack_buffer, (size_t)count);
        }
        heap_buffer = grown;
        out = heap_buffer;
      }
      out[count] = (char)ch;
    }
    ++count;
  }
  if (count == 0) {
    free(heap_buffer);
    return -1;
  }
  if (!suppress) {
    char** allocated_out;

    out[count] = 0;
    if (allocate) {
      allocated_out = va_arg(*ap, char**);
      if (heap_buffer == 0) {
        heap_buffer = (char*)malloc((size_t)count + 1);
        if (heap_buffer == 0) {
          errno = ENOMEM;
          return -1;
        }
        memcpy(heap_buffer, stack_buffer, (size_t)count + 1);
      }
      *allocated_out = heap_buffer;
    }
    return 1;
  }
  return 0;
}

static int scan_wide_string(struct scan_source* source, int width, int suppress, int allocate, va_list* ap) {
  wchar_t stack_buffer[128];
  wchar_t* heap_buffer = 0;
  wchar_t* out = 0;
  size_t capacity = sizeof(stack_buffer) / sizeof(stack_buffer[0]);
  int count = 0;
  wint_t wc;

  if (!suppress) {
    if (allocate) {
      out = stack_buffer;
    } else {
      out = va_arg(*ap, wchar_t*);
    }
  }
  source_skip_space(source);
  if (source_peekc(source) == EOF) {
    return -2;
  }
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
      if (allocate && (size_t)count + 2 > capacity) {
        wchar_t* grown;
        capacity *= 2;
        grown = heap_buffer == 0 ? (wchar_t*)malloc(capacity * sizeof(wchar_t)) :
            (wchar_t*)realloc(heap_buffer, capacity * sizeof(wchar_t));
        if (grown == 0) {
          free(heap_buffer);
          errno = ENOMEM;
          return -1;
        }
        if (heap_buffer == 0) {
          memcpy(grown, stack_buffer, (size_t)count * sizeof(wchar_t));
        }
        heap_buffer = grown;
        out = heap_buffer;
      }
      out[count] = (wchar_t)wc;
    }
    ++count;
  }
  if (count == 0) {
    free(heap_buffer);
    return -1;
  }
  if (!suppress) {
    wchar_t** allocated_out;

    out[count] = 0;
    if (allocate) {
      allocated_out = va_arg(*ap, wchar_t**);
      if (heap_buffer == 0) {
        heap_buffer = (wchar_t*)malloc(((size_t)count + 1) * sizeof(wchar_t));
        if (heap_buffer == 0) {
          errno = ENOMEM;
          return -1;
        }
        memcpy(heap_buffer, stack_buffer, ((size_t)count + 1) * sizeof(wchar_t));
      }
      *allocated_out = heap_buffer;
    }
    return 1;
  }
  return 0;
}

static int scan_chars(struct scan_source* source, int width, int suppress, int allocate, va_list* ap) {
  char stack_buffer[256];
  char* heap_buffer = 0;
  char* out = 0;
  int count;
  int ch;

  if (width == 0) {
    width = 1;
  }
  if (!suppress) {
    if (allocate) {
      out = width < (int)sizeof(stack_buffer) ? stack_buffer : (char*)malloc((size_t)width + 1);
      if (out == 0) {
        errno = ENOMEM;
        return -1;
      }
      if (out != stack_buffer) {
        heap_buffer = out;
      }
    } else {
      out = va_arg(*ap, char*);
    }
  }
  for (count = 0; count < width; ++count) {
    ch = source_getc(source);
    if (ch == EOF) {
      free(heap_buffer);
      if (count == 0) {
        return -2;
      }
      break;
    }
    if (!suppress) {
      out[count] = (char)ch;
    }
  }
  if (!suppress && allocate) {
    char** allocated_out = va_arg(*ap, char**);

    out[count] = 0;
    if (heap_buffer == 0) {
      heap_buffer = (char*)malloc((size_t)count + 1);
      if (heap_buffer == 0) {
        errno = ENOMEM;
        return -1;
      }
      memcpy(heap_buffer, stack_buffer, (size_t)count + 1);
    }
    *allocated_out = heap_buffer;
  }
  return suppress ? 0 : 1;
}

static int scan_wide_chars(struct scan_source* source, int width, int suppress, int allocate, va_list* ap) {
  wchar_t stack_buffer[128];
  wchar_t* heap_buffer = 0;
  wchar_t* out = 0;
  int count;

  if (width == 0) {
    width = 1;
  }
  if (!suppress) {
    if (allocate) {
      out = width < (int)(sizeof(stack_buffer) / sizeof(stack_buffer[0])) ? stack_buffer :
          (wchar_t*)malloc(((size_t)width + 1) * sizeof(wchar_t));
      if (out == 0) {
        errno = ENOMEM;
        return -1;
      }
      if (out != stack_buffer) {
        heap_buffer = out;
      }
    } else {
      out = va_arg(*ap, wchar_t*);
    }
  }
  for (count = 0; count < width; ++count) {
    wint_t wc = source_getwc_utf8(source);
    if (wc == WEOF) {
      free(heap_buffer);
      if (count == 0) {
        return -2;
      }
      break;
    }
    if (!suppress) {
      out[count] = (wchar_t)wc;
    }
  }
  if (!suppress && allocate) {
    wchar_t** allocated_out = va_arg(*ap, wchar_t**);

    out[count] = 0;
    if (heap_buffer == 0) {
      heap_buffer = (wchar_t*)malloc(((size_t)count + 1) * sizeof(wchar_t));
      if (heap_buffer == 0) {
        errno = ENOMEM;
        return -1;
      }
      memcpy(heap_buffer, stack_buffer, ((size_t)count + 1) * sizeof(wchar_t));
    }
    *allocated_out = heap_buffer;
  }
  return suppress ? 0 : 1;
}

static const char* scan_sccl(char* table, const char* format) {
  int c;
  int n;
  int value;

  c = (unsigned char)*format++;
  if (c == '^') {
    value = 1;
    c = (unsigned char)*format++;
  } else {
    value = 0;
  }
  memset(table, value, 256);
  if (c == 0) {
    return format - 1;
  }

  value = 1 - value;
  for (;;) {
    table[c] = (char)value;
doswitch:
    n = (unsigned char)*format++;
    switch (n) {
      case 0:
        return format - 1;
      case '-':
        n = (unsigned char)*format;
        if (n == ']' || n < c) {
          c = '-';
          break;
        }
        ++format;
        do {
          table[++c] = (char)value;
        } while (c < n);
        goto doswitch;
      case ']':
        return format;
      default:
        c = n;
        break;
    }
  }
}

static int scan_set(struct scan_source* source, const char** format, int width, int suppress, int allocate, va_list* ap) {
  char table[256];
  int count = 0;
  int ch;
  char stack_buffer[256];
  char* heap_buffer = 0;
  char* out = 0;
  size_t capacity = sizeof(stack_buffer);

  *format = scan_sccl(table, *format);

  if (source_peekc(source) == EOF) {
    return -2;
  }

  if (!suppress) {
    if (allocate) {
      out = stack_buffer;
    } else {
      out = va_arg(*ap, char*);
    }
  }
  while (width == 0 || count < width) {
    ch = source_getc(source);
    if (ch == EOF) {
      break;
    }
    if (table[(unsigned char)ch] == 0) {
      source_ungetc(source, ch);
      break;
    }
    if (!suppress) {
      if (allocate && (size_t)count + 2 > capacity) {
        char* grown;
        capacity *= 2;
        grown = heap_buffer == 0 ? (char*)malloc(capacity) : (char*)realloc(heap_buffer, capacity);
        if (grown == 0) {
          free(heap_buffer);
          errno = ENOMEM;
          return -1;
        }
        if (heap_buffer == 0) {
          memcpy(grown, stack_buffer, (size_t)count);
        }
        heap_buffer = grown;
        out = heap_buffer;
      }
      out[count] = (char)ch;
    }
    ++count;
  }
  if (count == 0) {
    free(heap_buffer);
    return -1;
  }
  if (!suppress) {
    char** allocated_out;

    out[count] = 0;
    if (allocate) {
      allocated_out = va_arg(*ap, char**);
      if (heap_buffer == 0) {
        heap_buffer = (char*)malloc((size_t)count + 1);
        if (heap_buffer == 0) {
          errno = ENOMEM;
          return -1;
        }
        memcpy(heap_buffer, stack_buffer, (size_t)count + 1);
      }
      *allocated_out = heap_buffer;
    }
    return 1;
  }
  return 0;
}

static int scan_wide_set(struct scan_source* source, const char** format, int width, int suppress, int allocate, va_list* ap) {
  char table[256];
  int count = 0;
  wchar_t stack_buffer[128];
  wchar_t* heap_buffer = 0;
  wchar_t* out = 0;
  size_t capacity = sizeof(stack_buffer) / sizeof(stack_buffer[0]);

  *format = scan_sccl(table, *format);

  if (source_peekc(source) == EOF) {
    return -2;
  }

  if (!suppress) {
    if (allocate) {
      out = stack_buffer;
    } else {
      out = va_arg(*ap, wchar_t*);
    }
  }
  while (width == 0 || count < width) {
    wint_t wc = source_getwc_utf8(source);

    if (wc == WEOF) {
      break;
    }
    if (wc < 0 || wc > 255 || table[(unsigned char)wc] == 0) {
      if (wc >= 0 && wc <= 255) {
        source_ungetc(source, (int)wc);
      }
      break;
    }
    if (!suppress) {
      if (allocate && (size_t)count + 2 > capacity) {
        wchar_t* grown;
        capacity *= 2;
        grown = heap_buffer == 0 ? (wchar_t*)malloc(capacity * sizeof(wchar_t)) :
            (wchar_t*)realloc(heap_buffer, capacity * sizeof(wchar_t));
        if (grown == 0) {
          free(heap_buffer);
          errno = ENOMEM;
          return -1;
        }
        if (heap_buffer == 0) {
          memcpy(grown, stack_buffer, (size_t)count * sizeof(wchar_t));
        }
        heap_buffer = grown;
        out = heap_buffer;
      }
      out[count] = (wchar_t)wc;
    }
    ++count;
  }
  if (count == 0) {
    free(heap_buffer);
    return -1;
  }
  if (!suppress) {
    wchar_t** allocated_out;

    out[count] = 0;
    if (allocate) {
      allocated_out = va_arg(*ap, wchar_t**);
      if (heap_buffer == 0) {
        heap_buffer = (wchar_t*)malloc(((size_t)count + 1) * sizeof(wchar_t));
        if (heap_buffer == 0) {
          errno = ENOMEM;
          return -1;
        }
        memcpy(heap_buffer, stack_buffer, ((size_t)count + 1) * sizeof(wchar_t));
      }
      *allocated_out = heap_buffer;
    }
    return 1;
  }
  return 0;
}

static int vscan_core(struct scan_source* source, const char* format, va_list ap) {
  int assigned = 0;
  /* Do not take &ap directly: `ap` is a *function parameter* of type
   * va_list, and on ABIs where va_list is itself an array type (x86_64
   * SysV: `typedef struct __va_list_tag va_list[1];`), array-typed
   * parameters implicitly decay to a pointer -- so `ap` here is already
   * effectively a `struct __va_list_tag *`, and `&ap` would give a
   * spurious extra level of indirection (`struct __va_list_tag **`),
   * one level more than every scan_*() helper below actually expects
   * (`va_list *`). This was invisible on aarch64/most other ABIs, where
   * va_list is a plain (non-array) struct and no such decay happens --
   * `&ap` there already produces exactly `va_list *`. va_copy() into a
   * genuine local variable (never itself a function parameter, so never
   * subject to array-to-pointer decay regardless of va_list's
   * representation) sidesteps the whole issue portably. */
  va_list ap_copy;

  va_copy(ap_copy, ap);

  while (*format != 0) {
    int suppress = 0;
    int allocate = 0;
    int width = 0;
    enum scan_length length = SCAN_LENGTH_NONE;
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
    if (!suppress && *format == 'm') {
      allocate = 1;
      ++format;
    } else if (!suppress && *format == 'a' &&
               (format[1] == 's' || format[1] == 'c' || format[1] == '[')) {
      allocate = 1;
      ++format;
    }
    for (;;) {
      if (*format == 0) {
        va_end(ap_copy);
        return assigned == 0 ? EOF : assigned;
      }
      if (isdigit((unsigned char)*format)) {
        width = parse_width(&format);
        continue;
      }
      if (!suppress && *format == 'm') {
        allocate = 1;
        ++format;
        continue;
      }
      if (!suppress && *format == 'a' &&
          (format[1] == 's' || format[1] == 'c' || format[1] == '[')) {
        allocate = 1;
        ++format;
        continue;
      }
      if (*format == 'q') {
        length = SCAN_LENGTH_LL;
        ++format;
        continue;
      }
      if (*format == 'w') {
        int fast = 0;
        int size = 0;

        ++format;
        if (*format == 'f') {
          fast = 1;
          ++format;
        }
        size = parse_width(&format);
        length = scan_w_to_length(size, fast);
        continue;
      }
      {
        enum scan_length parsed_length = parse_length(&format);
        if (parsed_length != SCAN_LENGTH_NONE) {
          length = parsed_length;
          continue;
        }
      }
      break;
    }
    spec = *format++;

    if (spec == 'D') {
      result = scan_integer(source, width, 10, 1, SCAN_LENGTH_L, suppress, &ap_copy);
    } else if (spec == 'd') {
      result = scan_integer(source, width, 10, 1, length, suppress, &ap_copy);
    } else if (spec == 'i') {
      result = scan_integer(source, width, 0, 1, length, suppress, &ap_copy);
    } else if (spec == 'b') {
      result = scan_integer(source, width, 2, 0, length, suppress, &ap_copy);
    } else if (spec == 'U') {
      result = scan_integer(source, width, 10, 0, SCAN_LENGTH_L, suppress, &ap_copy);
    } else if (spec == 'u') {
      result = scan_integer(source, width, 10, 0, length, suppress, &ap_copy);
    } else if (spec == 'x' || spec == 'X' || spec == 'p') {
      result = scan_integer(
          source, width, 16, 0, spec == 'p' ? SCAN_LENGTH_POINTER : length, suppress, &ap_copy);
    } else if (spec == 'O') {
      result = scan_integer(source, width, 8, 0, SCAN_LENGTH_L, suppress, &ap_copy);
    } else if (spec == 'o') {
      result = scan_integer(source, width, 8, 0, length, suppress, &ap_copy);
    } else if (spec == 'a' || spec == 'A' || spec == 'e' || spec == 'E' ||
               spec == 'f' || spec == 'F' || spec == 'g' || spec == 'G') {
      result = scan_float(source, width, length, suppress, &ap_copy);
    } else if (spec == 's') {
      if (length == SCAN_LENGTH_L) {
        result = scan_wide_string(source, width, suppress, allocate, &ap_copy);
      } else {
        result = scan_string(source, width, suppress, allocate, &ap_copy);
      }
    } else if (spec == 'c') {
      if (length == SCAN_LENGTH_L) {
        result = scan_wide_chars(source, width, suppress, allocate, &ap_copy);
      } else {
        result = scan_chars(source, width, suppress, allocate, &ap_copy);
      }
    } else if (spec == '[') {
      if (length == SCAN_LENGTH_L) {
        result = scan_wide_set(source, &format, width, suppress, allocate, &ap_copy);
      } else {
        result = scan_set(source, &format, width, suppress, allocate, &ap_copy);
      }
    } else if (spec == 'n') {
      if (!suppress) {
        assign_signed(&ap_copy, length, (long long)source->consumed);
      }
      result = 0;
    } else {
      break;
    }

    if (result == -2) {
      va_end(ap_copy);
      return assigned == 0 ? EOF : assigned;
    }
    if (result < 0) {
      va_end(ap_copy);
      return assigned;
    }
    assigned += result;
  }
  va_end(ap_copy);
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
