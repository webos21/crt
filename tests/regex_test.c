#include <regex.h>
#include <string.h>
#include <unistd.h>

static int fail(const char* message) {
  static const char prefix[] = "regex_test: ";
  static const char suffix[] = "\n";
  write(2, prefix, sizeof(prefix) - 1);
  write(2, message, strlen(message));
  write(2, suffix, sizeof(suffix) - 1);
  return 1;
}

static int check_capture(const char* target, const char* bre_pattern, const char* expected) {
  regex_t pat;
  regmatch_t m[2];
  int rc;

  if (regcomp(&pat, bre_pattern, 0) != 0) return 0;
  rc = regexec(&pat, target, 2, m, 0);
  if (rc != 0) {
    regfree(&pat);
    return expected == 0;
  }
  if (pat.re_nsub == 0) {
    regfree(&pat);
    return 0;
  }
  regfree(&pat);
  if (expected == 0) return 0;
  if ((size_t)(m[1].rm_eo - m[1].rm_so) != strlen(expected)) return 0;
  return memcmp(target + m[1].rm_so, expected, strlen(expected)) == 0;
}

int main(void) {
  regex_t pat;
  regmatch_t m[4];

  // The autoconf `ac_option`/`ac_optarg` idiom: extract the tail of a
  // `--enable-X`/`--disable-X` option name, and the value after `=`.
  if (!check_capture("x--disable-shared", "x--[^-]*-\\(.*\\)", "shared")) {
    return fail("bre group: disable-shared suffix");
  }
  if (!check_capture("x--enable-static", "x--[^-]*-\\(.*\\)", "static")) {
    return fail("bre group: enable-static suffix");
  }
  if (!check_capture("x--prefix=/tmp/foo", "x[^=]*=\\(.*\\)", "/tmp/foo")) {
    return fail("bre group: prefix value");
  }
  if (!check_capture("hello world", "hello \\(.*\\)", "world")) {
    return fail("bre group: trailing capture");
  }
  if (!check_capture("hello world", "\\(hello\\)", "hello")) {
    return fail("bre group: leading capture");
  }

  // Nested groups: outer captures the whole match, inner captures the
  // digits alone.
  if (regcomp(&pat, "a\\(b\\(c*\\)d\\)e", 0) != 0) return fail("regcomp nested");
  if (regexec(&pat, "abccde", 4, m, 0) != 0) return fail("regexec nested: no match");
  if (pat.re_nsub != 2) return fail("regexec nested: re_nsub");
  if (m[1].rm_so != 1 || m[1].rm_eo != 5) return fail("regexec nested: outer group bounds");
  if (m[2].rm_so != 2 || m[2].rm_eo != 4) return fail("regexec nested: inner group bounds");
  regfree(&pat);

  // A pattern with no group still reports re_nsub == 0 and matches by
  // length, matching the pre-fix behavior for the common case.
  if (regcomp(&pat, "abc", 0) != 0) return fail("regcomp no-group");
  if (regexec(&pat, "xabcx", 1, m, 0) != 0) return fail("regexec no-group: no match");
  if (pat.re_nsub != 0) return fail("regexec no-group: re_nsub");
  if (m[0].rm_so != 1 || m[0].rm_eo != 4) return fail("regexec no-group: bounds");
  regfree(&pat);

  // Unescaped parentheses in BRE mode are ordinary literal characters, not
  // group delimiters.
  if (regcomp(&pat, "(abc)", 0) != 0) return fail("regcomp literal parens");
  if (regexec(&pat, "x(abc)y", 1, m, 0) != 0) return fail("regexec literal parens: no match");
  if (pat.re_nsub != 0) return fail("regexec literal parens: re_nsub");
  regfree(&pat);

  // ERE groups use unescaped parentheses.
  if (regcomp(&pat, "a(b*)c", REG_EXTENDED) != 0) return fail("regcomp ere");
  if (regexec(&pat, "xabbbcy", 2, m, 0) != 0) return fail("regexec ere: no match");
  if (pat.re_nsub != 1) return fail("regexec ere: re_nsub");
  if (m[1].rm_so != 2 || m[1].rm_eo != 5) return fail("regexec ere: group bounds");
  regfree(&pat);

  write(1, "regex_test: ok\n", 15);
  return 0;
}
