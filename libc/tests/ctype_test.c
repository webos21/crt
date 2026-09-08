#include <ctype.h>
#include <stdio.h>

static int fail(const char* message) {
  fprintf(stderr, "ctype_test: %s\n", message);
  return 1;
}

static int expect_true(int value, const char* message) {
  if (!value) {
    return fail(message);
  }
  return 0;
}

static int expect_false(int value, const char* message) {
  if (value) {
    return fail(message);
  }
  return 0;
}

static int expect_int(int actual, int expected, const char* message) {
  if (actual != expected) {
    return fail(message);
  }
  return 0;
}

int main(void) {
  if (expect_true(isalpha('A'), "isalpha upper") ||
      expect_true(isalpha('z'), "isalpha lower") ||
      expect_false(isalpha('7'), "isalpha digit") ||
      expect_true(isalnum('7'), "isalnum digit") ||
      expect_false(isalnum('/'), "isalnum punct") ||
      expect_true(isblank(' '), "isblank space") ||
      expect_true(isblank('\t'), "isblank tab") ||
      expect_false(isblank('\n'), "isblank newline") ||
      expect_true(iscntrl('\n'), "iscntrl newline") ||
      expect_true(iscntrl(0x7f), "iscntrl del") ||
      expect_false(iscntrl('A'), "iscntrl alpha") ||
      expect_true(isdigit('0'), "isdigit zero") ||
      expect_false(isdigit('x'), "isdigit alpha") ||
      expect_true(isgraph('!'), "isgraph punct") ||
      expect_false(isgraph(' '), "isgraph space") ||
      expect_true(islower('a'), "islower lower") ||
      expect_false(islower('A'), "islower upper") ||
      expect_true(isprint(' '), "isprint space") ||
      expect_false(isprint('\n'), "isprint newline") ||
      expect_true(ispunct('/'), "ispunct slash") ||
      expect_false(ispunct('A'), "ispunct alpha") ||
      expect_true(isspace('\r'), "isspace cr") ||
      expect_false(isspace('x'), "isspace alpha") ||
      expect_true(isupper('Z'), "isupper upper") ||
      expect_false(isupper('z'), "isupper lower") ||
      expect_true(isxdigit('f'), "isxdigit lower") ||
      expect_true(isxdigit('F'), "isxdigit upper") ||
      expect_false(isxdigit('g'), "isxdigit miss") ||
      expect_true(isascii(0x7f), "isascii del") ||
      expect_false(isascii(0x80), "isascii high") ||
      expect_false(isalpha(-1), "isalpha eof") ||
      expect_int(tolower('A'), 'a', "tolower upper") ||
      expect_int(tolower('a'), 'a', "tolower lower") ||
      expect_int(toupper('z'), 'Z', "toupper lower") ||
      expect_int(toupper('Z'), 'Z', "toupper upper") ||
      expect_int(toascii(0xff), 0x7f, "toascii mask")) {
    return 1;
  }

  printf("ctype_test: ok\n");
  return 0;
}
