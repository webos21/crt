#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

static int fail(const char* message) {
  fprintf(stderr, "wchar_mbstate_test: %s\n", message);
  return 1;
}

int main(void) {
  const char euro_utf8[] = "\xe2\x82\xac";
  const char* narrow_src;
  const wchar_t* wide_src;
  mbstate_t state = {0, 0, 0};
  wchar_t wide[16];
  wchar_t buffer[16];
  wchar_t moved[8];
  char narrow[16];
  wchar_t wc;
  size_t result;

  if (sizeof(wchar_t) != 4 || (wchar_t)-1 >= (wchar_t)0) {
    return fail("wchar_t data model");
  }
  if (MB_CUR_MAX < 4) {
    return fail("MB_CUR_MAX");
  }
  if (btowc('A') != L'A' || btowc(0x80) != WEOF ||
      wctob(L'Z') != 'Z' || wctob((wint_t)0x20ac) != EOF) {
    return fail("btowc/wctob");
  }

  if (mbrtowc(&wc, "A", 1, &state) != 1 || wc != L'A' || !mbsinit(&state)) {
    return fail("mbrtowc ascii");
  }
  result = mbrtowc(&wc, euro_utf8, 2, &state);
  if (result != (size_t)-2 || mbsinit(&state)) {
    return fail("mbrtowc partial");
  }
  result = mbrtowc(&wc, euro_utf8 + 2, 1, &state);
  if (result != 1 || wc != (wchar_t)0x20ac || !mbsinit(&state)) {
    return fail("mbrtowc complete");
  }
  errno = 0;
  if (mbrtowc(&wc, "\xff", 1, &state) != (size_t)-1 || errno != EILSEQ) {
    return fail("mbrtowc invalid");
  }
  if (mbrtowc(&wc, "", 1, &state) != 0 || wc != 0) {
    return fail("mbrtowc nul");
  }

  memset(narrow, 0, sizeof(narrow));
  if (wcrtomb(narrow, (wchar_t)0x20ac, &state) != 3 ||
      strcmp(narrow, euro_utf8) != 0 ||
      wcrtomb(narrow, L'A', &state) != 1 || narrow[0] != 'A') {
    return fail("wcrtomb");
  }

  narrow_src = "A\xe2\x82\xacZ";
  if (mbsrtowcs(wide, &narrow_src, 16, &state) != 3 || narrow_src != 0 ||
      wide[0] != L'A' || wide[1] != (wchar_t)0x20ac || wide[2] != L'Z' || wide[3] != 0) {
    return fail("mbsrtowcs");
  }
  wide_src = wide;
  memset(narrow, 0, sizeof(narrow));
  if (wcsrtombs(narrow, &wide_src, sizeof(narrow), &state) != 5 || wide_src != 0 ||
      strcmp(narrow, "A\xe2\x82\xacZ") != 0) {
    return fail("wcsrtombs");
  }
  if (mbstowcs(buffer, "abc", 16) != 3 || wcscmp(buffer, L"abc") != 0 ||
      wcstombs(narrow, L"xy", sizeof(narrow)) != 2 || strcmp(narrow, "xy") != 0) {
    return fail("mbstowcs/wcstombs");
  }
  if (mblen("A", 1) != 1 || mbtowc(&wc, euro_utf8, 3) != 3 ||
      wc != (wchar_t)0x20ac || wctomb(narrow, L'Q') != 1 || narrow[0] != 'Q') {
    return fail("legacy mb");
  }

  if (wcslen(L"abc") != 3 || wcsnlen(L"abc", 2) != 2 ||
      wcscmp(L"abc", L"abd") >= 0 || wcsncmp(L"abc", L"abd", 2) != 0) {
    return fail("wide compare");
  }
  if (wcscpy(buffer, L"ab") != buffer || wcscat(buffer, L"cd") != buffer ||
      wcscmp(buffer, L"abcd") != 0 || wcschr(buffer, L'c') != buffer + 2 ||
      wcsrchr(buffer, L'a') != buffer || wcsstr(buffer, L"bc") != buffer + 1) {
    return fail("wide string");
  }
  wcsncpy(buffer, L"xy", 5);
  if (buffer[0] != L'x' || buffer[1] != L'y' || buffer[2] != 0 || buffer[4] != 0) {
    return fail("wcsncpy");
  }
  wmemset(moved, L'0', 8);
  if (wmemcpy(moved, L"abcdef", 6) != moved ||
      wmemchr(moved, L'd', 6) != moved + 3 ||
      wmemcmp(moved, L"abcdef", 6) != 0) {
    return fail("wmemcpy");
  }
  wmemmove(moved + 2, moved, 4);
  if (wmemcmp(moved, L"ababcd", 6) != 0) {
    return fail("wmemmove");
  }

  if (!iswalpha(L'A') || !iswdigit(L'7') || !iswspace(L'\n') ||
      towlower(L'A') != L'a' || towupper(L'z') != L'Z' ||
      !iswctype(L'F', wctype("xdigit")) ||
      towctrans(L'G', wctrans("tolower")) != L'g' ||
      wctype("unknown") != 0 || wctrans("unknown") != 0) {
    return fail("wctype");
  }

  printf("wchar_mbstate_test: ok\n");
  return 0;
}
