#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>

extern char* __dtoa(double d, int mode, int ndigits, int* decpt, int* sign, char** rve);
extern void __freedtoa(char* s);
extern char* __hdtoa(double d, const char* xdigs, int ndigits, int* decpt, int* sign, char** rve);
extern char* __hldtoa(long double e, const char* xdigs, int ndigits, int* decpt, int* sign, char** rve);
extern char* __ldtoa(long double* ld, int mode, int ndigits, int* decpt, int* sign, char** rve);

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
  int grouping;
  int width;
  int precision;
  int precision_set;
  int long_count;
  int short_count;
  int size_length;
  int intmax_length;
  int ptrdiff_length;
  int long_double_length;
  int value_index;
  int width_index;
  int precision_index;
};

enum printf_arg_type {
  PRINTF_ARG_NONE = 0,
  PRINTF_ARG_INT,
  PRINTF_ARG_UINT,
  PRINTF_ARG_LONG,
  PRINTF_ARG_ULONG,
  PRINTF_ARG_LLONG,
  PRINTF_ARG_ULLONG,
  PRINTF_ARG_SIZE,
  PRINTF_ARG_SSIZE,
  PRINTF_ARG_PTRDIFF,
  PRINTF_ARG_DOUBLE,
  PRINTF_ARG_LONG_DOUBLE,
  PRINTF_ARG_POINTER,
  PRINTF_ARG_STRING,
  PRINTF_ARG_WSTRING,
  PRINTF_ARG_INT_PTR,
  PRINTF_ARG_SCHAR_PTR,
  PRINTF_ARG_SHORT_PTR,
  PRINTF_ARG_LONG_PTR,
  PRINTF_ARG_LLONG_PTR,
  PRINTF_ARG_SSIZE_PTR,
  PRINTF_ARG_PTRDIFF_PTR,
  PRINTF_ARG_WINT
};

union printf_arg_value {
  int i;
  unsigned int u;
  long l;
  unsigned long ul;
  long long ll;
  unsigned long long ull;
  size_t z;
  ssize_t sz;
  ptrdiff_t td;
  double d;
  long double ld;
  void* p;
  const char* s;
  const wchar_t* ws;
  int* ip;
  signed char* scp;
  short* shp;
  long* lp;
  long long* llp;
  ssize_t* szp;
  ptrdiff_t* tdp;
  wchar_t wi;
};

#define PRINTF_MAX_POSITIONAL_ARGS 64

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

static int parse_decimal_value(const char** format) {
  int result = 0;

  while (is_digit(**format)) {
    result = result * 10 + (**format - '0');
    ++*format;
  }
  return result;
}

static const char* printf_decimal_point(void) {
  struct lconv* lc = localeconv();

  if (lc == 0 || lc->decimal_point == 0 || lc->decimal_point[0] == 0) {
    return ".";
  }
  return lc->decimal_point;
}

static int decimal_point_suffix(const char* text, size_t len) {
  const char* point = printf_decimal_point();
  size_t point_len = strlen(point);

  return point_len != 0 && len >= point_len &&
      memcmp(text + len - point_len, point, point_len) == 0;
}

static int printf_grouping_enabled(const struct printf_spec* spec) {
  struct lconv* lc;

  if (!spec->grouping) {
    return 0;
  }
  lc = localeconv();
  return lc != 0 && lc->thousands_sep != 0 && lc->thousands_sep[0] != 0 &&
      lc->grouping != 0 && lc->grouping[0] != 0 && lc->grouping[0] != CHAR_MAX;
}

static size_t grouping_span(size_t digits, const char* grouping) {
  size_t groups = 0;
  int group = (unsigned char)grouping[0];

  if (group <= 0 || group == CHAR_MAX) {
    return 0;
  }
  while (digits > (size_t)group) {
    digits -= (size_t)group;
    ++groups;
    if (grouping[1] != 0 && grouping[1] != CHAR_MAX) {
      ++grouping;
      group = (unsigned char)grouping[0];
    }
  }
  return groups;
}

