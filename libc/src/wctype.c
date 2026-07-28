#include <ctype.h>
#include <string.h>
#include <wctype.h>

#define CRT_WCTYPE_ALNUM 1UL
#define CRT_WCTYPE_ALPHA 2UL
#define CRT_WCTYPE_BLANK 3UL
#define CRT_WCTYPE_CNTRL 4UL
#define CRT_WCTYPE_DIGIT 5UL
#define CRT_WCTYPE_GRAPH 6UL
#define CRT_WCTYPE_LOWER 7UL
#define CRT_WCTYPE_PRINT 8UL
#define CRT_WCTYPE_PUNCT 9UL
#define CRT_WCTYPE_SPACE 10UL
#define CRT_WCTYPE_UPPER 11UL
#define CRT_WCTYPE_XDIGIT 12UL

#define CRT_WCTRANS_TOLOWER 1UL
#define CRT_WCTRANS_TOUPPER 2UL

static int ascii_wint(wint_t wc) {
  return wc >= 0 && wc <= 0x7f;
}

int iswalnum(wint_t wc) {
  return ascii_wint(wc) && isalnum((int)wc);
}

int iswalpha(wint_t wc) {
  return ascii_wint(wc) && isalpha((int)wc);
}

int iswblank(wint_t wc) {
  return ascii_wint(wc) && isblank((int)wc);
}

int iswcntrl(wint_t wc) {
  return ascii_wint(wc) && iscntrl((int)wc);
}

int iswdigit(wint_t wc) {
  return ascii_wint(wc) && isdigit((int)wc);
}

int iswgraph(wint_t wc) {
  return ascii_wint(wc) && isgraph((int)wc);
}

int iswlower(wint_t wc) {
  return ascii_wint(wc) && islower((int)wc);
}

int iswprint(wint_t wc) {
  return ascii_wint(wc) && isprint((int)wc);
}

int iswpunct(wint_t wc) {
  return ascii_wint(wc) && ispunct((int)wc);
}

int iswspace(wint_t wc) {
  return ascii_wint(wc) && isspace((int)wc);
}

int iswupper(wint_t wc) {
  return ascii_wint(wc) && isupper((int)wc);
}

int iswxdigit(wint_t wc) {
  return ascii_wint(wc) && isxdigit((int)wc);
}

wint_t towlower(wint_t wc) {
  return ascii_wint(wc) ? (wint_t)tolower((int)wc) : wc;
}

wint_t towupper(wint_t wc) {
  return ascii_wint(wc) ? (wint_t)toupper((int)wc) : wc;
}

wctype_t wctype(const char* property) {
  if (property == 0) {
    return 0;
  }
  if (strcmp(property, "alnum") == 0) return CRT_WCTYPE_ALNUM;
  if (strcmp(property, "alpha") == 0) return CRT_WCTYPE_ALPHA;
  if (strcmp(property, "blank") == 0) return CRT_WCTYPE_BLANK;
  if (strcmp(property, "cntrl") == 0) return CRT_WCTYPE_CNTRL;
  if (strcmp(property, "digit") == 0) return CRT_WCTYPE_DIGIT;
  if (strcmp(property, "graph") == 0) return CRT_WCTYPE_GRAPH;
  if (strcmp(property, "lower") == 0) return CRT_WCTYPE_LOWER;
  if (strcmp(property, "print") == 0) return CRT_WCTYPE_PRINT;
  if (strcmp(property, "punct") == 0) return CRT_WCTYPE_PUNCT;
  if (strcmp(property, "space") == 0) return CRT_WCTYPE_SPACE;
  if (strcmp(property, "upper") == 0) return CRT_WCTYPE_UPPER;
  if (strcmp(property, "xdigit") == 0) return CRT_WCTYPE_XDIGIT;
  return 0;
}

int iswctype(wint_t wc, wctype_t desc) {
  switch (desc) {
    case CRT_WCTYPE_ALNUM: return iswalnum(wc);
    case CRT_WCTYPE_ALPHA: return iswalpha(wc);
    case CRT_WCTYPE_BLANK: return iswblank(wc);
    case CRT_WCTYPE_CNTRL: return iswcntrl(wc);
    case CRT_WCTYPE_DIGIT: return iswdigit(wc);
    case CRT_WCTYPE_GRAPH: return iswgraph(wc);
    case CRT_WCTYPE_LOWER: return iswlower(wc);
    case CRT_WCTYPE_PRINT: return iswprint(wc);
    case CRT_WCTYPE_PUNCT: return iswpunct(wc);
    case CRT_WCTYPE_SPACE: return iswspace(wc);
    case CRT_WCTYPE_UPPER: return iswupper(wc);
    case CRT_WCTYPE_XDIGIT: return iswxdigit(wc);
    default: return 0;
  }
}

wctrans_t wctrans(const char* property) {
  if (property == 0) {
    return 0;
  }
  if (strcmp(property, "tolower") == 0) return CRT_WCTRANS_TOLOWER;
  if (strcmp(property, "toupper") == 0) return CRT_WCTRANS_TOUPPER;
  return 0;
}

wint_t towctrans(wint_t wc, wctrans_t desc) {
  if (desc == CRT_WCTRANS_TOLOWER) {
    return towlower(wc);
  }
  if (desc == CRT_WCTRANS_TOUPPER) {
    return towupper(wc);
  }
  return wc;
}
