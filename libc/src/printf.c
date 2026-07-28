#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

struct printf_buffer {
  char* data;
  size_t capacity;
  size_t length;
};

struct printf_spec {
  int left;
  int zero;
  int plus;
  int space;
  int alt;
  int width;
  int precision;
  int precision_set;
  int long_count;
  int size_length;
};

static void buffer_putc(struct printf_buffer* buffer, char c) {
  if (buffer->capacity != 0 && buffer->length + 1 < buffer->capacity) {
    buffer->data[buffer->length] = c;
  }
  buffer->length++;
}

static void buffer_write(struct printf_buffer* buffer, const char* s, size_t n) {
  size_t i;

  for (i = 0; i < n; ++i) {
    buffer_putc(buffer, s[i]);
  }
}

static int is_digit(char c) {
  return c >= '0' && c <= '9';
}

static void parse_decimal(const char** format, int* value) {
  int result = 0;

  while (is_digit(**format)) {
    result = result * 10 + (**format - '0');
    ++*format;
  }
  *value = result;
}

static size_t convert_unsigned(
    char* out,
    unsigned long long value,
    unsigned int base,
    int uppercase) {
  char scratch[64];
  const char* table = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
  size_t count = 0;
  size_t i;

  if (value == 0) {
    out[0] = '0';
    return 1;
  }

  while (value != 0) {
    scratch[count++] = table[value % base];
    value /= base;
  }

  for (i = 0; i < count; ++i) {
    out[i] = scratch[count - i - 1];
  }
  return count;
}

static void write_padding(struct printf_buffer* buffer, char ch, int count) {
  while (count-- > 0) {
    buffer_putc(buffer, ch);
  }
}

static void write_formatted(
    struct printf_buffer* buffer,
    const struct printf_spec* spec,
    const char* prefix,
    size_t prefix_len,
    const char* digits,
    size_t digits_len,
    int negative_zero_value) {
  int zero_count = 0;
  int content_len;
  int pad_count;
  char pad_ch = ' ';

  if (spec->precision_set) {
    if (spec->precision > (int)digits_len) {
      zero_count = spec->precision - (int)digits_len;
    }
  } else if (spec->zero && !spec->left && spec->width > 0) {
    pad_ch = '0';
  }

  if (negative_zero_value && spec->precision_set && spec->precision == 0) {
    digits_len = 0;
  }

  content_len = (int)prefix_len + zero_count + (int)digits_len;
  pad_count = spec->width > content_len ? spec->width - content_len : 0;

  if (!spec->left && pad_ch == ' ') {
    write_padding(buffer, ' ', pad_count);
  }
  if (prefix_len != 0) {
    buffer_write(buffer, prefix, prefix_len);
  }
  if (!spec->left && pad_ch == '0') {
    write_padding(buffer, '0', pad_count);
  }
  write_padding(buffer, '0', zero_count);
  buffer_write(buffer, digits, digits_len);
  if (spec->left) {
    write_padding(buffer, ' ', pad_count);
  }
}

static void format_string(struct printf_buffer* buffer, const struct printf_spec* spec, const char* value) {
  size_t len;
  int pad;

  if (value == 0) {
    value = "(null)";
  }
  len = strlen(value);
  if (spec->precision_set && spec->precision >= 0 && (size_t)spec->precision < len) {
    len = (size_t)spec->precision;
  }
  pad = spec->width > (int)len ? spec->width - (int)len : 0;
  if (!spec->left) {
    write_padding(buffer, ' ', pad);
  }
  buffer_write(buffer, value, len);
  if (spec->left) {
    write_padding(buffer, ' ', pad);
  }
}

static void format_integer(
    struct printf_buffer* buffer,
    const struct printf_spec* spec,
    unsigned long long value,
    int is_signed,
    int negative,
    unsigned int base,
    int uppercase) {
  char digits[64];
  char prefix[3];
  size_t digits_len;
  size_t prefix_len = 0;

  digits_len = convert_unsigned(digits, value, base, uppercase);

  if (is_signed) {
    if (negative) {
      prefix[prefix_len++] = '-';
    } else if (spec->plus) {
      prefix[prefix_len++] = '+';
    } else if (spec->space) {
      prefix[prefix_len++] = ' ';
    }
  } else if (spec->alt && value != 0) {
    if (base == 8) {
      prefix[prefix_len++] = '0';
    } else if (base == 16) {
      prefix[prefix_len++] = '0';
      prefix[prefix_len++] = uppercase ? 'X' : 'x';
    }
  }

  write_formatted(buffer, spec, prefix, prefix_len, digits, digits_len, value == 0);
}

