#include <errno.h>
#include <float.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <wchar.h>

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
  int short_count;
  int size_length;
  int intmax_length;
  int ptrdiff_length;
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

static void format_wide_string(struct printf_buffer* buffer, const struct printf_spec* spec, const wchar_t* value) {
  size_t wide_len;
  size_t byte_len;
  char* narrow;
  size_t i;
  struct printf_spec narrow_spec = *spec;

  if (value == 0) {
    format_string(buffer, spec, "(null)");
    return;
  }
  wide_len = wcslen(value);
  if (spec->precision_set && spec->precision >= 0 && (size_t)spec->precision < wide_len) {
    wide_len = (size_t)spec->precision;
  }
  narrow = (char*)malloc(wide_len * 4 + 1);
  if (narrow == 0) {
    return;
  }
  byte_len = 0;
  for (i = 0; i < wide_len; ++i) {
    char mb[4];
    size_t n = wcrtomb(mb, value[i], 0);
    size_t j;

    if (n == (size_t)-1) {
      free(narrow);
      return;
    }
    for (j = 0; j < n; ++j) {
      narrow[byte_len++] = mb[j];
    }
  }
  narrow[byte_len] = 0;
  if (spec->precision_set) {
    narrow_spec.precision_set = 0;
    narrow_spec.precision = 0;
  }
  format_string(buffer, &narrow_spec, narrow);
  free(narrow);
}