static size_t apply_grouping_to_digits(char* out, size_t out_capacity, const char* digits, size_t digits_len) {
  struct lconv* lc = localeconv();
  const char* grouping;
  const char* sep;
  size_t sep_len;
  size_t groups;
  size_t out_len;
  size_t in_pos = digits_len;
  size_t out_pos;
  int group;

  if (lc == 0 || lc->thousands_sep == 0 || lc->thousands_sep[0] == 0 ||
      lc->grouping == 0 || lc->grouping[0] == 0 || lc->grouping[0] == CHAR_MAX) {
    if (digits_len < out_capacity) {
      memcpy(out, digits, digits_len);
      out[digits_len] = 0;
    }
    return digits_len;
  }
  grouping = lc->grouping;
  sep = lc->thousands_sep;
  sep_len = strlen(sep);
  groups = grouping_span(digits_len, grouping);
  out_len = digits_len + groups * sep_len;
  if (out_len + 1 > out_capacity) {
    if (digits_len < out_capacity) {
      memcpy(out, digits, digits_len);
      out[digits_len] = 0;
    }
    return digits_len;
  }
  out_pos = out_len;
  out[out_pos] = 0;
  group = (unsigned char)grouping[0];
  while (in_pos > 0) {
    int copied = 0;
    while (in_pos > 0 && copied < group) {
      out[--out_pos] = digits[--in_pos];
      ++copied;
    }
    if (in_pos == 0) {
      break;
    }
    out_pos -= sep_len;
    memcpy(out + out_pos, sep, sep_len);
    if (grouping[1] != 0 && grouping[1] != CHAR_MAX) {
      ++grouping;
      group = (unsigned char)grouping[0];
    }
  }
  return out_len;
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
  char grouped_digits[128];
  char prefix[3];
  size_t digits_len;
  size_t prefix_len = 0;

  digits_len = convert_unsigned(digits, value, base, uppercase);
  if (base == 10 && printf_grouping_enabled(spec)) {
    digits_len = apply_grouping_to_digits(grouped_digits, sizeof(grouped_digits), digits, digits_len);
    memcpy(digits, grouped_digits, digits_len + 1);
  }

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

static int long_double_is_negative(long double value) {
  return __builtin_signbit(value) != 0;
}

static int long_double_is_nan(long double value) {
  return __builtin_isnan(value) != 0;
}

static int long_double_is_inf(long double value) {
  return __builtin_isinf(value) != 0;
}

static long double long_double_abs_value(long double value) {
  return long_double_is_negative(value) ? -value : value;
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

static int append_long_double_prefix(char* prefix, const struct printf_spec* spec, long double value) {
  if (long_double_is_negative(value)) {
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

static void format_long_double_special(
    struct printf_buffer* buffer,
    const struct printf_spec* spec,
    long double value,
    int uppercase) {
  char prefix[1];
  int prefix_len = long_double_is_nan(value) ? 0 : append_long_double_prefix(prefix, spec, value);
  const char* text = long_double_is_nan(value) ? (uppercase ? "NAN" : "nan") : (uppercase ? "INF" : "inf");

  write_formatted(buffer, spec, prefix, (size_t)prefix_len, text, strlen(text), 0);
}

static void trim_fraction(char* digits, size_t* len) {
  while (*len != 0 && digits[*len - 1] == '0') {
    --*len;
  }
  if (decimal_point_suffix(digits, *len)) {
    *len -= strlen(printf_decimal_point());
  }
  digits[*len] = 0;
}

static size_t append_decimal_point(char* out, size_t pos) {
  const char* point = printf_decimal_point();

  while (*point != 0) {
    out[pos++] = *point++;
  }
  return pos;
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

static char dtoa_digit_at(const char* dtoa_digits, size_t dtoa_len, int index) {
  if (index < 0 || (size_t)index >= dtoa_len) {
    return '0';
  }
  return dtoa_digits[index];
}

static size_t append_dtoa_fixed_digits(
    char* out,
    const char* dtoa_digits,
    size_t dtoa_len,
    int decpt,
    int precision,
    int alt,
    int grouping) {
  size_t len = 0;
  char grouped[192];
  size_t grouped_len;
  int i;

  if (decpt > 0) {
    for (i = 0; i < decpt; ++i) {
      out[len++] = dtoa_digit_at(dtoa_digits, dtoa_len, i);
    }
  } else {
    out[len++] = '0';
  }
  if (alt && len == 0) {
    out[len++] = '0';
  }
  if (grouping) {
    grouped_len = apply_grouping_to_digits(grouped, sizeof(grouped), out, len);
    if (grouped_len != len || (grouped_len != 0 && memcmp(grouped, out, grouped_len) != 0)) {
      memcpy(out, grouped, grouped_len);
      len = grouped_len;
    }
  }
  if (precision > 0 || alt) {
    len = append_decimal_point(out, len);
    for (i = 0; i < precision; ++i) {
      out[len++] = dtoa_digit_at(dtoa_digits, dtoa_len, decpt + i);
    }
  }
  out[len] = 0;
  return len;
}

static void format_double_fixed(struct printf_buffer* buffer, const struct printf_spec* spec, double value) {
  struct printf_spec out_spec = *spec;
  char digits[160];
  char prefix[1];
  int precision = spec->precision_set ? spec->precision : 6;
  size_t digits_len;
  size_t prefix_len = 0;
  int decpt = 0;
  int sign = 0;
  char* rve = 0;
  char* dtoa_digits;

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
  dtoa_digits = __dtoa(value, 5, precision, &decpt, &sign, &rve);
  if (dtoa_digits == 0) {
    digits[0] = '0';
    digits[1] = 0;
    write_formatted(buffer, &out_spec, prefix, prefix_len, digits, 1, 0);
    return;
  }
  (void)sign;
  digits_len = append_dtoa_fixed_digits(
      digits, dtoa_digits, strlen(dtoa_digits), decpt, precision, spec->alt, printf_grouping_enabled(spec));
  __freedtoa(dtoa_digits);
  out_spec.precision_set = 0;
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
  int precision = spec->precision_set ? spec->precision : 6;
  int decpt = 0;
  int sign = 0;
  char* rve = 0;
  char* dtoa_digits;
  size_t dtoa_len;
  int i;

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
  dtoa_digits = __dtoa(value, 4, precision + 1, &decpt, &sign, &rve);
  if (dtoa_digits == 0) {
    digits[0] = '0';
    digits[1] = 0;
    write_formatted(buffer, &out_spec, prefix, prefix_len, digits, 1, 0);
    return;
  }
  (void)sign;
  dtoa_len = strlen(dtoa_digits);
  digits[len++] = dtoa_digit_at(dtoa_digits, dtoa_len, 0);
  if (precision > 0 || spec->alt) {
    len = append_decimal_point(digits, len);
    for (i = 0; i < precision; ++i) {
      digits[len++] = dtoa_digit_at(dtoa_digits, dtoa_len, i + 1);
    }
    if (trim && !spec->alt) {
      trim_fraction(digits, &len);
    }
  }
  append_exponent(digits, &len, decpt - 1, uppercase);
  digits[len] = 0;
  __freedtoa(dtoa_digits);
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

      struct printf_spec out_spec = *spec;

      temp.data = local;
      temp.capacity = sizeof(local);
      temp.length = 0;
      format_double_fixed(&temp, &adjusted, value);
      len = temp.length < sizeof(local) ? temp.length : sizeof(local) - 1;
      local[len] = 0;
      if (!spec->alt) {
        trim_fraction(local, &len);
      }
      // %g's precision controls how many significant digits
      // format_double_fixed() above already rendered into `local` -- it
      // must NOT also reach write_formatted() below, which (correctly,
      // for %d/%x/etc.) treats a set precision as "zero-pad the digit
      // string to at least this many characters". Passing the original
      // spec straight through left that still set, so e.g. `%.6g` of
      // 4.0 printed "000004" instead of "4" (precision=6 read as "pad
      // to 6 digits" a second time, on top of already having controlled
      // digit generation via `adjusted` above). format_double_fixed/
      // format_double_exp already clear this the same way before their
      // own write_formatted() calls; this branch just missed it.
      out_spec.precision_set = 0;
      write_formatted(buffer, &out_spec, 0, 0, local, len, 0);
    }
  }
}

static void format_long_double_fixed(struct printf_buffer* buffer, const struct printf_spec* spec, long double value) {
  struct printf_spec out_spec = *spec;
  char digits[256];
  char prefix[1];
  int precision = spec->precision_set ? spec->precision : 6;
  size_t digits_len;
  size_t prefix_len;
  int decpt = 0;
  int sign = 0;
  char* rve = 0;
  char* dtoa_digits;

  if (precision < 0) {
    precision = 6;
  }
  if (precision > 36) {
    precision = 36;
  }
  if (long_double_is_nan(value) || long_double_is_inf(value)) {
    format_long_double_special(buffer, spec, value, 0);
    return;
  }
  prefix_len = (size_t)append_long_double_prefix(prefix, spec, value);
  dtoa_digits = __ldtoa(&value, 5, precision, &decpt, &sign, &rve);
  if (dtoa_digits == 0) {
    digits[0] = '0';
    digits[1] = 0;
    write_formatted(buffer, &out_spec, prefix, prefix_len, digits, 1, 0);
    return;
  }
  (void)sign;
  digits_len = append_dtoa_fixed_digits(
      digits, dtoa_digits, strlen(dtoa_digits), decpt, precision, spec->alt, printf_grouping_enabled(spec));
  __freedtoa(dtoa_digits);
  out_spec.precision_set = 0;
  write_formatted(buffer, &out_spec, prefix, prefix_len, digits, digits_len, 0);
}

static void format_long_double_exp(
    struct printf_buffer* buffer,
    const struct printf_spec* spec,
    long double value,
    int uppercase,
    int trim) {
  struct printf_spec out_spec = *spec;
  char digits[256];
  char prefix[1];
  size_t len = 0;
  size_t prefix_len;
  int precision = spec->precision_set ? spec->precision : 6;
  int decpt = 0;
  int sign = 0;
  char* rve = 0;
  char* dtoa_digits;
  size_t dtoa_len;
  int i;

  if (precision < 0) {
    precision = 6;
  }
  if (precision > 36) {
    precision = 36;
  }
  if (long_double_is_nan(value) || long_double_is_inf(value)) {
    format_long_double_special(buffer, spec, value, uppercase);
    return;
  }
  prefix_len = (size_t)append_long_double_prefix(prefix, spec, value);
  dtoa_digits = __ldtoa(&value, 4, precision + 1, &decpt, &sign, &rve);
  if (dtoa_digits == 0) {
    digits[0] = '0';
    digits[1] = 0;
    write_formatted(buffer, &out_spec, prefix, prefix_len, digits, 1, 0);
    return;
  }
  (void)sign;
  dtoa_len = strlen(dtoa_digits);
  digits[len++] = dtoa_digit_at(dtoa_digits, dtoa_len, 0);
  if (precision > 0 || spec->alt) {
    len = append_decimal_point(digits, len);
    for (i = 0; i < precision; ++i) {
      digits[len++] = dtoa_digit_at(dtoa_digits, dtoa_len, i + 1);
    }
    if (trim && !spec->alt) {
      trim_fraction(digits, &len);
    }
  }
  append_exponent(digits, &len, decpt - 1, uppercase);
  digits[len] = 0;
  __freedtoa(dtoa_digits);
  out_spec.precision_set = 0;
  write_formatted(buffer, &out_spec, prefix, prefix_len, digits, len, 0);
}

static void format_long_double_general(
    struct printf_buffer* buffer,
    const struct printf_spec* spec,
    long double value,
    int uppercase) {
  struct printf_spec adjusted = *spec;
  int precision = spec->precision_set ? spec->precision : 6;
  int exponent;

  if (precision == 0) {
    precision = 1;
  }
  if (long_double_is_nan(value) || long_double_is_inf(value)) {
    format_long_double_special(buffer, spec, value, uppercase);
    return;
  }
  exponent = decimal_exponent(long_double_abs_value(value));
  if (exponent < -4 || exponent >= precision) {
    adjusted.precision_set = 1;
    adjusted.precision = precision - 1;
    format_long_double_exp(buffer, &adjusted, value, uppercase, 1);
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
      char local[256];
      size_t len;

      struct printf_spec out_spec = *spec;

      temp.data = local;
      temp.capacity = sizeof(local);
      temp.length = 0;
      format_long_double_fixed(&temp, &adjusted, value);
      len = temp.length < sizeof(local) ? temp.length : sizeof(local) - 1;
      local[len] = 0;
      if (!spec->alt) {
        trim_fraction(local, &len);
      }
      // See the matching comment in format_double_general(); same bug,
      // same fix.
      out_spec.precision_set = 0;
      write_formatted(buffer, &out_spec, 0, 0, local, len, 0);
    }
  }
}

static void format_hex_digits(
    struct printf_buffer* buffer,
    const struct printf_spec* spec,
    const char* hdigits,
    int decpt,
    int negative,
    int uppercase) {
  struct printf_spec out_spec = *spec;
  char digits[256];
  char prefix[3];
  size_t prefix_len = 0;
  size_t len = 0;
  size_t hdigits_len = strlen(hdigits);
  int precision = spec->precision_set ? spec->precision : ((int)hdigits_len - 1);
  int i;

  if (precision < 0) {
    precision = 0;
  }
  if (precision > 64) {
    precision = 64;
  }
  if (negative) {
    prefix[prefix_len++] = '-';
  } else if (spec->plus) {
    prefix[prefix_len++] = '+';
  } else if (spec->space) {
    prefix[prefix_len++] = ' ';
  }
  prefix[prefix_len++] = '0';
  prefix[prefix_len++] = uppercase ? 'X' : 'x';
  digits[len++] = hdigits_len != 0 ? hdigits[0] : '0';
  if (precision > 0 || spec->alt) {
    len = append_decimal_point(digits, len);
    for (i = 0; i < precision; ++i) {
      digits[len++] = dtoa_digit_at(hdigits, hdigits_len, i + 1);
    }
    if (!spec->precision_set && !spec->alt) {
      trim_fraction(digits, &len);
    }
  }
  digits[len++] = uppercase ? 'P' : 'p';
  digits[len++] = decpt < 0 ? '-' : '+';
  {
    char exp_digits[16];
    size_t exp_len = convert_unsigned(exp_digits, decpt < 0 ? (unsigned int)-decpt : (unsigned int)decpt, 10, 0);
    size_t j;
    for (j = 0; j < exp_len; ++j) {
      digits[len++] = exp_digits[j];
    }
  }
  digits[len] = 0;
  out_spec.precision_set = 0;
  write_formatted(buffer, &out_spec, prefix, prefix_len, digits, len, 0);
}

static void format_double_hex(
    struct printf_buffer* buffer,
    const struct printf_spec* spec,
    double value,
    int uppercase) {
  const char* table = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
  int precision = spec->precision_set ? spec->precision + 1 : -1;
  int decpt = 0;
  int sign = 0;
  char* rve = 0;
  char* hdigits;

  if (double_is_nan(value) || double_is_inf(value)) {
    format_float_special(buffer, spec, value, uppercase);
    return;
  }
  hdigits = __hdtoa(value, table, precision, &decpt, &sign, &rve);
  if (hdigits == 0) {
    char zero[] = "0";
    format_hex_digits(buffer, spec, zero, 0, double_is_negative(value), uppercase);
    return;
  }
  if (value == 0.0) {
    decpt = 0;
  } else {
    --decpt;
  }
  format_hex_digits(buffer, spec, hdigits, decpt, sign, uppercase);
  __freedtoa(hdigits);
}

static void format_long_double_hex(
    struct printf_buffer* buffer,
    const struct printf_spec* spec,
    long double value,
    int uppercase) {
  const char* table = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
  int precision = spec->precision_set ? spec->precision + 1 : -1;
  int decpt = 0;
  int sign = 0;
  char* rve = 0;
  char* hdigits;

  if (long_double_is_nan(value) || long_double_is_inf(value)) {
    format_long_double_special(buffer, spec, value, uppercase);
    return;
  }
  hdigits = __hldtoa(value, table, precision, &decpt, &sign, &rve);
  if (hdigits == 0) {
    char zero[] = "0";
    format_hex_digits(buffer, spec, zero, 0, long_double_is_negative(value), uppercase);
    return;
  }
  if (value == 0.0L) {
    decpt = 0;
  } else {
    --decpt;
  }
  format_hex_digits(buffer, spec, hdigits, decpt, sign, uppercase);
  __freedtoa(hdigits);
}

static int printf_has_positional_args(const char* format) {
  const char* p = format;

  while (*p != 0) {
    if (*p++ != '%') {
      continue;
    }
    if (*p == '%') {
      ++p;
      continue;
    }
    if (is_digit(*p)) {
      const char* q = p;
      (void)parse_decimal_value(&q);
      if (*q == '$') {
        return 1;
      }
    }
    while (*p == '-' || *p == '0' || *p == '+' || *p == ' ' || *p == '#' || *p == '\'') {
      ++p;
    }
    if (*p == '*') {
      ++p;
      if (is_digit(*p)) {
        const char* q = p;
        (void)parse_decimal_value(&q);
        if (*q == '$') {
          return 1;
        }
      }
    } else {
      while (is_digit(*p)) {
        ++p;
      }
    }
    if (*p == '.') {
      ++p;
      if (*p == '*') {
        ++p;
        if (is_digit(*p)) {
          const char* q = p;
          (void)parse_decimal_value(&q);
          if (*q == '$') {
            return 1;
          }
        }
      } else {
        while (is_digit(*p)) {
          ++p;
        }
      }
    }
  }
  return 0;
}

static enum printf_arg_type printf_integer_arg_type(const struct printf_spec* spec, int is_signed) {
  if (spec->intmax_length || spec->long_count >= 2) {
    return is_signed ? PRINTF_ARG_LLONG : PRINTF_ARG_ULLONG;
  }
  if (spec->ptrdiff_length) {
    return PRINTF_ARG_PTRDIFF;
  }
  if (spec->size_length) {
    return is_signed ? PRINTF_ARG_SSIZE : PRINTF_ARG_SIZE;
  }
  if (spec->long_count == 1) {
    return is_signed ? PRINTF_ARG_LONG : PRINTF_ARG_ULONG;
  }
  return is_signed ? PRINTF_ARG_INT : PRINTF_ARG_UINT;
}

static enum printf_arg_type printf_n_arg_type(const struct printf_spec* spec) {
  if (spec->short_count >= 2) {
    return PRINTF_ARG_SCHAR_PTR;
  }
  if (spec->short_count == 1) {
    return PRINTF_ARG_SHORT_PTR;
  }
  if (spec->long_count >= 2) {
    return PRINTF_ARG_LLONG_PTR;
  }
  if (spec->long_count == 1) {
    return PRINTF_ARG_LONG_PTR;
  }
  if (spec->size_length) {
    return PRINTF_ARG_SSIZE_PTR;
  }
  if (spec->ptrdiff_length) {
    return PRINTF_ARG_PTRDIFF_PTR;
  }
  return PRINTF_ARG_INT_PTR;
}

static void printf_note_arg(enum printf_arg_type* types, int index, enum printf_arg_type type) {
  if (index <= 0 || index > PRINTF_MAX_POSITIONAL_ARGS || type == PRINTF_ARG_NONE) {
    return;
  }
  if (types[index] == PRINTF_ARG_NONE) {
    types[index] = type;
  }
}

static enum printf_arg_type printf_conversion_arg_type(char conversion, const struct printf_spec* spec) {
  switch (conversion) {
    case 'c':
      return spec->long_count > 0 ? PRINTF_ARG_WINT : PRINTF_ARG_INT;
    case 's':
      return spec->long_count > 0 ? PRINTF_ARG_WSTRING : PRINTF_ARG_STRING;
    case 'd':
    case 'i':
      return printf_integer_arg_type(spec, 1);
    case 'u':
    case 'o':
    case 'x':
    case 'X':
      return printf_integer_arg_type(spec, 0);
    case 'p':
      return PRINTF_ARG_POINTER;
    case 'n':
      return printf_n_arg_type(spec);
    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
    case 'a':
    case 'A':
      return spec->long_double_length ? PRINTF_ARG_LONG_DOUBLE : PRINTF_ARG_DOUBLE;
    default:
      return PRINTF_ARG_NONE;
  }
}

static const char* printf_parse_spec(const char* format, struct printf_spec* spec, int* next_arg) {
  const char* p = format;

  *spec = (struct printf_spec){0};
  if (is_digit(*p)) {
    const char* q = p;
    int index = parse_decimal_value(&q);
    if (*q == '$') {
      spec->value_index = index;
      p = q + 1;
    }
  }
  for (;;) {
    if (*p == '-') {
      spec->left = 1;
    } else if (*p == '0') {
      spec->zero = 1;
    } else if (*p == '+') {
      spec->plus = 1;
    } else if (*p == ' ') {
      spec->space = 1;
    } else if (*p == '#') {
      spec->alt = 1;
    } else if (*p == '\'') {
      spec->grouping = 1;
    } else {
      break;
    }
    ++p;
  }
  if (*p == '*') {
    ++p;
    if (is_digit(*p)) {
      const char* q = p;
      int index = parse_decimal_value(&q);
      if (*q == '$') {
        spec->width_index = index;
        p = q + 1;
      }
    }
    if (spec->width_index == 0) {
      spec->width_index = (*next_arg)++;
    }
  } else {
    parse_decimal(&p, &spec->width);
  }
  if (*p == '.') {
    ++p;
    spec->precision_set = 1;
    if (*p == '*') {
      ++p;
      if (is_digit(*p)) {
        const char* q = p;
        int index = parse_decimal_value(&q);
        if (*q == '$') {
          spec->precision_index = index;
          p = q + 1;
        }
      }
      if (spec->precision_index == 0) {
        spec->precision_index = (*next_arg)++;
      }
    } else {
      parse_decimal(&p, &spec->precision);
    }
  }
  while (*p == 'h') {
    spec->short_count++;
    ++p;
  }
  while (*p == 'l') {
    spec->long_count++;
    ++p;
  }
  if (*p == 'L') {
    spec->long_double_length = 1;
    ++p;
  }
  if (*p == 'z') {
    spec->size_length = 1;
    ++p;
  } else if (*p == 'j') {
    spec->intmax_length = 1;
    ++p;
  } else if (*p == 't') {
    spec->ptrdiff_length = 1;
    ++p;
  }
  if (spec->value_index == 0 && printf_conversion_arg_type(*p, spec) != PRINTF_ARG_NONE) {
    spec->value_index = (*next_arg)++;
  }
  return p;
}

static int printf_collect_positional_args(const char* format, enum printf_arg_type* types) {
  const char* p = format;
  int max_index = 0;
  int next_arg = 1;
  int i;

  for (i = 0; i <= PRINTF_MAX_POSITIONAL_ARGS; ++i) {
    types[i] = PRINTF_ARG_NONE;
  }
  while (*p != 0) {
    struct printf_spec spec;
    char conversion;

    if (*p++ != '%') {
      continue;
    }
    if (*p == '%') {
      ++p;
      continue;
    }
    p = printf_parse_spec(p, &spec, &next_arg);
    conversion = *p;
    printf_note_arg(types, spec.width_index, PRINTF_ARG_INT);
    printf_note_arg(types, spec.precision_index, PRINTF_ARG_INT);
    printf_note_arg(types, spec.value_index, printf_conversion_arg_type(conversion, &spec));
    if (spec.width_index > max_index) max_index = spec.width_index;
    if (spec.precision_index > max_index) max_index = spec.precision_index;
    if (spec.value_index > max_index) max_index = spec.value_index;
    if (*p != 0) {
      ++p;
    }
  }
  return max_index <= PRINTF_MAX_POSITIONAL_ARGS ? max_index : -1;
}

static void printf_read_positional_args(
    va_list ap,
    const enum printf_arg_type* types,
    union printf_arg_value* values,
    int max_index) {
  int i;

  for (i = 1; i <= max_index; ++i) {
    switch (types[i]) {
      case PRINTF_ARG_INT:
        values[i].i = va_arg(ap, int);
        break;
      case PRINTF_ARG_UINT:
        values[i].u = va_arg(ap, unsigned int);
        break;
      case PRINTF_ARG_LONG:
        values[i].l = va_arg(ap, long);
        break;
      case PRINTF_ARG_ULONG:
        values[i].ul = va_arg(ap, unsigned long);
        break;
      case PRINTF_ARG_LLONG:
        values[i].ll = va_arg(ap, long long);
        break;
      case PRINTF_ARG_ULLONG:
        values[i].ull = va_arg(ap, unsigned long long);
        break;
      case PRINTF_ARG_SIZE:
        values[i].z = va_arg(ap, size_t);
        break;
      case PRINTF_ARG_SSIZE:
        values[i].sz = va_arg(ap, ssize_t);
        break;
      case PRINTF_ARG_PTRDIFF:
        values[i].td = va_arg(ap, ptrdiff_t);
        break;
      case PRINTF_ARG_DOUBLE:
        values[i].d = va_arg(ap, double);
        break;
      case PRINTF_ARG_LONG_DOUBLE:
        values[i].ld = va_arg(ap, long double);
        break;
      case PRINTF_ARG_POINTER:
        values[i].p = va_arg(ap, void*);
        break;
      case PRINTF_ARG_STRING:
        values[i].s = va_arg(ap, const char*);
        break;
      case PRINTF_ARG_WSTRING:
        values[i].ws = va_arg(ap, const wchar_t*);
        break;
      case PRINTF_ARG_INT_PTR:
        values[i].ip = va_arg(ap, int*);
        break;
      case PRINTF_ARG_SCHAR_PTR:
        values[i].scp = va_arg(ap, signed char*);
        break;
      case PRINTF_ARG_SHORT_PTR:
        values[i].shp = va_arg(ap, short*);
        break;
      case PRINTF_ARG_LONG_PTR:
        values[i].lp = va_arg(ap, long*);
        break;
      case PRINTF_ARG_LLONG_PTR:
        values[i].llp = va_arg(ap, long long*);
        break;
      case PRINTF_ARG_SSIZE_PTR:
        values[i].szp = va_arg(ap, ssize_t*);
        break;
      case PRINTF_ARG_PTRDIFF_PTR:
        values[i].tdp = va_arg(ap, ptrdiff_t*);
        break;
      case PRINTF_ARG_WINT:
        values[i].wi = (wchar_t)va_arg(ap, int);
        break;
      case PRINTF_ARG_NONE:
      default:
        break;
    }
  }
}

static int vsnprintf_positional(char* s, size_t n, const char* format, va_list ap) {
  enum printf_arg_type types[PRINTF_MAX_POSITIONAL_ARGS + 1];
  union printf_arg_value values[PRINTF_MAX_POSITIONAL_ARGS + 1];
  struct printf_buffer buffer;
  va_list copy;
  int max_index;
  int next_arg = 1;

  max_index = printf_collect_positional_args(format, types);
  if (max_index < 0) {
    errno = EINVAL;
    if (n != 0) {
      s[0] = 0;
    }
    return -1;
  }
  memset(values, 0, sizeof(values));
  va_copy(copy, ap);
  printf_read_positional_args(copy, types, values, max_index);
  va_end(copy);

  buffer.data = s;
  buffer.capacity = n;
  buffer.length = 0;

  while (*format != 0) {
    struct printf_spec spec;
    char conversion;

    if (*format != '%') {
      buffer_putc(&buffer, *format++);
      continue;
    }
    ++format;
    if (*format == '%') {
      buffer_putc(&buffer, *format++);
      continue;
    }

    format = printf_parse_spec(format, &spec, &next_arg);
    conversion = *format;
    if (spec.width_index > 0) {
      spec.width = values[spec.width_index].i;
      if (spec.width < 0) {
        spec.left = 1;
        spec.width = -spec.width;
      }
    }
    if (spec.precision_index > 0) {
      spec.precision = values[spec.precision_index].i;
      if (spec.precision < 0) {
        spec.precision_set = 0;
        spec.precision = 0;
      }
    }

    switch (conversion) {
      case 'c':
        if (spec.long_count > 0) {
          format_wide_char(&buffer, &spec, values[spec.value_index].wi);
        } else {
          char ch = (char)values[spec.value_index].i;
          write_formatted(&buffer, &spec, 0, 0, &ch, 1, 0);
        }
        break;
      case 's':
        if (spec.long_count > 0) {
          format_wide_string(&buffer, &spec, values[spec.value_index].ws);
        } else {
          format_string(&buffer, &spec, values[spec.value_index].s);
        }
        break;
      case 'd':
      case 'i': {
        long long value;
        unsigned long long magnitude;
        int negative;
        switch (types[spec.value_index]) {
          case PRINTF_ARG_LONG: value = values[spec.value_index].l; break;
          case PRINTF_ARG_LLONG: value = values[spec.value_index].ll; break;
          case PRINTF_ARG_SSIZE: value = values[spec.value_index].sz; break;
          case PRINTF_ARG_PTRDIFF: value = values[spec.value_index].td; break;
          default: value = values[spec.value_index].i; break;
        }
        negative = value < 0;
        magnitude = negative ? (unsigned long long)(-(value + 1)) + 1ULL : (unsigned long long)value;
        format_integer(&buffer, &spec, magnitude, 1, negative, 10, 0);
        break;
      }
      case 'u':
      case 'o':
      case 'x':
      case 'X': {
        unsigned int base = conversion == 'o' ? 8u : (conversion == 'u' ? 10u : 16u);
        unsigned long long value;
        switch (types[spec.value_index]) {
          case PRINTF_ARG_ULONG: value = values[spec.value_index].ul; break;
          case PRINTF_ARG_ULLONG: value = values[spec.value_index].ull; break;
          case PRINTF_ARG_SIZE: value = values[spec.value_index].z; break;
          case PRINTF_ARG_PTRDIFF: value = (unsigned long long)values[spec.value_index].td; break;
          default: value = values[spec.value_index].u; break;
        }
        format_integer(&buffer, &spec, value, 0, 0, base, conversion == 'X');
        break;
      }
      case 'p': {
        struct printf_spec pointer_spec = spec;
        pointer_spec.alt = 1;
        format_integer(&buffer, &pointer_spec, (uintptr_t)values[spec.value_index].p, 0, 0, 16, 0);
        break;
      }
      case 'n':
        switch (types[spec.value_index]) {
          case PRINTF_ARG_SCHAR_PTR: *values[spec.value_index].scp = (signed char)buffer.length; break;
          case PRINTF_ARG_SHORT_PTR: *values[spec.value_index].shp = (short)buffer.length; break;
          case PRINTF_ARG_LONG_PTR: *values[spec.value_index].lp = (long)buffer.length; break;
          case PRINTF_ARG_LLONG_PTR: *values[spec.value_index].llp = (long long)buffer.length; break;
          case PRINTF_ARG_SSIZE_PTR: *values[spec.value_index].szp = (ssize_t)buffer.length; break;
          case PRINTF_ARG_PTRDIFF_PTR: *values[spec.value_index].tdp = (ptrdiff_t)buffer.length; break;
          default: *values[spec.value_index].ip = (int)buffer.length; break;
        }
        break;
      case 'm':
        format_string(&buffer, &spec, strerror(errno));
        break;
      case 'f':
        if (spec.long_double_length) {
          format_long_double_fixed(&buffer, &spec, values[spec.value_index].ld);
        } else {
          format_double_fixed(&buffer, &spec, values[spec.value_index].d);
        }
        break;
      case 'F':
        if (spec.long_double_length) {
          if (long_double_is_nan(values[spec.value_index].ld) || long_double_is_inf(values[spec.value_index].ld)) {
            format_long_double_special(&buffer, &spec, values[spec.value_index].ld, 1);
          } else {
            format_long_double_fixed(&buffer, &spec, values[spec.value_index].ld);
          }
        } else if (double_is_nan(values[spec.value_index].d) || double_is_inf(values[spec.value_index].d)) {
          format_float_special(&buffer, &spec, values[spec.value_index].d, 1);
        } else {
          format_double_fixed(&buffer, &spec, values[spec.value_index].d);
        }
        break;
      case 'e':
        if (spec.long_double_length) {
          format_long_double_exp(&buffer, &spec, values[spec.value_index].ld, 0, 0);
        } else {
          format_double_exp(&buffer, &spec, values[spec.value_index].d, 0, 0);
        }
        break;
      case 'E':
        if (spec.long_double_length) {
          format_long_double_exp(&buffer, &spec, values[spec.value_index].ld, 1, 0);
        } else {
          format_double_exp(&buffer, &spec, values[spec.value_index].d, 1, 0);
        }
        break;
      case 'g':
        if (spec.long_double_length) {
          format_long_double_general(&buffer, &spec, values[spec.value_index].ld, 0);
        } else {
          format_double_general(&buffer, &spec, values[spec.value_index].d, 0);
        }
        break;
      case 'G':
        if (spec.long_double_length) {
          format_long_double_general(&buffer, &spec, values[spec.value_index].ld, 1);
        } else {
          format_double_general(&buffer, &spec, values[spec.value_index].d, 1);
        }
        break;
      case 'a':
        if (spec.long_double_length) {
          format_long_double_hex(&buffer, &spec, values[spec.value_index].ld, 0);
        } else {
          format_double_hex(&buffer, &spec, values[spec.value_index].d, 0);
        }
        break;
      case 'A':
        if (spec.long_double_length) {
          format_long_double_hex(&buffer, &spec, values[spec.value_index].ld, 1);
        } else {
          format_double_hex(&buffer, &spec, values[spec.value_index].d, 1);
        }
        break;
      default:
        buffer_putc(&buffer, '%');
        if (conversion != 0) {
          buffer_putc(&buffer, conversion);
        }
        break;
    }
    if (*format != 0) {
      ++format;
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

int vsnprintf(char* s, size_t n, const char* format, va_list ap) {
  struct printf_buffer buffer;

  if (printf_has_positional_args(format)) {
    return vsnprintf_positional(s, n, format, ap);
  }

  buffer.data = s;
  buffer.capacity = n;
  buffer.length = 0;

  while (*format != 0) {
    struct printf_spec spec = {0};

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
      } else if (*format == '\'') {
        spec.grouping = 1;
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
    if (*format == 'L') {
      spec.long_double_length = 1;
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
        if (spec.long_double_length) {
          format_long_double_fixed(&buffer, &spec, va_arg(ap, long double));
        } else {
          format_double_fixed(&buffer, &spec, va_arg(ap, double));
        }
        break;
      case 'F':
        {
          if (spec.long_double_length) {
            long double value = va_arg(ap, long double);
            if (long_double_is_nan(value) || long_double_is_inf(value)) {
              format_long_double_special(&buffer, &spec, value, 1);
            } else {
              format_long_double_fixed(&buffer, &spec, value);
            }
          } else {
            double value = va_arg(ap, double);
            if (double_is_nan(value) || double_is_inf(value)) {
              format_float_special(&buffer, &spec, value, 1);
            } else {
              format_double_fixed(&buffer, &spec, value);
            }
          }
        }
        break;
      case 'e':
        if (spec.long_double_length) {
          format_long_double_exp(&buffer, &spec, va_arg(ap, long double), 0, 0);
        } else {
          format_double_exp(&buffer, &spec, va_arg(ap, double), 0, 0);
        }
        break;
      case 'E':
        if (spec.long_double_length) {
          format_long_double_exp(&buffer, &spec, va_arg(ap, long double), 1, 0);
        } else {
          format_double_exp(&buffer, &spec, va_arg(ap, double), 1, 0);
        }
        break;
      case 'g':
        if (spec.long_double_length) {
          format_long_double_general(&buffer, &spec, va_arg(ap, long double), 0);
        } else {
          format_double_general(&buffer, &spec, va_arg(ap, double), 0);
        }
        break;
      case 'G':
        if (spec.long_double_length) {
          format_long_double_general(&buffer, &spec, va_arg(ap, long double), 1);
        } else {
          format_double_general(&buffer, &spec, va_arg(ap, double), 1);
        }
        break;
      case 'a':
        if (spec.long_double_length) {
          format_long_double_hex(&buffer, &spec, va_arg(ap, long double), 0);
        } else {
          format_double_hex(&buffer, &spec, va_arg(ap, double), 0);
        }
        break;
      case 'A':
        if (spec.long_double_length) {
          format_long_double_hex(&buffer, &spec, va_arg(ap, long double), 1);
        } else {
          format_double_hex(&buffer, &spec, va_arg(ap, double), 1);
        }
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

int vdprintf(int fd, const char* format, va_list ap) {
  char* buffer;
  int result;
  ssize_t written;

  result = vasprintf(&buffer, format, ap);
  if (result < 0) {
    return -1;
  }
  written = write(fd, buffer, (size_t)result);
  free(buffer);
  if (written < 0) {
    return -1;
  }
  if (written != result) {
    errno = EIO;
    return -1;
  }
  return result;
}

int dprintf(int fd, const char* format, ...) {
  int result;
  va_list ap;

  va_start(ap, format);
  result = vdprintf(fd, format, ap);
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
