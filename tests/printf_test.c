#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "printf_test: %s\n", message);
  return 1;
}

int main(void) {
  char buffer[128];
  int result;
  int count = 0;
  int width = 6;
  int precision = 3;

  result = snprintf(buffer, sizeof(buffer), "%s %c %d %u %x %X %%", "fmt", 'A', -42, 42u, 0x2a, 0x2a);
  if (result != 20 || strcmp(buffer, "fmt A -42 42 2a 2A %") != 0) {
    return fail("snprintf basic");
  }

  result = snprintf(buffer, 8, "abcdefghi");
  if (result != 9 || strcmp(buffer, "abcdefg") != 0) {
    return fail("snprintf truncation");
  }

  result = snprintf(buffer, sizeof(buffer), "%lld %llu", -9223372036854775807LL - 1LL,
                    18446744073709551615ULL);
  if (result != 41 ||
      strcmp(buffer, "-9223372036854775808 18446744073709551615") != 0) {
    return fail("long long");
  }

  result = snprintf(buffer, sizeof(buffer), "|%08d|%-6s|%+d|% d|%#x|%#o|%.3s|%5zu|%zd|",
                    42, "hi", 7, 7, 0x2a, 10u, "abcdef", (size_t)9, (ssize_t)-3);
  if (strcmp(buffer, "|00000042|hi    |+7| 7|0x2a|012|abc|    9|-3|") != 0) {
    return fail("flags width precision");
  }

  result = sprintf(buffer, "%s:%d", "sprintf", 17);
  if (result != 10 || strcmp(buffer, "sprintf:17") != 0) {
    return fail("sprintf");
  }

  result = snprintf(buffer, sizeof(buffer), "|%*.*d|%hhd|%td|%nend", width, precision, 7,
                    (signed char)-5, (ptrdiff_t)-9, &count);
  if (result != 17 || strcmp(buffer, "|   007|-5|-9|end") != 0 || count != 14) {
    return fail("star short ptrdiff n");
  }

  result = snprintf(buffer, sizeof(buffer), "|%.2f|%m|", 3.14159);
  if (result < 0 || strncmp(buffer, "|3.14|", 6) != 0) {
    return fail("float m");
  }
  if (snprintf(buffer, sizeof(buffer), "|%.0f|%.2f|%+.1f|", 2.5, -0.0, 3.0) != 14 ||
      strcmp(buffer, "|2|-0.00|+3.0|") != 0) {
    return fail("float fixed");
  }
  if (snprintf(buffer, sizeof(buffer), "|%.2e|%.1E|%10.2e|", 1234.0, 0.01234, -12.5) != 29 ||
      strcmp(buffer, "|1.23e+03|1.2E-02| -1.25e+01|") != 0) {
    return fail("float exponent");
  }
  if (snprintf(buffer, sizeof(buffer), "|%.4g|%.3g|%#.3g|", 1234.0, 0.0001234, 12.0) != 20 ||
      strcmp(buffer, "|1234|0.000123|12.0|") != 0) {
    return fail("float general");
  }
  if (snprintf(buffer, sizeof(buffer), "|%a|%.2a|%A|", 1.5, 1.0, 0.25) != 27 ||
      strcmp(buffer, "|0x1.8p+0|0x1.00p+0|0X1P-2|") != 0) {
    return fail("float hex");
  }
  if (snprintf(buffer, sizeof(buffer), "|%f|%F|%e|", INFINITY, -INFINITY, NAN) != 14 ||
      strcmp(buffer, "|inf|-INF|nan|") != 0) {
    return fail("float special");
  }
  if (snprintf(buffer, sizeof(buffer), "|%.2Lf|%.2Le|%.1LA|", (long double)3.125,
               (long double)1234.0, (long double)1.5) != 24 ||
      strcmp(buffer, "|3.12|1.23e+03|0X1.8P+0|") != 0) {
    return fail("long double formatting");
  }
  if (snprintf(buffer, sizeof(buffer), "%2$s:%1$d:%3$.*4$f", 7, "pos", 3.125, 2) != 10 ||
      strcmp(buffer, "pos:7:3.12") != 0) {
    return fail("positional args");
  }
  if (snprintf(buffer, sizeof(buffer), "|%3$*2$.*1$f|", 1, 8, 2.25) != 10 ||
      strcmp(buffer, "|     2.2|") != 0) {
    return fail("positional width precision");
  }
  count = 0;
  if (snprintf(buffer, sizeof(buffer), "%2$s%1$n", &count, "abc") != 3 ||
      strcmp(buffer, "abc") != 0 || count != 3) {
    return fail("positional n");
  }
  if (fesetround(FE_DOWNWARD) != 0) {
    return fail("fesetround downward");
  }
  if (snprintf(buffer, sizeof(buffer), "%.0f", 2.9) != 1 || strcmp(buffer, "2") != 0) {
    return fail("float downward rounding");
  }
  if (fesetround(FE_UPWARD) != 0) {
    return fail("fesetround upward");
  }
  if (snprintf(buffer, sizeof(buffer), "%.0f", 2.1) != 1 || strcmp(buffer, "3") != 0) {
    return fail("float upward rounding");
  }
  if (fesetround(FE_TONEAREST) != 0) {
    return fail("fesetround nearest");
  }

  printf("printf_test: %s %d %x\n", "ok", 7, 255);
  return 0;
}
