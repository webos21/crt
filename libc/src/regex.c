#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#define CRT_REGEX_MAGIC 0x43525831

struct re_guts {
  char* pattern;
  int flags;
};

static int regex_fold(int ch, int flags) {
  return (flags & REG_ICASE) ? tolower((unsigned char)ch) : ch;
}

static int regex_atom_len(const char* pattern) {
  if (pattern[0] == '\\' && pattern[1] != 0) return 2;
  if (pattern[0] == '[') {
    int i = 1;
    if (pattern[i] == '!' || pattern[i] == '^') ++i;
    if (pattern[i] == ']') ++i;
    while (pattern[i] != 0 && pattern[i] != ']') {
      if (pattern[i] == '\\' && pattern[i + 1] != 0) ++i;
      ++i;
    }
    return pattern[i] == ']' ? i + 1 : -1;
  }
  return pattern[0] != 0 ? 1 : 0;
}

static int regex_match_bracket(const char* pattern, int len, int ch, int flags) {
  int i = 1;
  int negate = 0;
  int matched = 0;
  int first = 1;

  if (pattern[i] == '!' || pattern[i] == '^') {
    negate = 1;
    ++i;
  }

  ch = regex_fold(ch, flags);
  while (i < len - 1) {
    int start;
    int end;

    first = 0;
    if (pattern[i] == '\\' && pattern[i + 1] != 0) ++i;
    start = regex_fold((unsigned char)pattern[i], flags);
    end = start;
    if (i + 2 < len - 1 && pattern[i + 1] == '-') {
      i += 2;
      if (pattern[i] == '\\' && i + 1 < len - 1) ++i;
      end = regex_fold((unsigned char)pattern[i], flags);
    }
    if (start <= ch && ch <= end) matched = 1;
    ++i;
  }

  (void)first;
  return negate ? !matched : matched;
}

static int regex_match_atom(const char* pattern, int len, int ch, int flags) {
  if (ch == 0) return 0;
  if ((flags & REG_NEWLINE) && ch == '\n' && pattern[0] != '\n') return 0;
  if (pattern[0] == '.') return 1;
  if (pattern[0] == '[') return regex_match_bracket(pattern, len, ch, flags);
  if (pattern[0] == '\\' && len > 1) return regex_fold((unsigned char)pattern[1], flags) == regex_fold(ch, flags);
  return regex_fold((unsigned char)pattern[0], flags) == regex_fold(ch, flags);
}

static int regex_match_sequence(const char* pattern, const char* text, const char* text_end,
                                const char** match_end, int flags) {
  const char* p = pattern;
  const char* s = text;

  while (*p != 0) {
    int atom_len;
    int min_count = 1;
    int max_unbounded = 0;

    if (*p == '$' && p[1] == 0) {
      if (s == text_end || (!((flags & REG_NOTEOL) != 0) && s + 1 == text_end && *s == '\n')) {
        *match_end = s;
        return 1;
      }
      return 0;
    }

    atom_len = regex_atom_len(p);
    if (atom_len <= 0) return 0;

    if (p[atom_len] == '*') {
      min_count = 0;
      max_unbounded = 1;
    } else if ((flags & REG_EXTENDED) && p[atom_len] == '+') {
      max_unbounded = 1;
    } else if ((flags & REG_EXTENDED) && p[atom_len] == '?') {
      min_count = 0;
    }

    if (max_unbounded || min_count == 0) {
      const char* scan = s;
      int count = 0;

      while (scan < text_end && regex_match_atom(p, atom_len, (unsigned char)*scan, flags)) {
        ++scan;
        ++count;
      }
      if (count < min_count) return 0;
      do {
        const char* nested_end = scan;
        if (regex_match_sequence(p + atom_len + 1, scan, text_end, &nested_end, flags)) {
          *match_end = nested_end;
          return 1;
        }
      } while (scan-- > s + min_count);
      return 0;
    }

    if (s >= text_end || !regex_match_atom(p, atom_len, (unsigned char)*s, flags)) return 0;
    ++s;
    p += atom_len;
  }

  *match_end = s;
  return 1;
}