static void format_wide_char(struct printf_buffer* buffer, const struct printf_spec* spec, wchar_t wc) {
  char narrow[5];
  size_t length = wcrtomb(narrow, wc, 0);

  if (length == (size_t)-1) {
    return;
  }
  narrow[length] = 0;
  format_string(buffer, spec, narrow);
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

static uint64_t double_bits(double value) {
  union {
    double d;
    uint64_t u;
  } bits;

  bits.d = value;
  return bits.u;
}

static int double_is_negative(double value) {
  return (double_bits(value) >> 63) != 0;
}

static int double_is_nan(double value) {
  uint64_t bits = double_bits(value);
  return (bits & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL &&
      (bits & 0x000fffffffffffffULL) != 0;
}

static int double_is_inf(double value) {
  return (double_bits(value) & 0x7fffffffffffffffULL) == 0x7ff0000000000000ULL;
}

static long double double_abs_value(double value) {
  return value < 0.0 || double_is_negative(value) ? -(long double)value : (long double)value;
}

static int append_float_prefix(char* prefix, const struct printf_spec* spec, double value) {
  if (double_is_negative(value)) {
    prefix[0] = '-';
    return 1;
  }
  if (spec->plus) {
    prefix[0] = '+';
    return 1;
  }
  if (spec->space) {
    prefix[0] = ' ';
    return 1;
  }
  return 0;
}

static void format_float_special(
    struct printf_buffer* buffer,
    const struct printf_spec* spec,
    double value,
    int uppercase) {
  char prefix[1];
  int prefix_len = double_is_nan(value) ? 0 : append_float_prefix(prefix, spec, value);
  const char* text;

  if (double_is_nan(value)) {
    text = uppercase ? "NAN" : "nan";
  } else {
    text = uppercase ? "INF" : "inf";
  }
  write_formatted(buffer, spec, prefix, (size_t)prefix_len, text, strlen(text), 0);
}

static unsigned long long pow10_u64(int precision) {
  unsigned long long scale = 1;

  while (precision-- > 0) {
    scale *= 10ULL;
  }
  return scale;
}

static size_t append_unsigned_fixed_width(char* out, size_t pos, unsigned long long value, int width) {
  char digits[32];
  int i;

  for (i = width - 1; i >= 0; --i) {
    digits[i] = (char)('0' + (value % 10ULL));
    value /= 10ULL;
  }
  for (i = 0; i < width; ++i) {
    out[pos++] = digits[i];
  }
  return pos;
}

static void trim_fraction(char* digits, size_t* len) {
  while (*len != 0 && digits[*len - 1] == '0') {
    --*len;
  }
  if (*len != 0 && digits[*len - 1] == '.') {
    --*len;
  }
  digits[*len] = 0;
}

static int decimal_exponent(long double value) {
  int exponent = 0;

  if (value == 0.0L) {
    return 0;
  }
  while (value >= 10.0L) {
    value /= 10.0L;
    ++exponent;
  }
  while (value < 1.0L) {
    value *= 10.0L;
    --exponent;
  }
  return exponent;
}

static void append_exponent(char* digits, size_t* len, int exponent, int uppercase) {
  unsigned int magnitude = exponent < 0 ? (unsigned int)-exponent : (unsigned int)exponent;

  digits[(*len)++] = uppercase ? 'E' : 'e';
  digits[(*len)++] = exponent < 0 ? '-' : '+';
  if (magnitude >= 100U) {
    char temp[16];
    size_t n = convert_unsigned(temp, magnitude, 10, 0);
    size_t i;
    for (i = 0; i < n; ++i) {
      digits[(*len)++] = temp[i];
    }
  } else {
    digits[(*len)++] = (char)('0' + (magnitude / 10U));
    digits[(*len)++] = (char)('0' + (magnitude % 10U));
  }
}

static void format_double_fixed(struct printf_buffer* buffer, const struct printf_spec* spec, double value) {
  struct printf_spec out_spec = *spec;
  unsigned long long integer_part;
  unsigned long long scale;
  unsigned long long fractional;
  char digits[160];
  char integer_digits[64];
  char prefix[1];
  int precision = spec->precision_set ? spec->precision : 6;
  int negative = value < 0.0;
  size_t digits_len;
  size_t integer_len;
  size_t prefix_len = 0;

  if (precision < 0) {
    precision = 6;
  }
  if (precision > 18) {
    precision = 18;
  }
  if (double_is_nan(value) || double_is_inf(value)) {
    format_float_special(buffer, spec, value, 0);
    return;
  }

  prefix_len = (size_t)append_float_prefix(prefix, spec, value);
  value = (double)double_abs_value(value);
  scale = pow10_u64(precision);
  integer_part = (unsigned long long)value;
  fractional = (unsigned long long)(((long double)value - (long double)integer_part) * (long double)scale + 0.5L);
  if (fractional >= scale) {
    ++integer_part;
    fractional -= scale;
  }

  integer_len = convert_unsigned(integer_digits, integer_part, 10, 0);
  memcpy(digits, integer_digits, integer_len);
  digits_len = integer_len;
  out_spec.precision_set = 0;
  if (precision > 0 || spec->alt) {
    digits[digits_len++] = '.';
    digits_len = append_unsigned_fixed_width(digits, digits_len, fractional, precision);
  }
  digits[digits_len] = 0;
  (void)negative;
  write_formatted(buffer, &out_spec, prefix, prefix_len, digits, digits_len, 0);
}

static void format_double_exp(
    struct printf_buffer* buffer,
    const struct printf_spec* spec,
    double value,
    int uppercase,
    int trim) {
  struct printf_spec out_spec = *spec;
  char digits[192];
  char prefix[1];
  size_t len = 0;
  size_t prefix_len;
  long double magnitude;
  int precision = spec->precision_set ? spec->precision : 6;
  int exponent;
  unsigned long long scale;
  unsigned long long rounded;
  unsigned long long first;
  unsigned long long frac;

  if (precision < 0) {
    precision = 6;
  }
  if (precision > 18) {
    precision = 18;
  }
  if (double_is_nan(value) || double_is_inf(value)) {
    format_float_special(buffer, spec, value, uppercase);
    return;
  }
  prefix_len = (size_t)append_float_prefix(prefix, spec, value);
  magnitude = double_abs_value(value);
  exponent = decimal_exponent(magnitude);
  if (magnitude != 0.0L) {
    int i;
    if (exponent > 0) {
      for (i = 0; i < exponent; ++i) {
        magnitude /= 10.0L;
      }
    } else if (exponent < 0) {
      for (i = 0; i < -exponent; ++i) {
        magnitude *= 10.0L;
      }
    }
  }
  scale = pow10_u64(precision);
  rounded = (unsigned long long)(magnitude * (long double)scale + 0.5L);
  if (rounded >= 10ULL * scale) {
    rounded /= 10ULL;
    ++exponent;
  }
  first = precision == 0 ? rounded : rounded / scale;
  frac = precision == 0 ? 0 : rounded % scale;
  digits[len++] = (char)('0' + first);
  if (precision > 0 || spec->alt) {
    digits[len++] = '.';
    len = append_unsigned_fixed_width(digits, len, frac, precision);
    if (trim && !spec->alt) {
      trim_fraction(digits, &len);
    }
  }
  append_exponent(digits, &len, exponent, uppercase);
  digits[len] = 0;
  out_spec.precision_set = 0;
  write_formatted(buffer, &out_spec, prefix, prefix_len, digits, len, 0);
}

static void format_double_general(
    struct printf_buffer* buffer,
    const struct printf_spec* spec,
    double value,
    int uppercase) {
  struct printf_spec adjusted = *spec;
  int precision = spec->precision_set ? spec->precision : 6;
  int exponent;

  if (precision == 0) {
    precision = 1;
  }
  if (double_is_nan(value) || double_is_inf(value)) {
    format_float_special(buffer, spec, value, uppercase);
    return;
  }
  exponent = decimal_exponent(double_abs_value(value));
  if (exponent < -4 || exponent >= precision) {
    adjusted.precision_set = 1;
    adjusted.precision = precision - 1;
    format_double_exp(buffer, &adjusted, value, uppercase, 1);
  } else {
    adjusted.precision_set = 1;
    adjusted.precision = precision - (exponent + 1);
    if (adjusted.precision < 0) {
      adjusted.precision = 0;
    }
    adjusted.width = 0;
    adjusted.left = 0;
    adjusted.zero = 0;
    {
      struct printf_buffer temp;
      char local[192];
      size_t len;

      temp.data = local;
      temp.capacity = sizeof(local);
      temp.length = 0;
      format_double_fixed(&temp, &adjusted, value);
      len = temp.length < sizeof(local) ? temp.length : sizeof(local) - 1;
      local[len] = 0;
      if (!spec->alt) {
        trim_fraction(local, &len);
      }
      write_formatted(buffer, spec, 0, 0, local, len, 0);
    }
  }
}

static void format_double_hex(
    struct printf_buffer* buffer,
    const struct printf_spec* spec,
    double value,
    int uppercase) {
  struct printf_spec out_spec = *spec;
  char digits[192];
  char prefix[3];
  const char* table = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
  size_t prefix_len;
  size_t len = 0;
  long double magnitude;
  int precision = spec->precision_set ? spec->precision : 13;
  int exponent = 0;
  int i;

  if (precision < 0) {
    precision = 13;
  }
  if (precision > 32) {
    precision = 32;
  }
  if (double_is_nan(value) || double_is_inf(value)) {
    format_float_special(buffer, spec, value, uppercase);
    return;
  }
  prefix_len = (size_t)append_float_prefix(prefix, spec, value);
  prefix[prefix_len++] = '0';
  prefix[prefix_len++] = uppercase ? 'X' : 'x';
  magnitude = double_abs_value(value);
  if (magnitude != 0.0L) {
    while (magnitude >= 2.0L) {
      magnitude *= 0.5L;
      ++exponent;
    }
    while (magnitude < 1.0L) {
      magnitude *= 2.0L;
      --exponent;
    }
  }
  digits[len++] = magnitude >= 1.0L ? '1' : '0';
  if (magnitude >= 1.0L) {
    magnitude -= 1.0L;
  }
  if (precision > 0 || spec->alt) {
    digits[len++] = '.';
    for (i = 0; i < precision; ++i) {
      int nibble;
      magnitude *= 16.0L;
      nibble = (int)magnitude;
      if (nibble < 0) {
        nibble = 0;
      } else if (nibble > 15) {
        nibble = 15;
      }
      digits[len++] = table[nibble];
      magnitude -= (long double)nibble;
    }
    if (!spec->precision_set && !spec->alt) {
      trim_fraction(digits, &len);
    }
  }
  digits[len++] = uppercase ? 'P' : 'p';
  digits[len++] = exponent < 0 ? '-' : '+';
  {
    char exp_digits[16];
    size_t exp_len = convert_unsigned(exp_digits, exponent < 0 ? (unsigned int)-exponent : (unsigned int)exponent, 10, 0);
    size_t j;
    for (j = 0; j < exp_len; ++j) {
      digits[len++] = exp_digits[j];
    }
  }
  digits[len] = 0;
  out_spec.precision_set = 0;
  write_formatted(buffer, &out_spec, prefix, prefix_len, digits, len, 0);
}

int vsnprintf(char* s, size_t n, const char* format, va_list ap) {
  struct printf_buffer buffer;

  buffer.data = s;
  buffer.capacity = n;
  buffer.length = 0;

  while (*format != 0) {
    struct printf_spec spec = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

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

    if (*format == '*') {
      spec.width = va_arg(ap, int);
      if (spec.width < 0) {
        spec.left = 1;
        spec.width = -spec.width;
      }
      ++format;
    } else {
      parse_decimal(&format, &spec.width);
    }
    if (*format == '.') {
      format++;
      spec.precision_set = 1;
      if (*format == '*') {
        spec.precision = va_arg(ap, int);
        if (spec.precision < 0) {
          spec.precision_set = 0;
          spec.precision = 0;
        }
        ++format;
      } else {
        parse_decimal(&format, &spec.precision);
      }
    }

    while (*format == 'h') {
      spec.short_count++;
      format++;
    }
    while (*format == 'l') {
      spec.long_count++;
      format++;
    }
    if (*format == 'z') {
      spec.size_length = 1;
      format++;
    } else if (*format == 'j') {
      spec.intmax_length = 1;
      format++;
    } else if (*format == 't') {
      spec.ptrdiff_length = 1;
      format++;
    }

    switch (*format) {
      case 'c': {
        if (spec.long_count > 0) {
          format_wide_char(&buffer, &spec, (wchar_t)va_arg(ap, int));
        } else {
          char ch = (char)va_arg(ap, int);
          char text[1];
          text[0] = ch;
          write_formatted(&buffer, &spec, 0, 0, text, 1, 0);
        }
        break;
      }
      case 's':
        if (spec.long_count > 0) {
          format_wide_string(&buffer, &spec, va_arg(ap, const wchar_t*));
        } else {
          format_string(&buffer, &spec, va_arg(ap, const char*));
        }
        break;
      case 'd':
      case 'i': {
        long long value;
        unsigned long long magnitude;
        int negative;
        if (spec.intmax_length) {
          value = va_arg(ap, long long);
        } else if (spec.ptrdiff_length) {
          value = (long long)va_arg(ap, ptrdiff_t);
        } else if (spec.size_length) {
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
        if (spec.intmax_length) {
          value = va_arg(ap, unsigned long long);
        } else if (spec.ptrdiff_length) {
          value = (unsigned long long)va_arg(ap, ptrdiff_t);
        } else if (spec.size_length) {
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
      case 'n': {
        if (spec.short_count >= 2) {
          *va_arg(ap, signed char*) = (signed char)buffer.length;
        } else if (spec.short_count == 1) {
          *va_arg(ap, short*) = (short)buffer.length;
        } else if (spec.long_count >= 2) {
          *va_arg(ap, long long*) = (long long)buffer.length;
        } else if (spec.long_count == 1) {
          *va_arg(ap, long*) = (long)buffer.length;
        } else if (spec.size_length) {
          *va_arg(ap, ssize_t*) = (ssize_t)buffer.length;
        } else if (spec.ptrdiff_length) {
          *va_arg(ap, ptrdiff_t*) = (ptrdiff_t)buffer.length;
        } else {
          *va_arg(ap, int*) = (int)buffer.length;
        }
        break;
      }
      case 'm':
        format_string(&buffer, &spec, strerror(errno));
        break;
      case 'f':
        format_double_fixed(&buffer, &spec, va_arg(ap, double));
        break;
      case 'F':
        {
          double value = va_arg(ap, double);
          if (double_is_nan(value) || double_is_inf(value)) {
            format_float_special(&buffer, &spec, value, 1);
          } else {
            format_double_fixed(&buffer, &spec, value);
          }
        }
        break;
      case 'e':
        format_double_exp(&buffer, &spec, va_arg(ap, double), 0, 0);
        break;
      case 'E':
        format_double_exp(&buffer, &spec, va_arg(ap, double), 1, 0);
        break;
      case 'g':
        format_double_general(&buffer, &spec, va_arg(ap, double), 0);
        break;
      case 'G':
        format_double_general(&buffer, &spec, va_arg(ap, double), 1);
        break;
      case 'a':
        format_double_hex(&buffer, &spec, va_arg(ap, double), 0);
        break;
      case 'A':
        format_double_hex(&buffer, &spec, va_arg(ap, double), 1);
        break;
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

int vsprintf(char* s, const char* format, va_list ap) {
  return vsnprintf(s, (size_t)-1, format, ap);
}

int sprintf(char* s, const char* format, ...) {
  int result;
  va_list ap;

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);
  return result;
}

int vasprintf(char** strp, const char* format, va_list ap) {
  va_list copy;
  char* buffer;
  int length;
  int result;

  if (strp == 0) {
    errno = EINVAL;
    return -1;
  }
  *strp = 0;
  va_copy(copy, ap);
  length = vsnprintf(0, 0, format, copy);
  va_end(copy);
  if (length < 0) {
    return -1;
  }
  buffer = (char*)malloc((size_t)length + 1);
  if (buffer == 0) {
    errno = ENOMEM;
    return -1;
  }
  result = vsnprintf(buffer, (size_t)length + 1, format, ap);
  if (result < 0) {
    free(buffer);
    return -1;
  }
  *strp = buffer;
  return result;
}

int asprintf(char** strp, const char* format, ...) {
  int result;
  va_list ap;

  va_start(ap, format);
  result = vasprintf(strp, format, ap);
  va_end(ap);
  return result;
}

int vfprintf(FILE* stream, const char* format, va_list ap) {
  char stack_buffer[1024];
  char* buffer = stack_buffer;
  va_list copy;
  int result;

  va_copy(copy, ap);
  result = vsnprintf(stack_buffer, sizeof(stack_buffer), format, copy);
  va_end(copy);
  if (result < 0) {
    return result;
  }
  if ((size_t)result >= sizeof(stack_buffer)) {
    buffer = (char*)malloc((size_t)result + 1);
    if (buffer == 0) {
      return EOF;
    }
    result = vsnprintf(buffer, (size_t)result + 1, format, ap);
    if (result < 0) {
      free(buffer);
      return result;
    }
  }
  if (fwrite(buffer, 1, (size_t)result, stream) != (size_t)result) {
    if (buffer != stack_buffer) {
      free(buffer);
    }
    return EOF;
  }
  if (buffer != stack_buffer) {
    free(buffer);
  }
  return result;
}

int fprintf(FILE* stream, const char* format, ...) {
  int result;
  va_list ap;

  va_start(ap, format);
  result = vfprintf(stream, format, ap);
  va_end(ap);
  return result;
}

int printf(const char* format, ...) {
  int result;
  va_list ap;

  va_start(ap, format);
  result = vfprintf(stdout, format, ap);
  va_end(ap);
  return result;
}

int vprintf(const char* format, va_list ap) {
  return vfprintf(stdout, format, ap);
}
