#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <string.h>

static char c_locale_name[] = "C";
static char empty_string[] = "";
static char decimal_point[] = ".";

static struct lconv c_lconv = {
    decimal_point,
    empty_string,
    empty_string,
    empty_string,
    empty_string,
    empty_string,
    empty_string,
    empty_string,
    empty_string,
    empty_string,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
};

static int valid_category(int category) {
  return category >= LC_ALL && category <= LC_MESSAGES;
}

static int supported_locale_name(const char* locale) {
  return locale == 0 || locale[0] == '\0' ||
         strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0;
}

char* setlocale(int category, const char* locale) {
  if (!valid_category(category)) {
    errno = EINVAL;
    return 0;
  }
  if (locale == 0) {
    return c_locale_name;
  }
  if (!supported_locale_name(locale)) {
    errno = EINVAL;
    return 0;
  }
  return c_locale_name;
}

struct lconv* localeconv(void) {
  return &c_lconv;
}
