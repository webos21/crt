/* fnmatch() had no dedicated regression test before this file -- and a
 * real bug shipped silently as a result: match_here()'s end-of-pattern
 * base case returned the inverted value (match reported as FNM_NOMATCH
 * and vice versa), which broke any pattern whose final wildcard needed
 * that base case to signal success (e.g. "*.txt" against "alpha.txt").
 * Found while implementing glob() (glob.c), which is the first real
 * consumer of fnmatch() with test coverage. See HISTORY.md. */
#include <fnmatch.h>
#include <stdio.h>

static int fail(const char* message) {
  fprintf(stderr, "fnmatch_test: %s\n", message);
  return 1;
}

int main(void) {
  /* The exact case that exposed the bug: a leading '*' whose suffix
   * match only succeeds via match_here()'s *p==0 base case. */
  if (fnmatch("*.txt", "alpha.txt", 0) != 0) {
    return fail("leading star suffix match");
  }
  if (fnmatch("*.txt", "gamma.log", 0) == 0) {
    return fail("leading star suffix non-match");
  }

  if (fnmatch("a*c", "abc", 0) != 0 || fnmatch("a*c", "ac", 0) != 0 ||
      fnmatch("a*c", "abbbbc", 0) != 0 || fnmatch("a*c", "abd", 0) == 0) {
    return fail("middle star");
  }
  if (fnmatch("*", "anything", 0) != 0 || fnmatch("*", "", 0) != 0) {
    return fail("bare star");
  }
  if (fnmatch("**", "anything", 0) != 0) {
    return fail("collapsed stars");
  }
  if (fnmatch("abc", "abc", 0) != 0 || fnmatch("abc", "abd", 0) == 0 ||
      fnmatch("abc", "ab", 0) == 0 || fnmatch("ab", "abc", 0) == 0) {
    return fail("literal");
  }
  if (fnmatch("a?c", "abc", 0) != 0 || fnmatch("a?c", "ac", 0) == 0) {
    return fail("question mark");
  }
  if (fnmatch("[abc]x", "bx", 0) != 0 || fnmatch("[abc]x", "dx", 0) == 0 ||
      fnmatch("[^abc]x", "dx", 0) != 0 || fnmatch("[^abc]x", "ax", 0) == 0 ||
      fnmatch("[a-c]x", "bx", 0) != 0 || fnmatch("[a-c]x", "dx", 0) == 0) {
    return fail("bracket expression");
  }
  if (fnmatch("ABC", "abc", FNM_CASEFOLD) != 0 || fnmatch("ABC", "abc", 0) == 0) {
    return fail("FNM_CASEFOLD");
  }
  if (fnmatch("*", "a/b", FNM_PATHNAME) == 0 || fnmatch("*/*", "a/b", FNM_PATHNAME) != 0 ||
      fnmatch("a/*", "a/b", FNM_PATHNAME) != 0) {
    return fail("FNM_PATHNAME");
  }
  if (fnmatch(".*", ".hidden", FNM_PERIOD) != 0 || fnmatch("*", ".hidden", FNM_PERIOD) == 0) {
    return fail("FNM_PERIOD");
  }
  if (fnmatch("a\\*b", "a*b", 0) != 0 || fnmatch("a\\*b", "axb", 0) == 0) {
    return fail("escape default");
  }
  if (fnmatch("a\\*b", "a\\xb", FNM_NOESCAPE) != 0) {
    return fail("FNM_NOESCAPE");
  }
  if (fnmatch(0, "x", 0) == 0 || fnmatch("x", 0, 0) == 0) {
    return fail("NULL arguments");
  }

  printf("fnmatch_test: ok\n");
  return 0;
}