int vsnprintf(char* s, size_t n, const char* format, va_list ap) {
  struct printf_buffer buffer;

  buffer.data = s;
  buffer.capacity = n;
  buffer.length = 0;

  while (*format != 0) {
    struct printf_spec spec = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    if (*format != '%') {
      buffer_putc(&buffer, *format++);
      continue;
    }

    format++;
    if (*format == '%') {
      buffer_putc(&buffer, *format++);
      continue;
    }

    for (;;) {
      if (*format == '-') {
        spec.left = 1;
      } else if (*format == '0') {
        spec.zero = 1;
      } else if (*format == '+') {
        spec.plus = 1;
      } else if (*format == ' ') {
        spec.space = 1;
      } else if (*format == '#') {
        spec.alt = 1;
      } else {
        break;
      }
      format++;
    }

    parse_decimal(&format, &spec.width);
    if (*format == '.') {
      format++;
      spec.precision_set = 1;
      parse_decimal(&format, &spec.precision);
    }

    while (*format == 'l') {
      spec.long_count++;
      format++;
    }
    if (*format == 'z') {
      spec.size_length = 1;
      format++;
    }

    switch (*format) {
      case 'c': {
        char ch = (char)va_arg(ap, int);
        char text[1];
        text[0] = ch;
        write_formatted(&buffer, &spec, 0, 0, text, 1, 0);
        break;
      }
      case 's':
        format_string(&buffer, &spec, va_arg(ap, const char*));
        break;
      case 'd':
      case 'i': {
        long long value;
        unsigned long long magnitude;
        int negative;
        if (spec.size_length) {
          value = (long long)va_arg(ap, ssize_t);
        } else if (spec.long_count >= 2) {
          value = va_arg(ap, long long);
        } else if (spec.long_count == 1) {
          value = va_arg(ap, long);
        } else {
          value = va_arg(ap, int);
        }
        negative = value < 0;
        if (negative) {
          magnitude = (unsigned long long)(-(value + 1)) + 1;
        } else {
          magnitude = (unsigned long long)value;
        }
        format_integer(&buffer, &spec, magnitude, 1, negative, 10, 0);
        break;
      }
      case 'u':
      case 'o':
      case 'x':
      case 'X': {
        unsigned int base = *format == 'o' ? 8u : (*format == 'u' ? 10u : 16u);
        unsigned long long value;
        if (spec.size_length) {
          value = (unsigned long long)va_arg(ap, size_t);
        } else if (spec.long_count >= 2) {
          value = va_arg(ap, unsigned long long);
        } else if (spec.long_count == 1) {
          value = va_arg(ap, unsigned long);
        } else {
          value = va_arg(ap, unsigned int);
        }
        format_integer(
            &buffer,
            &spec,
            value,
            0,
            0,
            base,
            *format == 'X');
        break;
      }
      case 'p': {
        struct printf_spec pointer_spec = spec;
        uintptr_t value = (uintptr_t)va_arg(ap, void*);
        pointer_spec.alt = 1;
        format_integer(&buffer, &pointer_spec, value, 0, 0, 16, 0);
        break;
      }
      default:
        buffer_putc(&buffer, '%');
        if (*format != 0) {
          buffer_putc(&buffer, *format);
        }
        break;
    }

    if (*format != 0) {
      format++;
    }
  }

  if (n != 0) {
    if (buffer.length < n) {
      s[buffer.length] = 0;
    } else {
      s[n - 1] = 0;
    }
  }

  return (int)buffer.length;
}

int snprintf(char* s, size_t n, const char* format, ...) {
  int result;
  va_list ap;

  va_start(ap, format);
  result = vsnprintf(s, n, format, ap);
  va_end(ap);
  return result;
}

int fprintf(FILE* stream, const char* format, ...) {
  char buffer[1024];
  int result;
  va_list ap;

  va_start(ap, format);
  result = vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);
  if (result < 0) {
    return result;
  }
  fwrite(buffer, 1, (size_t)result < sizeof(buffer) ? (size_t)result : sizeof(buffer) - 1, stream);
  return result;
}

int printf(const char* format, ...) {
  char buffer[1024];
  int result;
  va_list ap;

  va_start(ap, format);
  result = vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);
  if (result < 0) {
    return result;
  }
  fwrite(buffer, 1, (size_t)result < sizeof(buffer) ? (size_t)result : sizeof(buffer) - 1, stdout);
  return result;
}
