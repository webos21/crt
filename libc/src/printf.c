#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct printf_buffer {
  char* data;
  size_t capacity;
  size_t length;
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

static void format_unsigned(struct printf_buffer* buffer, unsigned long long value, unsigned int base,
                            int uppercase) {
  char digits[32];
  const char* table = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
  size_t count = 0;

  if (value == 0) {
    buffer_putc(buffer, '0');
    return;
  }

  while (value != 0) {
    digits[count++] = table[value % base];
    value /= base;
  }

  while (count != 0) {
    buffer_putc(buffer, digits[--count]);
  }
}

static void format_signed(struct printf_buffer* buffer, long long value) {
  unsigned long long magnitude;

  if (value < 0) {
    buffer_putc(buffer, '-');
    magnitude = (unsigned long long)(-(value + 1)) + 1;
  } else {
    magnitude = (unsigned long long)value;
  }
  format_unsigned(buffer, magnitude, 10, 0);
}

int vsnprintf(char* s, size_t n, const char* format, va_list ap) {
  struct printf_buffer buffer;

  buffer.data = s;
  buffer.capacity = n;
  buffer.length = 0;

  while (*format != 0) {
    int long_count = 0;

    if (*format != '%') {
      buffer_putc(&buffer, *format++);
      continue;
    }

    format++;
    if (*format == '%') {
      buffer_putc(&buffer, *format++);
      continue;
    }

    while (*format == 'l') {
      long_count++;
      format++;
    }

    switch (*format) {
      case 'c': {
        buffer_putc(&buffer, (char)va_arg(ap, int));
        break;
      }
      case 's': {
        const char* value = va_arg(ap, const char*);
        if (value == 0) {
          value = "(null)";
        }
        buffer_write(&buffer, value, strlen(value));
        break;
      }
      case 'd':
      case 'i': {
        long long value;
        if (long_count >= 2) {
          value = va_arg(ap, long long);
        } else if (long_count == 1) {
          value = va_arg(ap, long);
        } else {
          value = va_arg(ap, int);
        }
        format_signed(&buffer, value);
        break;
      }
      case 'u':
      case 'x':
      case 'X': {
        unsigned long long value;
        if (long_count >= 2) {
          value = va_arg(ap, unsigned long long);
        } else if (long_count == 1) {
          value = va_arg(ap, unsigned long);
        } else {
          value = va_arg(ap, unsigned int);
        }
        format_unsigned(&buffer, value, *format == 'u' ? 10u : 16u, *format == 'X');
        break;
      }
      case 'p': {
        uintptr_t value = (uintptr_t)va_arg(ap, void*);
        buffer_write(&buffer, "0x", 2);
        format_unsigned(&buffer, value, 16, 0);
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
  char buffer[512];
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
  char buffer[512];
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