int regcomp(regex_t* re, const char* regex, int flags) {
  struct re_guts* guts;
  size_t i;

  if (re == 0 || regex == 0) return REG_INVARG;
  if ((flags & REG_PEND) && re->re_endp != 0 && re->re_endp < regex) return REG_INVARG;

  guts = (struct re_guts*)calloc(1, sizeof(*guts));
  if (guts == 0) return REG_ESPACE;
  if (flags & REG_PEND) {
    size_t len = (size_t)(re->re_endp - regex);
    guts->pattern = (char*)malloc(len + 1);
    if (guts->pattern == 0) {
      free(guts);
      return REG_ESPACE;
    }
    memcpy(guts->pattern, regex, len);
    guts->pattern[len] = 0;
  } else {
    guts->pattern = strdup(regex);
    if (guts->pattern == 0) {
      free(guts);
      return REG_ESPACE;
    }
  }
  guts->flags = flags;

  re->re_magic = CRT_REGEX_MAGIC;
  re->re_nsub = 0;
  re->re_endp = 0;
  re->re_g = guts;
  for (i = 0; guts->pattern[i] != 0; ++i) {
    if (guts->pattern[i] == '\\' && guts->pattern[i + 1] != 0) {
      ++i;
    } else if (guts->pattern[i] == '(') {
      ++re->re_nsub;
    }
  }
  return 0;
}

int regexec(const regex_t* re, const char* s, size_t match_count, regmatch_t matches[], int flags) {
  struct re_guts* guts;
  const char* pattern;
  const char* text_start;
  const char* text_end;
  const char* cursor;

  if (re == 0 || re->re_magic != CRT_REGEX_MAGIC || re->re_g == 0 || s == 0) return REG_INVARG;
  guts = re->re_g;
  pattern = guts->pattern;
  text_start = s;
  text_end = s + strlen(s);
  if ((flags & REG_STARTEND) && match_count > 0 && matches != 0) {
    if (matches[0].rm_so < 0 || matches[0].rm_eo < matches[0].rm_so) return REG_INVARG;
    text_start = s + matches[0].rm_so;
    text_end = s + matches[0].rm_eo;
  }

  if (*pattern == '^') {
    const char* end = text_start;
    if ((flags & REG_NOTBOL) == 0 && regex_match_sequence(pattern + 1, text_start, text_end, &end, guts->flags | flags)) {
      if (match_count > 0 && matches != 0 && !(guts->flags & REG_NOSUB)) {
        matches[0].rm_so = (regoff_t)(text_start - s);
        matches[0].rm_eo = (regoff_t)(end - s);
      }
      return 0;
    }
    return REG_NOMATCH;
  }

  for (cursor = text_start; cursor <= text_end; ++cursor) {
    const char* end = cursor;
    if (regex_match_sequence(pattern, cursor, text_end, &end, guts->flags | flags)) {
      if (match_count > 0 && matches != 0 && !(guts->flags & REG_NOSUB)) {
        matches[0].rm_so = (regoff_t)(cursor - s);
        matches[0].rm_eo = (regoff_t)(end - s);
      }
      return 0;
    }
    if (cursor == text_end) break;
  }
  return REG_NOMATCH;
}

size_t regerror(int error_code, const regex_t* re, char* buf, size_t n) {
  const char* message;
  size_t len;

  (void)re;
  switch (error_code) {
    case 0: message = "no error"; break;
    case REG_NOMATCH: message = "no match"; break;
    case REG_BADPAT: message = "invalid regular expression"; break;
    case REG_ESPACE: message = "out of memory"; break;
    case REG_INVARG: message = "invalid argument"; break;
    default: message = "regular expression error"; break;
  }

  len = strlen(message) + 1;
  if (buf != 0 && n != 0) {
    size_t copy = len <= n ? len : n;
    memcpy(buf, message, copy);
    if (copy == n) buf[n - 1] = 0;
  }
  return len;
}

void regfree(regex_t* re) {
  if (re == 0 || re->re_magic != CRT_REGEX_MAGIC || re->re_g == 0) return;
  free(re->re_g->pattern);
  free(re->re_g);
  re->re_magic = 0;
  re->re_nsub = 0;
  re->re_endp = 0;
  re->re_g = 0;
}
