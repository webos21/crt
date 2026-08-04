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

// Threaded through every recursive match attempt so that BRE `\( \)` / ERE
// `( )` capture groups can record their bounds (relative to the original
// subject string, per POSIX) no matter how deep the backtracking goes.
// `orig_pattern` stays fixed at the compiled pattern's start so a group's
// 1-based capture slot can be derived from its position in the pattern text
// alone (see group_open_index/group_close_index), rather than needing extra
// state carried across the recursive calls below.
struct regex_match_ctx {
  const char* orig_pattern;
  const char* subj_start;
  int flags;
  regmatch_t* caps;
  size_t ncaps;
};

// Groups nest, so naming "which capture slot does this delimiter belong to"
// requires a scan from the pattern start with an explicit open-group stack;
// there is no per-call state (recursion depth, etc.) that reduces to it.
static int group_open_index(const struct regex_match_ctx* ctx, const char* at) {
  int extended = (ctx->flags & REG_EXTENDED) != 0;
  int next_index = 1;
  const char* p = ctx->orig_pattern;

  while (p < at) {
    if (!extended && p[0] == '\\' && p[1] == '(') {
      ++next_index;
      p += 2;
    } else if (extended && p[0] == '(') {
      ++next_index;
      ++p;
    } else if (p[0] == '\\' && p[1] != 0) {
      p += 2;
    } else {
      ++p;
    }
  }
  return next_index;
}

static int group_close_index(const struct regex_match_ctx* ctx, const char* at) {
  int extended = (ctx->flags & REG_EXTENDED) != 0;
  int stack[32];
  int depth = 0;
  int next_index = 1;
  const char* p = ctx->orig_pattern;

  while (p < at) {
    if (!extended && p[0] == '\\' && p[1] == '(') {
      if (depth < 32) stack[depth] = next_index;
      ++depth;
      ++next_index;
      p += 2;
    } else if (!extended && p[0] == '\\' && p[1] == ')') {
      if (depth > 0) --depth;
      p += 2;
    } else if (extended && p[0] == '(') {
      if (depth < 32) stack[depth] = next_index;
      ++depth;
      ++next_index;
      ++p;
    } else if (extended && p[0] == ')') {
      if (depth > 0) --depth;
      ++p;
    } else if (p[0] == '\\' && p[1] != 0) {
      p += 2;
    } else {
      ++p;
    }
  }
  return depth > 0 ? stack[depth - 1] : 0;
}

static int regex_match_sequence(const char* pattern, const char* text, const char* text_end,
                                const char** match_end, struct regex_match_ctx* ctx) {
  const char* p = pattern;
  const char* s = text;
  int flags = ctx->flags;
  int extended = (flags & REG_EXTENDED) != 0;

  while (*p != 0) {
    int atom_len;
    int min_count = 1;
    int max_unbounded = 0;

    // Group delimiters don't consume input; they just record the text
    // offset (relative to the original subject) at open/close time and
    // move on. Reusing the same left-to-right, backtracking walk as the
    // rest of the pattern means a failed attempt's writes are simply
    // overwritten if/when a later attempt succeeds -- only the bindings
    // made along the ultimately-successful path are left behind. A
    // quantifier directly after a group (e.g. `\(x\)*`) is not supported:
    // the quantifier char is treated as a literal in the next iteration.
    if (!extended && p[0] == '\\' && p[1] == '(') {
      int idx = group_open_index(ctx, p);
      if (ctx->caps != 0 && (size_t)idx < ctx->ncaps) ctx->caps[idx].rm_so = (regoff_t)(s - ctx->subj_start);
      p += 2;
      continue;
    }
    if (!extended && p[0] == '\\' && p[1] == ')') {
      int idx = group_close_index(ctx, p);
      if (ctx->caps != 0 && (size_t)idx < ctx->ncaps) ctx->caps[idx].rm_eo = (regoff_t)(s - ctx->subj_start);
      p += 2;
      continue;
    }
    if (extended && p[0] == '(') {
      int idx = group_open_index(ctx, p);
      if (ctx->caps != 0 && (size_t)idx < ctx->ncaps) ctx->caps[idx].rm_so = (regoff_t)(s - ctx->subj_start);
      p += 1;
      continue;
    }
    if (extended && p[0] == ')') {
      int idx = group_close_index(ctx, p);
      if (ctx->caps != 0 && (size_t)idx < ctx->ncaps) ctx->caps[idx].rm_eo = (regoff_t)(s - ctx->subj_start);
      p += 1;
      continue;
    }

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
        if (regex_match_sequence(p + atom_len + 1, scan, text_end, &nested_end, ctx)) {
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
  // Group syntax is mode-dependent: BRE groups with `\( \)` (a bare `(` is
  // just a literal character), ERE groups with unescaped `( )`.
  for (i = 0; guts->pattern[i] != 0; ++i) {
    if (!(flags & REG_EXTENDED) && guts->pattern[i] == '\\' && guts->pattern[i + 1] == '(') {
      ++re->re_nsub;
      ++i;
    } else if (guts->pattern[i] == '\\' && guts->pattern[i + 1] != 0) {
      ++i;
    } else if ((flags & REG_EXTENDED) && guts->pattern[i] == '(') {
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
  struct regex_match_ctx ctx;
  int want_caps;

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

  want_caps = match_count > 0 && matches != 0 && !(guts->flags & REG_NOSUB);
  ctx.orig_pattern = pattern;
  ctx.subj_start = s;
  ctx.flags = guts->flags | flags;
  ctx.caps = want_caps ? matches : 0;
  ctx.ncaps = want_caps ? match_count : 0;

  if (*pattern == '^') {
    const char* end = text_start;
    if ((flags & REG_NOTBOL) == 0 && regex_match_sequence(pattern + 1, text_start, text_end, &end, &ctx)) {
      if (want_caps) {
        matches[0].rm_so = (regoff_t)(text_start - s);
        matches[0].rm_eo = (regoff_t)(end - s);
      }
      return 0;
    }
    return REG_NOMATCH;
  }

  for (cursor = text_start; cursor <= text_end; ++cursor) {
    const char* end = cursor;
    if (regex_match_sequence(pattern, cursor, text_end, &end, &ctx)) {
      if (want_caps) {
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
