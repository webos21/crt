#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

unsigned long long strtoull(const char* nptr, char** endptr, int base) {
  const char* s = nptr;
  unsigned long long acc = 0;
  unsigned long long cutoff;
  int c;
  int neg = 0;
  int any = 0;
  int cutlim;

  do {
    c = (unsigned char)*s++;
  } while (isspace(c));

  if (c == '-') {
    neg = 1;
    c = (unsigned char)*s++;
  } else if (c == '+') {
    c = (unsigned char)*s++;
  }

  if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
    c = (unsigned char)s[1];
    s += 2;
    base = 16;
  }

  if (base == 0) {
    base = c == '0' ? 8 : 10;
  }
  if (base < 2 || base > 36) {
    errno = EINVAL;
    if (endptr != 0) {
      *endptr = (char*)nptr;
    }
    return 0;
  }

  cutoff = ULLONG_MAX / (unsigned long long)base;
  cutlim = (int)(ULLONG_MAX % (unsigned long long)base);

  for (;; c = (unsigned char)*s++) {
    if (isdigit(c)) {
      c -= '0';
    } else if (isalpha(c)) {
      c -= isupper(c) ? 'A' - 10 : 'a' - 10;
    } else {
      break;
    }
    if (c >= base) {
      break;
    }
    if (any < 0) {
      continue;
    }
    if (acc > cutoff || (acc == cutoff && c > cutlim)) {
      any = -1;
      acc = ULLONG_MAX;
      errno = ERANGE;
    } else {
      any = 1;
      acc *= (unsigned long long)base;
      acc += (unsigned long long)c;
    }
  }

  if (neg && any > 0) {
    acc = 0ULL - acc;
  }
  if (endptr != 0) {
    *endptr = (char*)(any ? s - 1 : nptr);
  }
  return acc;
}
