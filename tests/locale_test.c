#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

static int fail(const char* message) {
  fprintf(stderr, "locale_test: %s\n", message);
  return 1;
}

int main(void) {
  struct lconv* conv;

  if (setlocale(LC_ALL, 0) == 0 || strcmp(setlocale(LC_ALL, 0), "C") != 0) {
    return fail("query default");
  }
  if (setlocale(LC_ALL, "C") == 0 || strcmp(setlocale(LC_ALL, "C"), "C") != 0) {
    return fail("set C");
  }
  if (setlocale(LC_ALL, "POSIX") == 0 || strcmp(setlocale(LC_ALL, 0), "C") != 0) {
    return fail("set POSIX");
  }
  if (setlocale(LC_NUMERIC, "") == 0 || strcmp(setlocale(LC_NUMERIC, 0), "C") != 0) {
    return fail("set empty");
  }

  errno = 0;
  if (setlocale(LC_ALL, "en_US.UTF-8") != 0 || errno != EINVAL) {
    return fail("unsupported locale");
  }
  errno = 0;
  if (setlocale(999, "C") != 0 || errno != EINVAL) {
    return fail("invalid category");
  }

  conv = localeconv();
  if (conv == 0 ||
      strcmp(conv->decimal_point, ".") != 0 ||
      strcmp(conv->thousands_sep, "") != 0 ||
      strcmp(conv->grouping, "") != 0 ||
      strcmp(conv->currency_symbol, "") != 0 ||
      conv->frac_digits != CHAR_MAX ||
      conv->int_frac_digits != CHAR_MAX ||
      conv->p_sign_posn != CHAR_MAX ||
      conv->n_sign_posn != CHAR_MAX) {
    return fail("localeconv");
  }

  printf("locale_test: ok\n");
  return 0;
}
