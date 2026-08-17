#include <ctype.h>
#include <fnmatch.h>

static int fold_char(int ch, int flags) {
  return (flags & FNM_CASEFOLD) ? tolower((unsigned char)ch) : ch;
}

static int match_bracket(const char** pattern, int ch, int flags) {
  const char* p = *pattern;
  int negate = 0;
  int matched = 0;
  int first = 1;

  if (*p == '!' || *p == '^') {
    negate = 1;
    ++p;
  }

  ch = fold_char(ch, flags);
  while (*p != 0 && (*p != ']' || first)) {
    int start;
    int end;

    first = 0;
    if (*p == '\\' && !(flags & FNM_NOESCAPE) && p[1] != 0) ++p;
    start = fold_char((unsigned char)*p, flags);
    end = start;
    if (p[0] != 0 && p[1] == '-' && p[2] != 0 && p[2] != ']') {
      p += 2;
      if (*p == '\\' && !(flags & FNM_NOESCAPE) && p[1] != 0) ++p;
      end = fold_char((unsigned char)*p, flags);
    }
    if (start <= ch && ch <= end) matched = 1;
    if (*p != 0) ++p;
  }

  if (*p != ']') return -1;
  *pattern = p + 1;
  return negate ? !matched : matched;
}

static int match_here(const char* pattern, const char* string, int flags) {
  const char* p = pattern;
  const char* s = string;

  for (;;) {
    /* End of pattern: a match iff the string is also exhausted (or, under
     * FNM_LEADING_DIR, iff whatever remains of the string starts with a
     * '/' -- the pattern matched a leading directory component). This
     * previously returned the inverted value (`*s == 0 || ...` is 1 --
     * FNM_NOMATCH -- exactly when there IS a match, and 0 -- match --
     * exactly when there ISN'T), silently breaking every fnmatch() call
     * whose pattern's last wildcard needed this base case to report
     * success (e.g. "*.txt" against "alpha.txt": the recursive attempt
     * that lines up correctly hit this line and was misreported as a
     * failure, so the star loop kept scanning past it and the whole
     * match eventually failed for real). No caller had a dedicated
     * regression test until tests/fnmatch_test.c below caught it. */
    if (*p == 0) {
      if (*s == 0 || ((flags & FNM_LEADING_DIR) && *s == '/')) {
        return 0;
      }
      return FNM_NOMATCH;
    }

    if (*p == '*') {
      while (*p == '*') ++p;
      if ((flags & FNM_PERIOD) && *s == '.' && (s == string || ((flags & FNM_PATHNAME) && s[-1] == '/'))) {
        return FNM_NOMATCH;
      }
      if (*p == 0) {
        return ((flags & FNM_PATHNAME) && __builtin_strchr(s, '/') != 0) ? FNM_NOMATCH : 0;
      }
      while (*s != 0) {
        if ((flags & FNM_PATHNAME) && *s == '/') break;
        if (match_here(p, s, flags) == 0) return 0;
        ++s;
      }
      return match_here(p, s, flags);
    }

    if (*s == 0) return FNM_NOMATCH;
    if ((flags & FNM_PATHNAME) && *s == '/' && *p != '/') return FNM_NOMATCH;
    if ((flags & FNM_PERIOD) && *s == '.' && (s == string || ((flags & FNM_PATHNAME) && s[-1] == '/')) &&
        *p != '.') {
      return FNM_NOMATCH;
    }

    if (*p == '?') {
      ++p;
      ++s;
      continue;
    }

    if (*p == '[') {
      int bracket;
      ++p;
      bracket = match_bracket(&p, (unsigned char)*s, flags);
      if (bracket <= 0) return FNM_NOMATCH;
      ++s;
      continue;
    }

    if (*p == '\\' && !(flags & FNM_NOESCAPE) && p[1] != 0) ++p;
    if (fold_char((unsigned char)*p, flags) != fold_char((unsigned char)*s, flags)) return FNM_NOMATCH;
    ++p;
    ++s;
  }
}

int fnmatch(const char* pattern, const char* string, int flags) {
  if (pattern == 0 || string == 0) return FNM_NOMATCH;
  return match_here(pattern, string, flags);
}
